#include <assert.h>
#include <stdio.h>
#include "../../source/gxm_draw.c"

int main(void)
{
    uint32_t divisor = 123;
    assert(gxm_stream_divisor(SCE_GXM_INDEX_SOURCE_INDEX_16BIT, &divisor) == 0 && divisor == 0);
    assert(gxm_stream_divisor(SCE_GXM_INDEX_SOURCE_INDEX_32BIT, &divisor) == 0 && divisor == 0);
    assert(gxm_stream_divisor(SCE_GXM_INDEX_SOURCE_INSTANCE_16BIT, &divisor) == 0 && divisor == 1);
    assert(gxm_stream_divisor(SCE_GXM_INDEX_SOURCE_INSTANCE_32BIT, &divisor) == 0 && divisor == 1);
    divisor = 123;
    assert((uint32_t)gxm_stream_divisor((SceGxmIndexSource)4, &divisor) == SCE_GXM_ERROR_INVALID_VALUE);
    assert(divisor == 123);
    uint32_t indices = 123, instances = 456;
    assert((uint32_t)gxm_instanced_counts(10, 0, &indices, &instances) == SCE_GXM_ERROR_INVALID_VALUE);
    assert(indices == 123 && instances == 456);
    assert(gxm_instanced_counts(12, 3, &indices, &instances) == 0 && indices == 3 && instances == 4);
    assert(gxm_instanced_counts(13, 3, &indices, &instances) == 0 && indices == 3 && instances == 4);
    assert(gxm_instanced_counts(0, 3, &indices, &instances) == 0 && instances == 0);
    assert(gxm_instanced_counts(UINT32_MAX, 1, &indices, &instances) == 0 && instances == UINT32_MAX);
    const struct { SceGxmPrimitiveType vita; DkPrimitive gpu; } primitives[] = {
        {SCE_GXM_PRIMITIVE_POINTS, DkPrimitive_Points},
        {SCE_GXM_PRIMITIVE_LINES, DkPrimitive_Lines},
        {SCE_GXM_PRIMITIVE_TRIANGLES, DkPrimitive_Triangles},
        {SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, DkPrimitive_TriangleStrip},
        {SCE_GXM_PRIMITIVE_TRIANGLE_FAN, DkPrimitive_TriangleFan},
    };
    for (size_t i = 0; i < sizeof(primitives) / sizeof(primitives[0]); ++i) {
        DkPrimitive gpu;
        assert(gxm_translate_primitive(primitives[i].vita, &gpu) == 0);
        assert(gpu == primitives[i].gpu);
    }
    DkPrimitive gpu = DkPrimitive_TriangleFan;
    assert((uint32_t)gxm_translate_primitive((SceGxmPrimitiveType)123, &gpu) ==
           SCE_GXM_ERROR_INVALID_VALUE && gpu == DkPrimitive_TriangleFan);
    assert((uint32_t)gxm_translate_primitive(SCE_GXM_PRIMITIVE_POINTS, NULL) ==
           SCE_GXM_ERROR_INVALID_POINTER);
    uint32_t offset = 123;
    assert(gxm_validate_index_range(SCE_GXM_INDEX_FORMAT_U16, 0x1002, 0x1000, 10, 4, &offset) == 0);
    assert(offset == 2);
    assert(gxm_validate_index_range(SCE_GXM_INDEX_FORMAT_U32, 0x1004, 0x1000, 12, 2, &offset) == 0);
    assert(offset == 4);
    offset = 123;
    assert((uint32_t)gxm_validate_index_range(SCE_GXM_INDEX_FORMAT_U32, 0x1004, 0x1000, 11, 2, &offset) ==
           SCE_GXM_ERROR_INVALID_VALUE);
    assert(offset == 123);
    assert((uint32_t)gxm_validate_index_range(SCE_GXM_INDEX_FORMAT_U16, 0x1001, 0x1000, 10, 1, &offset) ==
           SCE_GXM_ERROR_INVALID_ALIGNMENT);
    assert((uint32_t)gxm_validate_index_range(SCE_GXM_INDEX_FORMAT_U32, 0x1002, 0x1000, 10, 1, &offset) ==
           SCE_GXM_ERROR_INVALID_ALIGNMENT);
    assert((uint32_t)gxm_validate_index_range(SCE_GXM_INDEX_FORMAT_U16, 0xffe, 0x1000, 10, 1, &offset) ==
           SCE_GXM_ERROR_INVALID_POINTER);
    assert((uint32_t)gxm_validate_index_range(SCE_GXM_INDEX_FORMAT_U16, 0x1010, 0x1000, 10, 0, &offset) ==
           SCE_GXM_ERROR_INVALID_VALUE);
    assert((uint32_t)gxm_validate_index_range((SceGxmIndexFormat)123, 0x1000, 0x1000, 10, 1, &offset) ==
           SCE_GXM_ERROR_INVALID_VALUE);
    assert((uint32_t)gxm_validate_index_range(SCE_GXM_INDEX_FORMAT_U16, 0, 0, 10, 1, &offset) ==
           SCE_GXM_ERROR_INVALID_POINTER);
    assert((uint32_t)gxm_validate_index_range(SCE_GXM_INDEX_FORMAT_U16, 0x1000, 0x1000, 10, 1, NULL) ==
           SCE_GXM_ERROR_INVALID_POINTER);
    assert((uint32_t)gxm_validate_index_range(SCE_GXM_INDEX_FORMAT_U32, 0x1000, 0x1000, UINT32_MAX,
                                            UINT32_MAX, &offset) == SCE_GXM_ERROR_INVALID_VALUE);
    /* Subtraction-based checks also work at the address-space boundary. */
    assert(gxm_validate_index_range(SCE_GXM_INDEX_FORMAT_U32, UINTPTR_MAX - 3,
                                    UINTPTR_MAX - 7, 8, 1, &offset) == 0 && offset == 4);
    puts("GXM index range regression checks passed.");
    return 0;
}
