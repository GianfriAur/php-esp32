/*
 * dispatch()/now(), the listener registry, and the dispatch-frame stack.
 *
 */
#include "event_bus.h"

#include <string.h>
#include <setjmp.h>

typedef struct {
    uint16_t       tag;
    evt_handler_fn fn;
    void          *ctx;
    bool           used;
} listener_t;

static listener_t s_listeners[EVT_MAX_LISTENERS];

static evt_t *s_queue[EVT_QUEUE_DEPTH];
static int    s_q_head;
static int    s_q_count;

typedef struct {
    evt_t *event;   /* the slot this frame owns; NULL for now()/logical events */
} frame_t;

static frame_t s_frames[EVT_FRAME_DEPTH_MAX];
static int     s_frame_top;

static jmp_buf s_jb;
static bool    s_protected;   /* inside a drain/now setjmp region */

bool evt_listen(uint16_t tag, evt_handler_fn fn, void *ctx)
{
    if (!fn) {
        return false;
    }
    for (int i = 0; i < EVT_MAX_LISTENERS; i++) {
        if (!s_listeners[i].used) {
            s_listeners[i].tag  = tag;
            s_listeners[i].fn   = fn;
            s_listeners[i].ctx  = ctx;
            s_listeners[i].used = true;
            return true;
        }
    }
    return false;
}

bool evt_dispatch(evt_t *e)
{
    if (!e || s_q_count == EVT_QUEUE_DEPTH) {
        return false;
    }
    int tail = (s_q_head + s_q_count) % EVT_QUEUE_DEPTH;
    s_queue[tail] = e;
    s_q_count++;
    return true;
}

static void run_handlers(evt_t *e)
{
    for (int i = 0; i < EVT_MAX_LISTENERS; i++) {
        if (s_listeners[i].used && s_listeners[i].tag == e->tag) {
            s_listeners[i].fn(e, s_listeners[i].ctx);
        }
    }
}

void evt_frames_unwind_to(int base)
{
    if (base < 0) {
        base = 0;
    }
    while (s_frame_top > base) {
        s_frame_top--;
        evt_t *ev = s_frames[s_frame_top].event;
        if (ev) {
            evt_pool_release(ev);
        }
    }
}

static void frame_push(evt_t *owned)
{
    if (s_frame_top >= EVT_FRAME_DEPTH_MAX) {
        return;   /* depth policy comes with the cascade counters */
    }
    s_frames[s_frame_top].event = owned;
    s_frame_top++;
}

static void frame_pop(void)
{
    if (s_frame_top > 0) {
        evt_frames_unwind_to(s_frame_top - 1);
    }
}

int evt_drain(void)
{
    volatile int ran = 0;

    while (s_q_count > 0) {
        evt_t *e = s_queue[s_q_head];
        s_q_head = (s_q_head + 1) % EVT_QUEUE_DEPTH;
        s_q_count--;

        volatile int base = s_frame_top;
        if (setjmp(s_jb) == 0) {
            s_protected = true;
            frame_push(e);
            run_handlers(e);
            frame_pop();
        } else {
            evt_frames_unwind_to(base);
        }
        s_protected = false;
        ran++;
    }

    return ran;
}

void evt_now(evt_t *e)
{
    if (!e) {
        return;
    }

    if (s_protected) {
        /* Nested: rely on the outer region's setjmp so a bail unwinds to the outermost. */
        frame_push(NULL);
        run_handlers(e);
        frame_pop();
        return;
    }

    volatile int base = s_frame_top;
    if (setjmp(s_jb) == 0) {
        s_protected = true;
        frame_push(NULL);
        run_handlers(e);
        frame_pop();
    } else {
        evt_frames_unwind_to(base);
    }
    s_protected = false;
}

void evt_bailout(void)
{
    if (s_protected) {
        longjmp(s_jb, 1);
    }
}

void evt_bus_reset(void)
{
    memset(s_listeners, 0, sizeof s_listeners);
    memset(s_queue, 0, sizeof s_queue);
    s_q_head = 0;
    s_q_count = 0;
    memset(s_frames, 0, sizeof s_frames);
    s_frame_top = 0;
    s_protected = false;
}

int evt_frame_depth(void)
{
    return s_frame_top;
}
