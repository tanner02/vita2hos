#include <assert.h>
#include <stdio.h>
#include "../../source/gxm_draw.c"

int main(void)
{
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
