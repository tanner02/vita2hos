#ifndef LOAD_FIXED_H
#define LOAD_FIXED_H
#include <switch.h>

#include "load_segment.h"

typedef struct {
    void *backing;
    void *writable;
    uintptr_t address;
    size_t size;
    Handle handle;
    VirtmemReservation *reservation;
    bool executable, writable_mapped, executable_mapped;
} LoadFixedMemory;

/* Page-aligned, disjoint fixed segments; rejects unsupported writable code.
 * Validates the complete image before any mappings are changed. */
int load_fixed_layout(const segment_info_t *segments, int count);
Result load_fixed_reserve(LoadFixedMemory *memory, uintptr_t address, size_t size, bool executable);
Result load_fixed_map(LoadFixedMemory *memory);
Result load_fixed_close(LoadFixedMemory *memory);
void load_fixed_flush(const LoadFixedMemory *memory);
#endif
