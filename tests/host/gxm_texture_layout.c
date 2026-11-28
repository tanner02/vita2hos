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
    /* Gamma changes image interpretation, never the encoded texel bytes. */
    const SceGxmTextureFormat gamma_formats[] = {SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR,
        SCE_GXM_TEXTURE_FORMAT_UBC1_ABGR, SCE_GXM_TEXTURE_FORMAT_UBC2_ABGR,
        SCE_GXM_TEXTURE_FORMAT_UBC3_ABGR};
    const DkImageFormat gamma_native[] = {DkImageFormat_RGBA8_Unorm_sRGB,
        DkImageFormat_RGBA_BC1_sRGB,DkImageFormat_RGBA_BC2_sRGB,DkImageFormat_RGBA_BC3_sRGB};
    for (unsigned i=0;i<4;++i) {
        assert(!sceGxmTextureInitLinear(&texture,NULL,gamma_formats[i],8,8,3));
        GxmTextureMipChain linear,gamma;
        assert(!gxm_texture_get_mip_chain(inner,&linear));
        SceGxmTexture before=texture;
        assert(!sceGxmTextureSetGammaMode(&texture,SCE_GXM_TEXTURE_GAMMA_BGR));
        uint32_t a[4],b[4];memcpy(a,&before,16);memcpy(b,&texture,16);
        assert((a[0]^b[0])==0x08000000u);
        assert(!memcmp(a+1,b+1,12));
        assert(!gxm_texture_get_mip_chain(inner,&gamma));
        assert(gamma.count==3 && linear.count==3);
        assert(gamma.source_size==linear.source_size);
        for (unsigned level=0;level<3;++level) {
            assert(gamma.offsets[level]==linear.offsets[level]);
            GxmTextureLayout expected=linear.levels[level];expected.format=gamma_native[i];
            assert(!memcmp(&expected,&gamma.levels[level],sizeof(expected)));
        }
        unsigned char source[256],out_linear[512],out_gamma[512];
        for (unsigned j=0;j<sizeof(source);++j) source[j]=(unsigned char)(j*17);
        assert(!gxm_texture_pack_linear(&linear.levels[0],source,sizeof(source),out_linear,sizeof(out_linear)));
        assert(!gxm_texture_pack_linear(&gamma.levels[0],source,sizeof(source),out_gamma,sizeof(out_gamma)));
        assert(!memcmp(out_linear,out_gamma,gamma.levels[0].size));
        assert(!sceGxmTextureSetGammaMode(&texture,SCE_GXM_TEXTURE_GAMMA_GR));
        saved=layout;
        assert((uint32_t)gxm_texture_get_linear_layout(inner,&layout)==SCE_GXM_ERROR_UNSUPPORTED);
        assert(!memcmp(&saved,&layout,sizeof(layout)));
    }
    assert(!sceGxmTextureInitLinear(&texture,NULL,SCE_GXM_TEXTURE_FORMAT_R8,8,8,1));
    assert(!sceGxmTextureSetGammaMode(&texture,SCE_GXM_TEXTURE_GAMMA_R));
    assert((uint32_t)gxm_texture_get_linear_layout(inner,&layout)==SCE_GXM_ERROR_UNSUPPORTED);
    assert((uint32_t)sceGxmTextureSetGammaMode(NULL,SCE_GXM_TEXTURE_GAMMA_NONE)==SCE_GXM_ERROR_INVALID_POINTER);
    /* The SDK's row-count boundary applies to physical blocks, not texels. */
    assert(!gxm_texture_native_one_gob(NULL));
    GxmTextureLayout native={0};
    for (unsigned h=0;h<=40;++h) {
        native.height=h;native.block_bytes=0;
        assert(gxm_texture_native_one_gob(&native)==(h>=6 && h<=8));
        native.block_bytes=8;
        assert(gxm_texture_native_one_gob(&native)==(h>=21 && h<=32));
        native.block_bytes=16;
        assert(gxm_texture_native_one_gob(&native)==(h>=21 && h<=32));
    }
    native.height=UINT32_MAX;assert(!gxm_texture_native_one_gob(&native));
    puts("GXM texture-layout regression checks passed.");
    return 0;
}
