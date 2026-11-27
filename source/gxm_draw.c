#include "gxm/draw.h"

int gxm_stream_divisor(SceGxmIndexSource source, uint32_t *divisor)
{
    if (!divisor) return SCE_GXM_ERROR_INVALID_POINTER;
    switch (source) {
    case SCE_GXM_INDEX_SOURCE_INDEX_16BIT:
    case SCE_GXM_INDEX_SOURCE_INDEX_32BIT: *divisor = 0; return 0;
    case SCE_GXM_INDEX_SOURCE_INSTANCE_16BIT:
    case SCE_GXM_INDEX_SOURCE_INSTANCE_32BIT: *divisor = 1; return 0;
    default: return SCE_GXM_ERROR_INVALID_VALUE;
    }
}

int gxm_instanced_counts(unsigned int total, unsigned int wrap,
                         uint32_t *indices, uint32_t *instances)
{
    if (!indices || !instances) return SCE_GXM_ERROR_INVALID_POINTER;
    if (!wrap) return SCE_GXM_ERROR_INVALID_VALUE;
    *indices = wrap;
    /* Match the pinned reference: incomplete trailing instances are ignored. */
    *instances = total / wrap;
    return 0;
}

int gxm_translate_primitive(SceGxmPrimitiveType primitive, DkPrimitive *translated)
{
    if (!translated) return SCE_GXM_ERROR_INVALID_POINTER;
    DkPrimitive result;
    switch (primitive) {
    case SCE_GXM_PRIMITIVE_POINTS: result = DkPrimitive_Points; break;
    case SCE_GXM_PRIMITIVE_LINES: result = DkPrimitive_Lines; break;
    case SCE_GXM_PRIMITIVE_TRIANGLES: result = DkPrimitive_Triangles; break;
    case SCE_GXM_PRIMITIVE_TRIANGLE_STRIP: result = DkPrimitive_TriangleStrip; break;
    case SCE_GXM_PRIMITIVE_TRIANGLE_FAN: result = DkPrimitive_TriangleFan; break;
    /* Preserve the existing triangle-edges fallback; its edge semantics
     * still need a dedicated implementation and hardware validation. */
    case SCE_GXM_PRIMITIVE_TRIANGLE_EDGES: result = DkPrimitive_Triangles; break;
    default: return SCE_GXM_ERROR_INVALID_VALUE;
    }
    *translated = result;
    return 0;
}

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
