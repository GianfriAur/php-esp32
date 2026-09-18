/*
 * Host test for the slot pool: acquire/release, exhaustion returns NULL, no leaks (the per-tag
 * oracle returns to zero), classes independent, release guards. Grows as the event bus does.
 *
 * Build & run:
 *   gcc -std=c11 -Wall -Wextra -Iinclude src/pool.c test/pool_exhaustion.c -o /tmp/pooltest && /tmp/pooltest
 */
#include "event_bus.h"

#include <stdio.h>
#include <stdlib.h>

static int g_checks;
static int g_failures;

#define CHECK(cond) do {                                                \
        g_checks++;                                                     \
        if (!(cond)) {                                                  \
            g_failures++;                                               \
            printf("  FAIL  %s:%d  %s\n", __FILE__, __LINE__, #cond);  \
        }                                                              \
    } while (0)

/* Fill a class to capacity, then prove one more acquire fails. */
static void test_exhaustion(evt_class_t cls, uint16_t tag)
{
    uint16_t cap = evt_pool_capacity(cls);
    CHECK(cap > 0);

    evt_t *slots[256];
    CHECK(cap <= 256);

    for (uint16_t i = 0; i < cap; i++) {
        slots[i] = evt_pool_acquire(cls, tag);
        CHECK(slots[i] != NULL);
    }
    CHECK(evt_pool_in_use(cls) == cap);
    CHECK(evt_pool_peak(cls) == cap);
    CHECK(evt_pool_in_use_by_tag(tag) == cap);

    CHECK(evt_pool_acquire(cls, tag) == NULL);

    evt_pool_release(slots[0]);
    CHECK(evt_pool_in_use(cls) == (uint16_t) (cap - 1));
    slots[0] = evt_pool_acquire(cls, tag);
    CHECK(slots[0] != NULL);
    CHECK(evt_pool_acquire(cls, tag) == NULL);

    for (uint16_t i = 0; i < cap; i++) {
        evt_pool_release(slots[i]);
    }
    CHECK(evt_pool_in_use(cls) == 0);
    CHECK(evt_pool_in_use_by_tag(tag) == 0);
    CHECK(evt_pool_peak(cls) == cap);   /* high-water mark: does not fall */
}

/* An acquired slot has an initialised header and a writable class-sized buffer. */
static void test_slot_shape(void)
{
    evt_t *e = evt_pool_acquire(EVT_CLASS_SMALL, 42);
    CHECK(e != NULL);
    CHECK(e->tag == 42);
    CHECK(e->source == EVT_SRC_NONE);
    CHECK(e->flags == EVT_FLAG_OWNED);
    CHECK(e->depth == 0);
    CHECK(e->payload_len == 0);
    CHECK(e->slot_class == EVT_CLASS_SMALL);
    CHECK(e->payload != NULL);

    unsigned char *buf = (unsigned char *) e->payload;
    for (int i = 0; i < EVT_SMALL_SIZE; i++) {
        buf[i] = (unsigned char) i;
    }
    e->payload_len = EVT_SMALL_SIZE;
    CHECK(buf[EVT_SMALL_SIZE - 1] == (unsigned char) (EVT_SMALL_SIZE - 1));

    evt_pool_release(e);
    CHECK(evt_pool_in_use(EVT_CLASS_SMALL) == 0);
}

/* Exhausting one class leaves the others full. */
static void test_classes_independent(void)
{
    uint16_t tiny_cap = evt_pool_capacity(EVT_CLASS_TINY);
    for (uint16_t i = 0; i < tiny_cap; i++) {
        CHECK(evt_pool_acquire(EVT_CLASS_TINY, 1) != NULL);
    }
    CHECK(evt_pool_acquire(EVT_CLASS_TINY, 1) == NULL);
    CHECK(evt_pool_in_use(EVT_CLASS_SMALL) == 0);
    CHECK(evt_pool_acquire(EVT_CLASS_SMALL, 1) != NULL);

    evt_pool_teardown();
    CHECK(evt_pool_init());
}

/* release tolerates NULL and a double release. */
static void test_release_guards(void)
{
    evt_pool_release(NULL);

    evt_t *e = evt_pool_acquire(EVT_CLASS_TINY, 7);
    CHECK(e != NULL);
    evt_pool_release(e);
    uint16_t after = evt_pool_in_use(EVT_CLASS_TINY);
    evt_pool_release(e);
    CHECK(evt_pool_in_use(EVT_CLASS_TINY) == after);
    CHECK(after == 0);
}

int main(void)
{
    printf(">>> event_bus pool gate (host)\n");

    CHECK(evt_pool_init());
    CHECK(evt_pool_init());   /* idempotent */

    test_slot_shape();
    test_exhaustion(EVT_CLASS_TINY,   100);
    test_exhaustion(EVT_CLASS_SMALL,  200);
    test_exhaustion(EVT_CLASS_MEDIUM, 300);
    test_release_guards();
    test_classes_independent();

    for (int c = 0; c < EVT_CLASS_COUNT; c++) {
        CHECK(evt_pool_in_use((evt_class_t) c) == 0);
    }

    evt_pool_teardown();

    printf("=== %d checks, %d failures ===\n", g_checks, g_failures);
    if (g_failures == 0) {
        printf(">>> POOL GATE OK\n");
        return 0;
    }
    return 1;
}
