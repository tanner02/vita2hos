#include "gxm/draw.h"

int gxm_translate_cull(SceGxmCullMode mode, DkFace *translated)
{
    if (!translated) return SCE_GXM_ERROR_INVALID_POINTER;
    DkFace result;
    /* Context creation fixes the native front-face winding to clockwise. */
    switch (mode) {
    case SCE_GXM_CULL_NONE: result = DkFace_None; break;
    case SCE_GXM_CULL_CW: result = DkFace_Front; break;
    case SCE_GXM_CULL_CCW: result = DkFace_Back; break;
    default: return SCE_GXM_ERROR_INVALID_VALUE;
    }
    *translated = result;
    return 0;
}

int gxm_validate_vertex_layout(const SceGxmVertexAttribute *attributes, unsigned int attribute_count,
                                const SceGxmVertexStream *streams, unsigned int stream_count)
{
    if (attribute_count > SCE_GXM_MAX_VERTEX_ATTRIBUTES || stream_count > SCE_GXM_MAX_VERTEX_STREAMS)
        return SCE_GXM_ERROR_INVALID_VALUE;
    if ((attribute_count && !attributes) || (stream_count && !streams))
        return SCE_GXM_ERROR_INVALID_POINTER;
    for (unsigned int i = 0; i < stream_count; ++i) {
        uint32_t divisor;
        int ret = gxm_stream_divisor((SceGxmIndexSource)streams[i].indexSource, &divisor);
        if (ret != 0) return ret;
    }
    for (unsigned int i = 0; i < attribute_count; ++i) {
        const SceGxmVertexAttribute *a = &attributes[i];
        if (a->streamIndex >= stream_count || !a->componentCount || a->componentCount > 4 ||
            a->format > SCE_GXM_ATTRIBUTE_FORMAT_F32 || a->offset > 0x3fff)
            return SCE_GXM_ERROR_INVALID_VALUE;
    }
    return 0;
}

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
