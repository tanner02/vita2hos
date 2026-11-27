#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../source/gxm_blend.c"

int main(void)
{
    SceGxmBlendInfo blend = {SCE_GXM_COLOR_MASK_ALL, SCE_GXM_BLEND_FUNC_ADD, SCE_GXM_BLEND_FUNC_ADD,
        SCE_GXM_BLEND_FACTOR_SRC_ALPHA, SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        SCE_GXM_BLEND_FACTOR_ONE, SCE_GXM_BLEND_FACTOR_ZERO};
    GxmBlendState result;
    assert(gxm_translate_blend(&blend, &result) == 0 && result.enabled);
    assert(result.state.colorBlendOp == DkBlendOp_Add && result.state.alphaBlendOp == DkBlendOp_Add);
    assert(result.state.srcColorBlendFactor == DkBlendFactor_SrcAlpha);
    assert(result.state.dstColorBlendFactor == DkBlendFactor_InvSrcAlpha);
    assert(result.state.srcAlphaBlendFactor == DkBlendFactor_One);
    assert(result.state.dstAlphaBlendFactor == DkBlendFactor_Zero);
    blend.colorFunc = SCE_GXM_BLEND_FUNC_NONE;
    assert(gxm_translate_blend(&blend, &result) == 0 && result.enabled);
    assert(result.state.srcColorBlendFactor == DkBlendFactor_SrcAlpha && result.state.dstColorBlendFactor == DkBlendFactor_InvSrcAlpha);
    blend.alphaFunc = SCE_GXM_BLEND_FUNC_NONE;
    assert(gxm_translate_blend(&blend, &result) == 0 && !result.enabled);
    blend.colorFunc = SCE_GXM_BLEND_FUNC_REVERSE_SUBTRACT;
    blend.colorSrc = SCE_GXM_BLEND_FACTOR_ONE;
    blend.colorDst = SCE_GXM_BLEND_FACTOR_ONE;
    assert(gxm_translate_blend(&blend, &result) == 0 && result.state.colorBlendOp == DkBlendOp_RevSub);
    GxmBlendState unchanged = result;
    blend.colorSrc = SCE_GXM_BLEND_FACTOR_DST_ALPHA_SATURATE;
    assert((uint32_t)gxm_translate_blend(&blend, &result) == SCE_GXM_ERROR_UNSUPPORTED);
    assert(memcmp(&result, &unchanged, sizeof(result)) == 0);
    blend.colorFunc = SCE_GXM_BLEND_FUNC_MIN;
    assert(gxm_translate_blend(&blend, &result) == 0 && result.state.colorBlendOp == DkBlendOp_Min);
    blend.colorFunc = 15;
    assert((uint32_t)gxm_translate_blend(&blend, &result) == SCE_GXM_ERROR_INVALID_VALUE);
    assert((uint32_t)gxm_translate_blend(NULL, &result) == SCE_GXM_ERROR_INVALID_POINTER);
    puts("Fragment blending translation checks passed.");
    return 0;
}
