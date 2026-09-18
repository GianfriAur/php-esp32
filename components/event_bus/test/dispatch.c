/*
 * Host test for dispatch()/now(), the listener registry, and the frame stack: handlers fire in order,
 * a queued event's slot is released after its last handler, now() leaves ownership with the caller,
 * a full queue refuses, and a handler can emit a cascading event.
 *
 * Build & run:
 *   gcc -std=c11 -Wall -Wextra -Iinclude src/pool.c src/dispatch.c test/dispatch.c -o /tmp/disptest && /tmp/disptest
 */
#include "event_bus.h"

#include <stdio.h>
#include <string.h>

static int g_checks;
static int g_failures;

#define CHECK(cond) do {                                                \
        g_checks++;                                                     \
        if (!(cond)) {                                                  \
            g_failures++;                                               \
            printf("  FAIL  %s:%d  %s\n", __FILE__, __LINE__, #cond);  \
        }                                                              \
    } while (0)

enum { TAG_A = 1, TAG_B = 2, TAG_CASCADE = 3 };

static int  g_calls;
static int  g_order[8];
static int  g_order_n;
static evt_t *g_seen;
static int  g_depth_in_handler;

static void reset_probe(void)
{
    g_calls = 0;
    g_order_n = 0;
    g_seen = NULL;
    g_depth_in_handler = 0;
}

static void handler_record(evt_t *e, void *ctx)
{
    g_calls++;
    g_seen = e;
    g_depth_in_handler = evt_frame_depth();
    if (ctx && g_order_n < 8) {
        g_order[g_order_n++] = *(int *) ctx;
    }
}

/* Emits a now() event from inside a dispatch, to observe nested frames. */
static void handler_now_nested(evt_t *e, void *ctx)
{
    (void) e; (void) ctx;
    evt_t inner;                    /* a stack event: now() takes no slot */
    memset(&inner, 0, sizeof inner);
    inner.tag = TAG_B;
    inner.slot_class = EVT_CLASS_TINY;
    evt_now(&inner);
}

/* Dispatches a follow-up event (the button -> ... cascade). */
static void handler_cascade(evt_t *e, void *ctx)
{
    (void) e; (void) ctx;
    evt_t *next = evt_pool_acquire(EVT_CLASS_TINY, TAG_A);
    if (next) {
        evt_dispatch(next);
    }
}

static void test_dispatch_calls_handler_and_releases(void)
{
    evt_bus_reset();
    reset_probe();
    CHECK(evt_listen(TAG_A, handler_record, NULL));

    evt_t *e = evt_pool_acquire(EVT_CLASS_TINY, TAG_A);
    CHECK(e != NULL);
    CHECK(evt_dispatch(e));
    CHECK(g_calls == 0);                         /* dispatch is async: not run yet */
    CHECK(evt_pool_in_use(EVT_CLASS_TINY) == 1); /* slot still held by the queue */

    CHECK(evt_drain() == 1);
    CHECK(g_calls == 1);
    CHECK(g_seen == e);
    CHECK(g_depth_in_handler == 1);              /* inside one dispatch frame */
    CHECK(evt_pool_in_use(EVT_CLASS_TINY) == 0); /* released after the handler */
    CHECK(evt_frame_depth() == 0);
}

static void test_multiple_listeners_ordered_release_once(void)
{
    evt_bus_reset();
    reset_probe();
    static int id1 = 1, id2 = 2;
    CHECK(evt_listen(TAG_A, handler_record, &id1));
    CHECK(evt_listen(TAG_A, handler_record, &id2));

    evt_t *e = evt_pool_acquire(EVT_CLASS_SMALL, TAG_A);
    CHECK(evt_dispatch(e));
    CHECK(evt_drain() == 1);

    CHECK(g_calls == 2);
    CHECK(g_order_n == 2 && g_order[0] == 1 && g_order[1] == 2);   /* registration order */
    CHECK(evt_pool_in_use(EVT_CLASS_SMALL) == 0);                  /* released once, after the last */
}

static void test_now_keeps_ownership(void)
{
    evt_bus_reset();
    reset_probe();
    CHECK(evt_listen(TAG_A, handler_record, NULL));

    evt_t *e = evt_pool_acquire(EVT_CLASS_TINY, TAG_A);
    CHECK(e != NULL);
    evt_now(e);
    CHECK(g_calls == 1);
    CHECK(g_seen == e);
    CHECK(evt_pool_in_use(EVT_CLASS_TINY) == 1);   /* now() did not release: caller still owns it */

    evt_pool_release(e);
    CHECK(evt_pool_in_use(EVT_CLASS_TINY) == 0);
}

static void test_dispatch_guards(void)
{
    evt_bus_reset();
    CHECK(evt_dispatch(NULL) == false);

    /* The queue is sized >= the total slot count, so the pool runs out before the queue: every
     * acquired slot dispatches, and drain returns them all. (The queue-full branch is exercised in
     * the exhaustion gate with a reduced queue depth.) */
    int n = 0;
    evt_t *e;
    while ((e = evt_pool_acquire(EVT_CLASS_TINY, TAG_A)) != NULL) {
        CHECK(evt_dispatch(e));
        n++;
    }
    CHECK(n == evt_pool_capacity(EVT_CLASS_TINY));
    CHECK(evt_drain() == n);
    CHECK(evt_pool_in_use(EVT_CLASS_TINY) == 0);
}

static void test_now_nested_frames(void)
{
    evt_bus_reset();
    reset_probe();
    CHECK(evt_listen(TAG_A, handler_now_nested, NULL));
    CHECK(evt_listen(TAG_B, handler_record, NULL));

    evt_t *e = evt_pool_acquire(EVT_CLASS_TINY, TAG_A);
    CHECK(evt_dispatch(e));
    CHECK(evt_drain() == 1);

    CHECK(g_calls == 1);                 /* TAG_B handler ran via the nested now() */
    CHECK(g_depth_in_handler == 2);      /* dispatch frame + now frame */
    CHECK(evt_frame_depth() == 0);       /* all unwound */
    CHECK(evt_pool_in_use(EVT_CLASS_TINY) == 0);
}

static void test_cascade_dispatch(void)
{
    evt_bus_reset();
    reset_probe();
    CHECK(evt_listen(TAG_CASCADE, handler_cascade, NULL));
    CHECK(evt_listen(TAG_A, handler_record, NULL));

    evt_t *e = evt_pool_acquire(EVT_CLASS_TINY, TAG_CASCADE);
    CHECK(evt_dispatch(e));

    /* One drain runs the cascade handler, which queues a TAG_A event the same loop then runs. */
    CHECK(evt_drain() == 2);
    CHECK(g_calls == 1);                 /* the TAG_A handler ran once */
    CHECK(evt_pool_in_use(EVT_CLASS_TINY) == 0);
}

int main(void)
{
    printf(">>> event_bus dispatch gate (host)\n");
    CHECK(evt_pool_init());

    test_dispatch_calls_handler_and_releases();
    test_multiple_listeners_ordered_release_once();
    test_now_keeps_ownership();
    test_dispatch_guards();
    test_now_nested_frames();
    test_cascade_dispatch();

    evt_pool_teardown();

    printf("=== %d checks, %d failures ===\n", g_checks, g_failures);
    if (g_failures == 0) {
        printf(">>> DISPATCH GATE OK\n");
        return 0;
    }
    return 1;
}
