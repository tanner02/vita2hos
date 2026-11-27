#include <assert.h>
#include <stdio.h>

#include "../../source/gxm_texture.c"
#include "../../source/gxm_texture_layout.c"

static void check_swizzle(const GxmTextureLayout *layout, DkImageSwizzle r, DkImageSwizzle g,
                          DkImageSwizzle b, DkImageSwizzle a)
{
    assert(layout->swizzle[0] == r && layout->swizzle[1] == g);
    assert(layout->swizzle[2] == b && layout->swizzle[3] == a);
}

int main(void)
{
    SceGxmTexture texture;
    SceGxmTextureInner *inner = (SceGxmTextureInner *)&texture;
    GxmTextureLayout layout;
    assert(sceGxmTextureInitLinear(&texture, NULL, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ARGB,
                                 9, 7, 1) == 0);
    assert(gxm_texture_get_linear_layout(inner, &layout) == 0);
    assert(layout.format == DkImageFormat_RGBA8_Unorm);
    assert(layout.width == 9 && layout.height == 7 && layout.pitch == 64 && layout.size == 448);
    check_swizzle(&layout, DkImageSwizzle_Blue, DkImageSwizzle_Green,
                  DkImageSwizzle_Red, DkImageSwizzle_Alpha);

    assert(sceGxmTextureInitLinear(&texture, NULL, SCE_GXM_TEXTURE_FORMAT_L8, 32, 16, 1) == 0);
    assert(gxm_texture_get_linear_layout(inner, &layout) == 0);
    assert(layout.format == DkImageFormat_R8_Unorm && layout.pitch == 32 && layout.size == 512);
    check_swizzle(&layout, DkImageSwizzle_Red, DkImageSwizzle_Red,
                  DkImageSwizzle_Red, DkImageSwizzle_One);
    inner->swizzle_format = SCE_GXM_TEXTURE_SWIZZLE1_R000 >> 12;
    assert(gxm_texture_get_linear_layout(inner, &layout) == 0);
    check_swizzle(&layout, DkImageSwizzle_Zero, DkImageSwizzle_Zero,
                  DkImageSwizzle_Zero, DkImageSwizzle_Red);

    assert(sceGxmTextureInitLinear(&texture, NULL, SCE_GXM_TEXTURE_FORMAT_F16F16_GRRR,
                                 16, 8, 1) == 0);
    assert(gxm_texture_get_linear_layout(inner, &layout) == 0);
    assert(layout.format == DkImageFormat_RG16_Float && layout.pitch == 64);
    check_swizzle(&layout, DkImageSwizzle_Red, DkImageSwizzle_Red,
                  DkImageSwizzle_Red, DkImageSwizzle_Green);
    inner->swizzle_format = 7;
    assert((uint32_t)gxm_texture_get_linear_layout(inner, &layout) == SCE_GXM_ERROR_INVALID_VALUE);

    assert(sceGxmTextureInitLinear(&texture, NULL, SCE_GXM_TEXTURE_FORMAT_R8, 8, 8, 1) == 0);
    assert(gxm_texture_get_linear_layout(inner, &layout) == 0);
    assert(layout.source_pitch == 8 && layout.pitch == 32);
    inner->type = SCE_GXM_TEXTURE_LINEAR_STRIDED >> 29;
    inner->mip_filter = 1;
    inner->min_filter = 3; /* Encoded stride: ((1 | 6) + 1) * 4 == 32. */
    inner->mip_count = 0;
    inner->lod_bias = 0;
    assert(gxm_texture_get_linear_layout(inner, &layout) == 0 && layout.pitch == 32);
    inner->min_filter = 0;
    inner->mip_filter = 0;
    assert((uint32_t)gxm_texture_get_linear_layout(inner, &layout) == SCE_GXM_ERROR_INVALID_VALUE);
    inner->type = SCE_GXM_TEXTURE_CUBE >> 29;
    assert((uint32_t)gxm_texture_get_linear_layout(inner, &layout) == SCE_GXM_ERROR_UNSUPPORTED);
    assert((uint32_t)gxm_texture_get_linear_layout(NULL, &layout) == SCE_GXM_ERROR_INVALID_POINTER);
    assert(sceGxmTextureInitLinear(&texture, NULL, SCE_GXM_TEXTURE_FORMAT_PVRT2BPP_ABGR, 32, 32, 1) == 0);
    GxmTextureLayout saved = layout;
    assert((uint32_t)gxm_texture_get_linear_layout(inner, &layout) == SCE_GXM_ERROR_UNSUPPORTED);
    assert(memcmp(&saved, &layout, sizeof(layout)) == 0);
    const unsigned strides[]={4,12,64,512,4096,16384,32768};
    for (unsigned i=0;i<sizeof(strides)/sizeof(*strides);++i) {
        assert(!sceGxmTextureInitLinearStrided(&texture,NULL,SCE_GXM_TEXTURE_FORMAT_R8,
                                               3,2,strides[i]));
        assert(!gxm_texture_get_linear_layout(inner,&layout));
        assert(layout.source_pitch==strides[i] && layout.source_size==strides[i]*2);
        assert(layout.width==3 && layout.height==2);
    }
    uint32_t words[4];memcpy(words,&texture,sizeof(words));assert(words[0]==0x07fe0e90);
    assert(!sceGxmTextureInitTiled(&texture,NULL,SCE_GXM_TEXTURE_FORMAT_R8,32,32,1));
    assert(!sceGxmTextureSetWidth(&texture,33) && !sceGxmTextureSetHeight(&texture,17));
    assert(!gxm_texture_get_linear_layout(inner,&layout));
    assert(layout.width==33 && layout.height==17 && layout.source_width==64 && layout.source_height==32);
    puts("GXM texture-layout regression checks passed.");
    return 0;
}
