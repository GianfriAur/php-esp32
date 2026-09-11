#pragma once

/*
 * Event bus C core: the event type and the slot pool. Every event is a slot from a preallocated
 * pool; acquire takes an index from a free-list and returns NULL when the class is exhausted.
 */
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

enum {
    EVT_FLAG_OWNED    = 1u << 0,   /* the event owns its slot */
    EVT_FLAG_FROM_ISR = 1u << 1,   /* emitted from an ISR -> the ISR pool */
    EVT_FLAG_COALESCE = 1u << 2,   /* latest-wins */
};

/* No device origin: timer tick, GPIO edge, system event. */
#define EVT_SRC_NONE   ((uint16_t) 0xFFFF)

typedef struct {
    uint16_t tag;          /* event type; index into the listener table */
    uint16_t source;       /* emitting device (registry index), or EVT_SRC_NONE */
    uint16_t flags;        /* EVT_FLAG_* */
    uint8_t  slot_class;   /* evt_class_t */
    uint8_t  depth;        /* cascade depth */
    uint16_t payload_len;
    void    *payload;      /* the slot buffer, slot-class capacity */
} evt_t;

/* Global fixed size classes. The per-device LARGE sub-pool arrives with the driver framework. */
typedef enum {
    EVT_CLASS_TINY = 0,
    EVT_CLASS_SMALL,
    EVT_CLASS_MEDIUM,
    EVT_CLASS_COUNT,
} evt_class_t;

/* Suggested defaults, overridable at build time. */
#ifndef EVT_TINY_SIZE
#define EVT_TINY_SIZE     32
#endif
#ifndef EVT_TINY_COUNT
#define EVT_TINY_COUNT    32
#endif
#ifndef EVT_SMALL_SIZE
#define EVT_SMALL_SIZE    256
#endif
#ifndef EVT_SMALL_COUNT
#define EVT_SMALL_COUNT   16
#endif
#ifndef EVT_MEDIUM_SIZE
#define EVT_MEDIUM_SIZE   2048
#endif
#ifndef EVT_MEDIUM_COUNT
#define EVT_MEDIUM_COUNT  8
#endif

/* Preallocate the class pools. Idempotent; false if an allocation failed. */
bool evt_pool_init(void);

void evt_pool_teardown(void);

/* Take a slot from `cls`, header initialised (tag set, source EVT_SRC_NONE, flags OWNED, depth 0,
 * payload_len 0). NULL when the class is exhausted. */
evt_t *evt_pool_acquire(evt_class_t cls, uint16_t tag);

/* Return a slot. NULL-safe; ignores a double release or a foreign pointer. */
void evt_pool_release(evt_t *e);

uint16_t evt_pool_in_use(evt_class_t cls);
uint16_t evt_pool_peak(evt_class_t cls);
uint16_t evt_pool_capacity(evt_class_t cls);
uint32_t evt_pool_in_use_by_tag(uint16_t tag);   /* the leak oracle */
