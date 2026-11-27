#include "gxm/draw.h"

int gxm_validate_index_range(SceGxmIndexFormat format, uintptr_t address,
                             uintptr_t block_base, size_t block_size,
                             unsigned int count, uint32_t *offset)
{
    size_t element_size;
    switch (format) {
    case SCE_GXM_INDEX_FORMAT_U16: element_size = 2; break;
    case SCE_GXM_INDEX_FORMAT_U32: element_size = 4; break;
    default: return SCE_GXM_ERROR_INVALID_VALUE;
    }
    if (!address || !offset) return SCE_GXM_ERROR_INVALID_POINTER;
    if (address % element_size) return SCE_GXM_ERROR_INVALID_ALIGNMENT;
    if (address < block_base) return SCE_GXM_ERROR_INVALID_POINTER;
    const uintptr_t displacement = address - block_base;
    if (displacement > block_size || displacement > UINT32_MAX ||
        count > (block_size - displacement) / element_size)
        return SCE_GXM_ERROR_INVALID_VALUE;
    *offset = (uint32_t)displacement;
    return 0;
}
