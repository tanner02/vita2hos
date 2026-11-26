#include <stdint.h>
#include "load_layout.h"

int load_layout_place(uintptr_t base, uint32_t used, uint32_t size, uint32_t alignment,
                       uint32_t capacity, uint32_t *offset, uint32_t *end)
{
    if (!offset || !end || (alignment && (alignment & (alignment - 1))))
        return -1;
    if (alignment == 0)
        alignment = 1;
    if (used > capacity || base > UINTPTR_MAX - used)
        return -1;
    uintptr_t address = base + used;
    uint32_t padding = (uint32_t)(-(uintptr_t)address & (alignment - 1));
    if (padding > capacity - used)
        return -1;
    uint32_t start = used + padding;
    if (size > capacity - start || base > UINTPTR_MAX - start ||
        size > UINTPTR_MAX - (base + start))
        return -1;
    *offset = start;
    *end = start + size;
    return 0;
}
