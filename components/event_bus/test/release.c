/*
 * Host test for automatic release on frame unwind: a handler that bails out mid-dispatch still has
 * its event's slot returned, an intermediate listener that bails skips the rest and releases once, a
 * bail deep in a nested cascade unwinds every frame it jumps, and thousands of mixed cycles leave no
 * slot behind.
 *
 * Build & run:
 *   gcc -std=c11 -Wall -Wextra -Iinclude src/pool.c src/dispatch.c test/release.c -o /tmp/reltest && /tmp/reltest
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

enum { TAG_A = 1, TAG_B = 2, TAG_OUTER = 10, TAG_INNER = 11 };

static int g_calls;

static void h_record(evt_t *e, void *ctx)  { (void) e; (void) ctx; g_calls++; }
static void h_bailout(evt_t *e, void *ctx) { (void) e; (void) ctx; g_calls++; evt_bailout(); }

static void h_now_inner(evt_t *e, void *ctx)
{
    (void) e; (void) ctx;
    evt_t inner;
    memset(&inner, 0, sizeof inner);
    inner.tag = TAG_INNER;
    inner.slot_class = EVT_CLASS_TINY;
    evt_now(&inner);      /* the inner handler bails out; control never returns here */
    g_calls += 100;       /* must not run */
}

static void test_bailout_releases_slot(void)
{
    evt_bus_reset();
    g_calls = 0;
    CHECK(evt_listen(TAG_A, h_bailout, NULL));

    evt_t *e = evt_pool_acquire(EVT_CLASS_TINY, TAG_A);
    CHECK(evt_dispatch(e));
    CHECK(evt_drain() == 1);

    CHECK(g_calls == 1);
    CHECK(evt_pool_in_use(EVT_CLASS_TINY) == 0);   /* released on unwind, not leaked */
    CHECK(evt_frame_depth() == 0);
}

static void test_intermediate_listener_bails(void)
{
    evt_bus_reset();
    g_calls = 0;
    CHECK(evt_listen(TAG_A, h_bailout, NULL));   /* first: bails */
    CHECK(evt_listen(TAG_A, h_record, NULL));    /* must not run */

    evt_t *e = evt_pool_acquire(EVT_CLASS_SMALL, TAG_A);
    CHECK(evt_dispatch(e));
    evt_drain();

    CHECK(g_calls == 1);
    CHECK(evt_pool_in_use(EVT_CLASS_SMALL) == 0);   /* released exactly once */
    CHECK(evt_frame_depth() == 0);
}

static void test_cascade_cross_frame_unwind(void)
{
    evt_bus_reset();
    g_calls = 0;
    CHECK(evt_listen(TAG_OUTER, h_now_inner, NULL));
    CHECK(evt_listen(TAG_INNER, h_bailout, NULL));

    evt_t *e = evt_pool_acquire(EVT_CLASS_TINY, TAG_OUTER);
    CHECK(evt_dispatch(e));
    evt_drain();

    CHECK(g_calls == 1);                            /* only the inner handler ran, then bailed */
    CHECK(evt_frame_depth() == 0);                  /* both frames unwound */
    CHECK(evt_pool_in_use(EVT_CLASS_TINY) == 0);    /* the outer event's slot came back */
}

static void test_now_standalone_bailout(void)
{
    evt_bus_reset();
    g_calls = 0;
    CHECK(evt_listen(TAG_A, h_bailout, NULL));

    evt_t inner;
    memset(&inner, 0, sizeof inner);
    inner.tag = TAG_A;
    inner.slot_class = EVT_CLASS_TINY;
    evt_now(&inner);   /* handler bails; now() catches and unwinds */

    CHECK(g_calls == 1);
    CHECK(evt_frame_depth() == 0);
}

static void test_no_leak_over_cycles(void)
{
    evt_bus_reset();
    CHECK(evt_listen(TAG_A, h_bailout, NULL));   /* bails every time */
    CHECK(evt_listen(TAG_B, h_record, NULL));    /* normal every time */

    for (int i = 0; i < 1000; i++) {
        evt_t *a = evt_pool_acquire(EVT_CLASS_TINY, TAG_A);
        evt_t *b = evt_pool_acquire(EVT_CLASS_SMALL, TAG_B);
        CHECK(a && b);
        CHECK(evt_dispatch(a));
        CHECK(evt_dispatch(b));
        evt_drain();
    }

    CHECK(evt_pool_in_use_by_tag(TAG_A) == 0);
    CHECK(evt_pool_in_use_by_tag(TAG_B) == 0);
    CHECK(evt_pool_in_use(EVT_CLASS_TINY) == 0);
    CHECK(evt_pool_in_use(EVT_CLASS_SMALL) == 0);
    CHECK(evt_frame_depth() == 0);
}

int main(void)
{
    printf(">>> event_bus release gate (host)\n");
    CHECK(evt_pool_init());

    test_bailout_releases_slot();
    test_intermediate_listener_bails();
    test_cascade_cross_frame_unwind();
    test_now_standalone_bailout();
    test_no_leak_over_cycles();

    evt_pool_teardown();

    printf("=== %d checks, %d failures ===\n", g_checks, g_failures);
    if (g_failures == 0) {
        printf(">>> RELEASE GATE OK\n");
        return 0;
    }
    return 1;
}
