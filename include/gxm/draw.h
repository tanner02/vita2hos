#ifndef GXM_DRAW_H
#define GXM_DRAW_H

#include <stddef.h>
#include <stdint.h>
#include <psp2/gxm.h>

int gxm_validate_index_range(SceGxmIndexFormat format, uintptr_t address,
                             uintptr_t block_base, size_t block_size,
                             unsigned int count, uint32_t *offset);

#endif
