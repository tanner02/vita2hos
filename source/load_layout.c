#include <stdint.h>
#include "load_layout.h"

void *load_address_translate(const LoadAddressRange *ranges, size_t count,
                              uintptr_t address, size_t size)
{
    if (!ranges || address == 0)
        return NULL;
    for (size_t i = 0; i < count; i++) {
        if (address < ranges[i].runtime)
            continue;
        uintptr_t offset = address - ranges[i].runtime;
        if (offset > ranges[i].size || size > ranges[i].size - offset ||
            ranges[i].writable > UINTPTR_MAX - offset)
            continue;
        uintptr_t writable = ranges[i].writable + offset;
        if (size > UINTPTR_MAX - writable)
            return NULL;
        return (void *)writable;
    }
    return NULL;
}

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
