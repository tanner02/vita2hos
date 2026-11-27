#include <assert.h>
#include <stdio.h>
#include "../../source/gxm_texture.c"
#include "../../source/gxm_texture_layout.c"

int main(void)
{
    const SceGxmTextureFormat formats[] = {SCE_GXM_TEXTURE_FORMAT_UBC1_ABGR,
        SCE_GXM_TEXTURE_FORMAT_UBC2_ABGR, SCE_GXM_TEXTURE_FORMAT_UBC3_ABGR,
        SCE_GXM_TEXTURE_FORMAT_UBC4_R, SCE_GXM_TEXTURE_FORMAT_SBC4_R,
        SCE_GXM_TEXTURE_FORMAT_UBC5_GR, SCE_GXM_TEXTURE_FORMAT_SBC5_GR};
    const DkImageFormat native[] = {DkImageFormat_RGBA_BC1, DkImageFormat_RGBA_BC2, DkImageFormat_RGBA_BC3,
        DkImageFormat_R_BC4_Unorm, DkImageFormat_R_BC4_Snorm,
        DkImageFormat_RG_BC5_Unorm, DkImageFormat_RG_BC5_Snorm};
    const unsigned sizes[] = {8, 16, 16, 8, 8, 16, 16};
    const unsigned orders[2][8] = {{0,4,1,5,2,6,3,7}, {0,2,1,3,4,6,5,7}};
    SceGxmTexture texture;
    SceGxmTextureInner *inner = (SceGxmTextureInner *)&texture;
    GxmTextureLayout layout;
    GxmTextureMipChain chain;
    unsigned char source[512], destination[512];
    for (unsigned f = 0; f < sizeof(formats) / sizeof(formats[0]); ++f) {
        unsigned bytes = sizes[f];
        for (unsigned shape = 0; shape < 2; ++shape) {
            unsigned width = shape ? 8 : 16, height = shape ? 16 : 8;
            assert(!sceGxmTextureInitSwizzled(&texture, NULL, formats[f], width, height, 1));
            assert(!gxm_texture_get_linear_layout(inner, &layout));
            assert(layout.format == native[f] && layout.block_bytes == bytes && !layout.bytes_per_pixel);
            assert(layout.source_size == 8 * bytes);
            for (unsigned block = 0; block < 8; ++block)
                for (unsigned byte = 0; byte < bytes; ++byte)
                    source[block * bytes + byte] = orders[shape][block] * 16 + byte;
            assert(!gxm_texture_pack_linear(&layout, source, layout.source_size, destination, sizeof(destination)));
            for (unsigned y = 0; y < height / 4; ++y)
                for (unsigned x = 0; x < width / 4; ++x)
                    for (unsigned byte = 0; byte < bytes; ++byte)
                        assert(destination[y * layout.pitch + x * bytes + byte] == (y * (width / 4) + x) * 16 + byte);
        }
        assert(!sceGxmTextureInitSwizzled(&texture, NULL, formats[f], 32, 16, 5));
        assert(!gxm_texture_get_mip_chain(inner, &chain));
        assert(chain.count == 5 && chain.offsets[1] == 32 * bytes && chain.source_size == 44 * bytes);
        assert(chain.levels[4].width == 2 && chain.levels[4].height == 1);
        assert(chain.levels[4].source_size == bytes);
    }
    assert(!sceGxmTextureInitLinear(&texture, NULL, formats[0], 9, 5, 3));
    assert(!gxm_texture_get_mip_chain(inner, &chain));
    assert(chain.offsets[1] == 64 && chain.offsets[2] == 80 && chain.source_size == 96);
    layout = chain.levels[0];
    assert(layout.source_pitch == 32 && layout.source_height == 2 && layout.pitch == 32);
    memset(source, 0xcc, sizeof(source));
    for (unsigned y = 0; y < 2; ++y) memset(source + y * 32, 0x31 + y, 24);
    assert(!gxm_texture_pack_linear(&layout, source, 64, destination, sizeof(destination)));
    for (unsigned y = 0; y < 2; ++y) {
        for (unsigned x = 0; x < 24; ++x) assert(destination[y * 32 + x] == 0x31 + y);
        for (unsigned x = 24; x < 32; ++x) assert(destination[y * 32 + x] == 0);
    }
    memset(destination, 0xa5, sizeof(destination));
    assert((uint32_t)gxm_texture_pack_linear(&layout, source, 63, destination, sizeof(destination)) == SCE_GXM_ERROR_INVALID_VALUE);
    assert(destination[0] == 0xa5);
    assert(!sceGxmTextureInitSwizzled(&texture, NULL, SCE_GXM_TEXTURE_FORMAT_UBC1_1BGR, 1, 1, 1));
    assert(!gxm_texture_get_linear_layout(inner, &layout));
    assert(layout.source_size == 8 && layout.swizzle[3] == DkImageSwizzle_One);
    assert(!sceGxmTextureInitSwizzled(&texture, NULL, SCE_GXM_TEXTURE_FORMAT_SBC4_RRRR, 4, 4, 1));
    assert(!gxm_texture_get_linear_layout(inner, &layout));
    assert(layout.format == DkImageFormat_R_BC4_Snorm);
    for (unsigned i = 0; i < 4; ++i) assert(layout.swizzle[i] == DkImageSwizzle_Red);
    assert(!sceGxmTextureInitSwizzled(&texture, NULL, SCE_GXM_TEXTURE_FORMAT_UBC5_GR, 4, 4, 1));
    assert(!gxm_texture_get_linear_layout(inner, &layout));
    assert(layout.swizzle[0] == DkImageSwizzle_Red && layout.swizzle[1] == DkImageSwizzle_Green);
    assert(layout.swizzle[2] == DkImageSwizzle_Zero && layout.swizzle[3] == DkImageSwizzle_One);
    inner->swizzle_format = 7;
    assert((uint32_t)gxm_texture_get_linear_layout(inner, &layout) == SCE_GXM_ERROR_INVALID_VALUE);
    assert(!sceGxmTextureInitTiled(&texture, NULL, formats[0], 32, 32, 1));
    assert((uint32_t)gxm_texture_get_linear_layout(inner, &layout) == SCE_GXM_ERROR_UNSUPPORTED);
    puts("BC texture format, block order, padding and mip-tail checks passed.");
}
