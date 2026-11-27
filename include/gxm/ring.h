#ifndef GXM_RING_H
#define GXM_RING_H
#include <stdint.h>
#include <stdbool.h>
/* Checked guest suballocation and relative reservation; failures preserve outputs. */
static inline bool gxm_ring_span(uintptr_t base, uint32_t capacity, uintptr_t address,
                                 uint32_t size, uint32_t *offset)
{
    if (!size || address < base || address-base > capacity || size > capacity-(address-base))
        return false;
    *offset = (uint32_t)(address-base);
    return true;
}
static inline bool gxm_ring_reserve(uint32_t capacity, uint32_t size,
                                    uint32_t *head, uint32_t *offset)
{
    if (!size || size > capacity || *head > capacity) return false;
    uint32_t start = size > capacity-*head ? 0 : *head;
    *offset = start;
    *head = start + size;
    return true;
}
#endif
