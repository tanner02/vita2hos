#include "gxm/draw.h"
#include <string.h>

int gxm_build_vertex_attribute_state(const SceGxmVertexAttribute *attributes, unsigned count,
    const GxmVertexInput *inputs, DkVtxAttribState *state)
{
    if (!state || (count && (!attributes || !inputs)))
        return SCE_GXM_ERROR_INVALID_POINTER;
    if (count > SCE_GXM_MAX_VERTEX_ATTRIBUTES) return SCE_GXM_ERROR_INVALID_VALUE;
    DkVtxAttribState result[SCE_GXM_MAX_VERTEX_ATTRIBUTES] = {0};
    uint32_t used = 0;
    for (unsigned i = 0; i < SCE_GXM_MAX_VERTEX_ATTRIBUTES; ++i) {
        result[i].isFixed = 1;
        result[i].size = DkVtxAttribSize_4x32;
        result[i].type = DkVtxAttribType_Float;
    }
    for (unsigned i = 0; i < count; ++i) {
        const GxmVertexInput *input = &inputs[i];
        if (input->location == UINT32_MAX) continue;
        unsigned elements = attributes[i].componentCount;
        unsigned element_bytes = 0;
        SceGxmAttributeFormat format = attributes[i].format;
        if (input->register_format) {
            elements = input->register_components;
            element_bytes = input->register_component_bytes;
            if (!elements || elements > SCE_GXM_MAX_VERTEX_ATTRIBUTES * 4)
                return SCE_GXM_ERROR_INVALID_VALUE;
            switch (element_bytes) {
            case 1: format = SCE_GXM_ATTRIBUTE_FORMAT_U8; break;
            case 2: format = SCE_GXM_ATTRIBUTE_FORMAT_U16; break;
            case 4: format = SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED; break;
            default: return SCE_GXM_ERROR_INVALID_VALUE;
            }
        } else if (!elements || elements > 4) {
            return SCE_GXM_ERROR_INVALID_VALUE;
        }
        const unsigned slots = input->register_format ? (elements + 3) / 4 :
            (input->array_elements ? input->array_elements : 1);
        unsigned slot_bytes = 4 * element_bytes;
        if (!input->register_format && slots > 1) {
            switch (format) {
            case SCE_GXM_ATTRIBUTE_FORMAT_U8: case SCE_GXM_ATTRIBUTE_FORMAT_S8:
            case SCE_GXM_ATTRIBUTE_FORMAT_U8N: case SCE_GXM_ATTRIBUTE_FORMAT_S8N:
                element_bytes = 1; break;
            case SCE_GXM_ATTRIBUTE_FORMAT_U16: case SCE_GXM_ATTRIBUTE_FORMAT_S16:
            case SCE_GXM_ATTRIBUTE_FORMAT_U16N: case SCE_GXM_ATTRIBUTE_FORMAT_S16N:
            case SCE_GXM_ATTRIBUTE_FORMAT_F16: element_bytes = 2; break;
            case SCE_GXM_ATTRIBUTE_FORMAT_F32:
            case SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED: element_bytes = 4; break;
            default: return SCE_GXM_ERROR_INVALID_VALUE;
            }
            slot_bytes = elements * element_bytes;
        }
        if (input->location >= SCE_GXM_MAX_VERTEX_ATTRIBUTES ||
            slots > SCE_GXM_MAX_VERTEX_ATTRIBUTES - input->location ||
            attributes[i].streamIndex >= SCE_GXM_MAX_VERTEX_STREAMS)
            return SCE_GXM_ERROR_INVALID_VALUE;
        for (unsigned slot = 0; slot < slots; ++slot) {
            const unsigned location = input->location + slot;
            const unsigned offset = attributes[i].offset + slot * slot_bytes;
            if ((used & (1U << location)) || offset > 0x3fff)
                return SCE_GXM_ERROR_INVALID_VALUE;
            const unsigned remaining = input->register_format ? elements - slot * 4 : elements;
            DkVtxAttribType type;
            DkVtxAttribSize size;
            int ret = gxm_translate_vertex_format(format, remaining < 4 ? remaining : 4,
                input->register_format || input->integer_input, &type, &size);
            if (ret) return ret;
            result[location].bufferId = attributes[i].streamIndex;
            result[location].offset = offset;
            result[location].isFixed = 0;
            result[location].type = type;
            result[location].size = size;
            used |= 1U << location;
        }
    }
    memcpy(state, result, sizeof(result));
    return 0;
}

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
            a->format > SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED || a->offset > 0x3fff)
            return SCE_GXM_ERROR_INVALID_VALUE;
    }
    return 0;
}

int gxm_translate_vertex_format(SceGxmAttributeFormat format, unsigned int components, bool integer_input,
                                  DkVtxAttribType *type, DkVtxAttribSize *size)
{
    if (!type || !size) return SCE_GXM_ERROR_INVALID_POINTER;
    if (!components || components > 4) return SCE_GXM_ERROR_INVALID_VALUE;
    DkVtxAttribType result;
    unsigned width;
    switch (format) {
    case SCE_GXM_ATTRIBUTE_FORMAT_U8:
        result = integer_input ? DkVtxAttribType_Uint : DkVtxAttribType_Uscaled; width = 0; break;
    case SCE_GXM_ATTRIBUTE_FORMAT_S8:
        result = integer_input ? DkVtxAttribType_Sint : DkVtxAttribType_Sscaled; width = 0; break;
    case SCE_GXM_ATTRIBUTE_FORMAT_U16:
        result = integer_input ? DkVtxAttribType_Uint : DkVtxAttribType_Uscaled; width = 1; break;
    case SCE_GXM_ATTRIBUTE_FORMAT_S16:
        result = integer_input ? DkVtxAttribType_Sint : DkVtxAttribType_Sscaled; width = 1; break;
    case SCE_GXM_ATTRIBUTE_FORMAT_U8N: result = DkVtxAttribType_Unorm; width = 0; break;
    case SCE_GXM_ATTRIBUTE_FORMAT_S8N: result = DkVtxAttribType_Snorm; width = 0; break;
    case SCE_GXM_ATTRIBUTE_FORMAT_U16N: result = DkVtxAttribType_Unorm; width = 1; break;
    case SCE_GXM_ATTRIBUTE_FORMAT_S16N: result = DkVtxAttribType_Snorm; width = 1; break;
    case SCE_GXM_ATTRIBUTE_FORMAT_F16: result = DkVtxAttribType_Float; width = 1; break;
    case SCE_GXM_ATTRIBUTE_FORMAT_F32: result = DkVtxAttribType_Float; width = 2; break;
    /* Untyped elements carry complete 32-bit register values, without
     * normalization or float conversion during vertex fetch. */
    case SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED: result = DkVtxAttribType_Uint; width = 2; break;
    default: return SCE_GXM_ERROR_INVALID_VALUE;
    }
    static const DkVtxAttribSize sizes[3][4] = {
        {DkVtxAttribSize_1x8, DkVtxAttribSize_2x8, DkVtxAttribSize_3x8, DkVtxAttribSize_4x8},
        {DkVtxAttribSize_1x16, DkVtxAttribSize_2x16, DkVtxAttribSize_3x16, DkVtxAttribSize_4x16},
        {DkVtxAttribSize_1x32, DkVtxAttribSize_2x32, DkVtxAttribSize_3x32, DkVtxAttribSize_4x32}
    };
    *type = result;
    *size = sizes[width][components - 1];
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

DkRasterizerState gxm_effective_rasterizer(const DkRasterizerState *state, bool two_sided)
{
    DkRasterizerState effective = *state;
    if (!two_sided)
        effective.polygonModeBack = effective.polygonModeFront;
    return effective;
}

int gxm_translate_polygon(SceGxmPolygonMode mode, DkPolygonMode *translated)
{
    if (!translated) return SCE_GXM_ERROR_INVALID_POINTER;
    DkPolygonMode result;
    switch (mode) {
    case SCE_GXM_POLYGON_MODE_TRIANGLE_FILL: result = DkPolygonMode_Fill; break;
    case SCE_GXM_POLYGON_MODE_LINE:
    case SCE_GXM_POLYGON_MODE_TRIANGLE_LINE: result = DkPolygonMode_Line; break;
    case SCE_GXM_POLYGON_MODE_POINT_10UV:
    case SCE_GXM_POLYGON_MODE_POINT:
    case SCE_GXM_POLYGON_MODE_POINT_01UV:
    case SCE_GXM_POLYGON_MODE_TRIANGLE_POINT: result = DkPolygonMode_Point; break;
    default: return SCE_GXM_ERROR_INVALID_VALUE;
    }
    *translated = result;
    return 0;
}
