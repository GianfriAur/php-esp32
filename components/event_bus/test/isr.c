/*
 * Host test for the ISR pool: acquire from the reserve sets FROM_ISR, exhaustion drops and counts
 * (never NULL-crashes), the reserve is independent of the main pool, and a FROM_ISR event released
 * after a dispatch or a bailout returns to the ISR reserve (not the main pool).
 *
 * Build & run:
 *   gcc -std=c11 -Wall -Wextra -Iinclude src/pool.c src/dispatch.c src/isr_pool.c test/isr.c -o /tmp/isrtest && /tmp/isrtest
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

enum { TAG_BTN = 1 };

static int g_calls;
static evt_t *g_seen;

static void h_record(evt_t *e, void *ctx)  { (void) ctx; g_calls++; g_seen = e; }
static void h_bailout(evt_t *e, void *ctx) { (void) e; (void) ctx; g_calls++; evt_bailout(); }

static void fresh(void)
{
    evt_pool_teardown();
    CHECK(evt_pool_init());
    evt_bus_reset();
}

static void test_isr_flag_and_shape(void)
{
    fresh();
    evt_t *e = evt_isr_acquire(TAG_BTN);
    CHECK(e != NULL);
    CHECK(e->tag == TAG_BTN);
    CHECK((e->flags & EVT_FLAG_FROM_ISR) != 0);
    CHECK((e->flags & EVT_FLAG_OWNED) != 0);
    CHECK(evt_isr_in_use() == 1);

    evt_pool_release(e);                 /* routes to the ISR reserve via FROM_ISR */
    CHECK(evt_isr_in_use() == 0);
}

static void test_isr_exhaustion_drops(void)
{
    fresh();
    uint16_t cap = evt_isr_capacity();
    CHECK(cap > 0);

    evt_t *slots[64];
    CHECK(cap <= 64);
    for (uint16_t i = 0; i < cap; i++) {
        slots[i] = evt_isr_acquire(TAG_BTN);
        CHECK(slots[i] != NULL);
    }
    CHECK(evt_isr_in_use() == cap);
    CHECK(evt_isr_dropped() == 0);

    CHECK(evt_isr_acquire(TAG_BTN) == NULL);   /* empty reserve: drop, not crash */
    CHECK(evt_isr_dropped() == 1);
    CHECK(evt_isr_acquire(TAG_BTN) == NULL);
    CHECK(evt_isr_dropped() == 2);

    for (uint16_t i = 0; i < cap; i++) {
        evt_isr_release(slots[i]);
    }
    CHECK(evt_isr_in_use() == 0);
}

static void test_isr_separate_from_main_pool(void)
{
    fresh();
    /* Exhaust the ISR reserve; the main TINY pool must be untouched. */
    uint16_t cap = evt_isr_capacity();
    for (uint16_t i = 0; i < cap; i++) {
        CHECK(evt_isr_acquire(TAG_BTN) != NULL);
    }
    CHECK(evt_isr_acquire(TAG_BTN) == NULL);
    CHECK(evt_pool_in_use(EVT_CLASS_TINY) == 0);
    CHECK(evt_pool_acquire(EVT_CLASS_TINY, TAG_BTN) != NULL);   /* main pool still serves */
}

static void test_isr_dispatch_releases_to_reserve(void)
{
    fresh();
    g_calls = 0; g_seen = NULL;
    CHECK(evt_listen(TAG_BTN, h_record, NULL));

    evt_t *e = evt_isr_acquire(TAG_BTN);
    CHECK(e != NULL);
    CHECK(evt_dispatch(e));
    CHECK(evt_isr_in_use() == 1);        /* held by the queue */
    CHECK(evt_drain() == 1);

    CHECK(g_calls == 1);
    CHECK(g_seen == e);
    CHECK(evt_isr_in_use() == 0);                     /* returned to the ISR reserve */
    CHECK(evt_pool_in_use(EVT_CLASS_TINY) == 0);      /* not to the main pool */
}

static void test_isr_bailout_releases_to_reserve(void)
{
    fresh();
    g_calls = 0;
    CHECK(evt_listen(TAG_BTN, h_bailout, NULL));

    evt_t *e = evt_isr_acquire(TAG_BTN);
    CHECK(evt_dispatch(e));
    evt_drain();

    CHECK(g_calls == 1);
    CHECK(evt_isr_in_use() == 0);        /* unwind returned the ISR slot to its reserve */
    CHECK(evt_frame_depth() == 0);
}

int main(void)
{
    printf(">>> event_bus ISR pool gate (host)\n");

    test_isr_flag_and_shape();
    test_isr_exhaustion_drops();
    test_isr_separate_from_main_pool();
    test_isr_dispatch_releases_to_reserve();
    test_isr_bailout_releases_to_reserve();

    evt_pool_teardown();

    printf("=== %d checks, %d failures ===\n", g_checks, g_failures);
    if (g_failures == 0) {
        printf(">>> ISR GATE OK\n");
        return 0;
    }
    return 1;
}
