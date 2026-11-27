#include "gxm/blend.h"

static int factor(unsigned int value, DkBlendFactor *translated)
{
    switch (value) {
    case SCE_GXM_BLEND_FACTOR_ZERO: *translated = DkBlendFactor_Zero; break;
    case SCE_GXM_BLEND_FACTOR_ONE: *translated = DkBlendFactor_One; break;
    case SCE_GXM_BLEND_FACTOR_SRC_COLOR: *translated = DkBlendFactor_SrcColor; break;
    case SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_COLOR: *translated = DkBlendFactor_InvSrcColor; break;
    case SCE_GXM_BLEND_FACTOR_SRC_ALPHA: *translated = DkBlendFactor_SrcAlpha; break;
    case SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA: *translated = DkBlendFactor_InvSrcAlpha; break;
    case SCE_GXM_BLEND_FACTOR_DST_COLOR: *translated = DkBlendFactor_DstColor; break;
    case SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_COLOR: *translated = DkBlendFactor_InvDstColor; break;
    case SCE_GXM_BLEND_FACTOR_DST_ALPHA: *translated = DkBlendFactor_DstAlpha; break;
    case SCE_GXM_BLEND_FACTOR_ONE_MINUS_DST_ALPHA: *translated = DkBlendFactor_InvDstAlpha; break;
    case SCE_GXM_BLEND_FACTOR_SRC_ALPHA_SATURATE: *translated = DkBlendFactor_SrcAlphaSaturate; break;
    case SCE_GXM_BLEND_FACTOR_DST_ALPHA_SATURATE: return SCE_GXM_ERROR_UNSUPPORTED;
    default: return SCE_GXM_ERROR_INVALID_VALUE;
    }
    return 0;
}

static int channel(unsigned int function, unsigned int source, unsigned int destination,
                    DkBlendOp *operation, DkBlendFactor *src, DkBlendFactor *dst)
{
    *src = DkBlendFactor_One;
    *dst = DkBlendFactor_Zero;
    switch (function) {
    case SCE_GXM_BLEND_FUNC_NONE: /* The reference uses ADD with the supplied factors. */
    case SCE_GXM_BLEND_FUNC_ADD: *operation = DkBlendOp_Add; break;
    case SCE_GXM_BLEND_FUNC_SUBTRACT: *operation = DkBlendOp_Sub; break;
    case SCE_GXM_BLEND_FUNC_REVERSE_SUBTRACT: *operation = DkBlendOp_RevSub; break;
    case SCE_GXM_BLEND_FUNC_MIN: *operation = DkBlendOp_Min; return 0;
    case SCE_GXM_BLEND_FUNC_MAX: *operation = DkBlendOp_Max; return 0;
    default: return SCE_GXM_ERROR_INVALID_VALUE;
    }
    int ret = factor(source, src);
    return ret ? ret : factor(destination, dst);
}

int gxm_translate_blend(const SceGxmBlendInfo *blend, GxmBlendState *translated)
{
    if (!blend || !translated) return SCE_GXM_ERROR_INVALID_POINTER;
    GxmBlendState result = {0};
    if (blend->colorFunc == SCE_GXM_BLEND_FUNC_NONE && blend->alphaFunc == SCE_GXM_BLEND_FUNC_NONE) {
        result.state.colorBlendOp = result.state.alphaBlendOp = DkBlendOp_Add;
        result.state.srcColorBlendFactor = result.state.srcAlphaBlendFactor = DkBlendFactor_One;
        result.state.dstColorBlendFactor = result.state.dstAlphaBlendFactor = DkBlendFactor_Zero;
        *translated = result;
        return 0;
    }
    DkBlendOp color, alpha;
    DkBlendFactor color_src, color_dst, alpha_src, alpha_dst;
    int ret = channel(blend->colorFunc, blend->colorSrc, blend->colorDst,
                      &color, &color_src, &color_dst);
    if (ret) return ret;
    ret = channel(blend->alphaFunc, blend->alphaSrc, blend->alphaDst,
                   &alpha, &alpha_src, &alpha_dst);
    if (ret) return ret;
    result.state.colorBlendOp = color;
    result.state.alphaBlendOp = alpha;
    result.state.srcColorBlendFactor = color_src;
    result.state.dstColorBlendFactor = color_dst;
    result.state.srcAlphaBlendFactor = alpha_src;
    result.state.dstAlphaBlendFactor = alpha_dst;
    result.enabled = blend->colorFunc != SCE_GXM_BLEND_FUNC_NONE || blend->alphaFunc != SCE_GXM_BLEND_FUNC_NONE;
    *translated = result;
    return 0;
}
