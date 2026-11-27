#include <assert.h>
#include <stdio.h>
#include "../../source/gxm_texture.c"
#include "../../source/gxm_texture_layout.c"

int main(void)
{
    SceGxmTexture texture;
    SceGxmTextureInner *inner = (SceGxmTextureInner *)&texture;
    GxmTextureMipChain chain;
    assert(!sceGxmTextureInitSwizzled(&texture, NULL, SCE_GXM_TEXTURE_FORMAT_R8, 32, 16, 13));
    assert(!gxm_texture_get_mip_chain(inner, &chain));
    const unsigned offsets[] = {0, 512, 640, 672, 680};
    assert(chain.count == 5 && chain.source_size == 682);
    for (unsigned i = 0; i < chain.count; ++i) {
        assert(chain.offsets[i] == offsets[i]);
        assert(chain.levels[i].width == (32U >> i) && chain.levels[i].height == (16U >> i));
    }
    assert(!sceGxmTextureInitSwizzled(&texture, NULL, SCE_GXM_TEXTURE_FORMAT_A8B8G8R8, 32, 16, 5));
    assert(!gxm_texture_get_mip_chain(inner, &chain));
    assert(chain.count == 5 && chain.offsets[1] == 2048 && chain.source_size == 2728);
    assert(!sceGxmTextureInitSwizzled(&texture, NULL, SCE_GXM_TEXTURE_FORMAT_R8, 4096, 4096, 13));
    assert(!gxm_texture_get_mip_chain(inner, &chain));
    assert(chain.count == 13 && chain.source_size == 22369621);
    assert(chain.levels[12].width == 1 && chain.levels[12].height == 1);
    assert(!sceGxmTextureInitLinear(&texture, NULL, SCE_GXM_TEXTURE_FORMAT_R8, 9, 5, 3));
    inner->mip_filter = 1;
    assert(!gxm_texture_get_mip_chain(inner, &chain));
    assert(chain.count == 3 && chain.offsets[1] == 128 && chain.offsets[2] == 160);
    assert(chain.source_size == 168);
    unsigned char source[168], destination[4096];
    memset(source, 0xcc, sizeof(source));
    for (unsigned i = 0; i < chain.count; ++i) {
        const GxmTextureLayout *level = &chain.levels[i];
        for (unsigned y = 0; y < level->height; ++y)
            memset(source + chain.offsets[i] + y * level->source_pitch, 10 + i, level->width);
    }
    for (unsigned i = 0; i < chain.count; ++i) {
        const GxmTextureLayout *level = &chain.levels[i];
        assert(!gxm_texture_pack_linear(level, source + chain.offsets[i],
            sizeof(source) - chain.offsets[i], destination, sizeof(destination)));
        for (unsigned y = 0; y < level->height; ++y)
            for (unsigned x = 0; x < level->width; ++x)
                assert(destination[y * level->pitch + x] == 10 + i);
    }
    assert(!sceGxmTextureInitTiled(&texture, NULL, SCE_GXM_TEXTURE_FORMAT_R8, 35, 33, 2));
    assert(!gxm_texture_get_mip_chain(inner, &chain));
    assert(chain.count == 2 && chain.offsets[1] == 4096 && chain.source_size == 5120);
    assert(!sceGxmTextureInitSwizzledArbitrary(&texture, NULL, SCE_GXM_TEXTURE_FORMAT_R8, 5, 3, 2));
    assert(!gxm_texture_get_mip_chain(inner, &chain));
    assert(chain.count == 2 && chain.offsets[1] == 32 && chain.source_size == 34);
    assert(!sceGxmTextureInitLinear(&texture, NULL, SCE_GXM_TEXTURE_FORMAT_R8, 9, 5, 0));
    assert(!gxm_texture_get_mip_chain(inner, &chain));
    assert(chain.count == 1 && chain.source_size == 80);
    inner->type = SCE_GXM_TEXTURE_LINEAR_STRIDED >> 29;
    inner->mip_filter = 1; inner->min_filter = 1; inner->mip_count = 3; inner->lod_bias = 0;
    assert(!gxm_texture_get_mip_chain(inner, &chain));
    assert(chain.count == 1 && chain.levels[0].source_pitch == 112 && chain.source_size == 560);
    memset(&chain, 0xff, sizeof(chain));
    assert((uint32_t)gxm_texture_get_mip_chain(NULL, &chain) == SCE_GXM_ERROR_INVALID_POINTER);
    GxmTextureMipChain empty = {0};
    assert(!memcmp(&chain, &empty, sizeof(chain)));
    puts("Mip-chain dimensions, offsets and source packing checks passed.");
}
