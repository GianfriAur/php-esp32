/*
 * Host test for the two cascade counters: a dispatch cascade is capped and the over-limit event is
 * dropped (counted), a nested now() cascade is capped and the over-limit call refused (counted), and
 * a plain dispatch drops nothing.
 *
 * Build & run:
 *   gcc -std=c11 -Wall -Wextra -Iinclude src/pool.c src/dispatch.c test/cascade.c -o /tmp/casctest && /tmp/casctest
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

enum { TAG_A = 1, TAG_CHAIN = 2, TAG_NOW = 3 };

static int g_calls;

static void h_record(evt_t *e, void *ctx) { (void) e; (void) ctx; g_calls++; }

/* Re-dispatches its own tag, so the cascade grows until the depth cap drops the next hop. */
static void h_chain(evt_t *e, void *ctx)
{
    (void) e; (void) ctx;
    g_calls++;
    evt_t *n = evt_pool_acquire(EVT_CLASS_TINY, TAG_CHAIN);
    if (n && !evt_dispatch(n)) {
        evt_pool_release(n);   /* dropped at the cap: give the slot back */
    }
}

/* now()s its own tag, so nested frames grow until the now cap refuses the next call. */
static void h_now_recur(evt_t *e, void *ctx)
{
    (void) e; (void) ctx;
    g_calls++;
    evt_t inner;
    memset(&inner, 0, sizeof inner);
    inner.tag = TAG_NOW;
    inner.slot_class = EVT_CLASS_TINY;
    evt_now(&inner);
}

static void test_dispatch_depth_cap(void)
{
    evt_bus_reset();
    g_calls = 0;
    CHECK(evt_listen(TAG_CHAIN, h_chain, NULL));

    evt_t *e = evt_pool_acquire(EVT_CLASS_TINY, TAG_CHAIN);   /* depth 0 */
    CHECK(evt_dispatch(e));
    evt_drain();

    /* depth 0 plus EVT_DISPATCH_DEPTH_MAX hops all run; the next hop is dropped. */
    CHECK(g_calls == EVT_DISPATCH_DEPTH_MAX + 1);
    CHECK(evt_dispatch_dropped() == 1);
    CHECK(evt_pool_in_use(EVT_CLASS_TINY) == 0);
}

static void test_now_depth_cap(void)
{
    evt_bus_reset();
    g_calls = 0;
    CHECK(evt_listen(TAG_NOW, h_now_recur, NULL));

    evt_t first;
    memset(&first, 0, sizeof first);
    first.tag = TAG_NOW;
    first.slot_class = EVT_CLASS_TINY;
    evt_now(&first);

    CHECK(g_calls == EVT_NOW_DEPTH_MAX);   /* the call that would exceed is refused, not run */
    CHECK(evt_now_rejected() == 1);
    CHECK(evt_now_depth() == 0);
    CHECK(evt_frame_depth() == 0);
}

static void test_plain_dispatch_drops_nothing(void)
{
    evt_bus_reset();
    g_calls = 0;
    CHECK(evt_listen(TAG_A, h_record, NULL));

    evt_t *e = evt_pool_acquire(EVT_CLASS_TINY, TAG_A);
    CHECK(evt_dispatch(e));
    CHECK(evt_drain() == 1);

    CHECK(g_calls == 1);
    CHECK(evt_dispatch_dropped() == 0);
    CHECK(evt_pool_in_use(EVT_CLASS_TINY) == 0);
}

int main(void)
{
    printf(">>> event_bus cascade gate (host)\n");
    CHECK(evt_pool_init());

    test_dispatch_depth_cap();
    test_now_depth_cap();
    test_plain_dispatch_drops_nothing();

    evt_pool_teardown();

    printf("=== %d checks, %d failures ===\n", g_checks, g_failures);
    if (g_failures == 0) {
        printf(">>> CASCADE GATE OK\n");
        return 0;
    }
    return 1;
}
