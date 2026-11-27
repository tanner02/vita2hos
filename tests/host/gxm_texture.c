#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../../source/gxm_texture.c"

int main(void)
{
    SceGxmTexture texture;
    SceGxmTextureInner *inner = (SceGxmTextureInner *)&texture;
    memset(&texture, 0xff, sizeof(texture));
    assert(sceGxmTextureInitLinear(&texture, (void *)(uintptr_t)0x1000,
                                 SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR, 128, 64, 1) == 0);
    assert(inner->type == (SCE_GXM_TEXTURE_LINEAR >> 29));
    assert(inner->width == 127 && inner->height == 63 && inner->mip_count == 0);
    assert(inner->data_addr == 0x400 && inner->palette_addr == 0 && inner->mip_filter == 0);
    SceGxmTexture initialized = texture;
    assert((uint32_t)sceGxmTextureInitLinear(&texture, NULL,
           SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR, 0, 64, 1) == SCE_GXM_ERROR_INVALID_VALUE);
    assert(memcmp(&texture, &initialized, sizeof(texture)) == 0);
    inner->type = SCE_GXM_TEXTURE_LINEAR >> 29;
    inner->width = 127;
    inner->height = 63;
    inner->base_format = 9;
    inner->lod_bias = 31;
    assert(sceGxmTextureSetMinFilter(&texture, SCE_GXM_TEXTURE_FILTER_LINEAR) == 0);
    assert(sceGxmTextureSetMagFilter(&texture, SCE_GXM_TEXTURE_FILTER_POINT) == 0);
    assert(inner->min_filter == 1 && inner->mag_filter == 0);
    assert(sceGxmTextureSetUAddrMode(&texture, SCE_GXM_TEXTURE_ADDR_REPEAT) == 0);
    assert(sceGxmTextureSetVAddrModeSafe(&texture, SCE_GXM_TEXTURE_ADDR_CLAMP) == 0);
    assert(inner->uaddr_mode == 0 && inner->vaddr_mode == 2);
    assert(sceGxmTextureSetData(&texture, (void *)(uintptr_t)0x12345678) == 0);
    assert(inner->data_addr == (0x12345678 >> 2));
    assert(inner->width == 127 && inner->height == 63 && inner->base_format == 9);
    assert(inner->lod_bias == 31 && inner->type == (SCE_GXM_TEXTURE_LINEAR >> 29));

    SceGxmTexture saved = texture;
    assert((uint32_t)sceGxmTextureSetMinFilter(&texture, SCE_GXM_TEXTURE_FILTER_MIPMAP_LINEAR) ==
           SCE_GXM_ERROR_UNSUPPORTED);
    assert((uint32_t)sceGxmTextureSetUAddrMode(&texture, SCE_GXM_TEXTURE_ADDR_MIRROR) ==
           SCE_GXM_ERROR_UNSUPPORTED);
    assert((uint32_t)sceGxmTextureSetVAddrMode(&texture, (SceGxmTextureAddrMode)8) ==
           SCE_GXM_ERROR_INVALID_VALUE);
    assert(memcmp(&saved, &texture, sizeof(texture)) == 0);
    assert((uint32_t)sceGxmTextureSetData(NULL, NULL) == SCE_GXM_ERROR_INVALID_POINTER);
    assert((uint32_t)sceGxmTextureSetMagFilter(NULL, SCE_GXM_TEXTURE_FILTER_POINT) ==
           SCE_GXM_ERROR_INVALID_POINTER);

    inner->type = SCE_GXM_TEXTURE_SWIZZLED >> 29;
    assert(sceGxmTextureSetUAddrModeSafe(&texture, SCE_GXM_TEXTURE_ADDR_MIRROR) == 0);
    assert(sceGxmTextureSetVAddrMode(&texture, SCE_GXM_TEXTURE_ADDR_CLAMP_FULL_BORDER) == 0);
    assert(inner->uaddr_mode == 1 && inner->vaddr_mode == 5);
    inner->type = SCE_GXM_TEXTURE_CUBE >> 29;
    assert((uint32_t)sceGxmTextureSetUAddrMode(&texture, SCE_GXM_TEXTURE_ADDR_REPEAT) ==
           SCE_GXM_ERROR_UNSUPPORTED);
    assert(sceGxmTextureSetUAddrMode(&texture, SCE_GXM_TEXTURE_ADDR_CLAMP) == 0);
    inner->type = SCE_GXM_TEXTURE_LINEAR_STRIDED >> 29;
    assert((uint32_t)sceGxmTextureSetMinFilter(&texture, SCE_GXM_TEXTURE_FILTER_POINT) ==
           SCE_GXM_ERROR_UNSUPPORTED);
    assert(sceGxmTextureSetMagFilter(&texture, SCE_GXM_TEXTURE_FILTER_LINEAR) == 0);
    assert(sceGxmTextureSetData(&texture, NULL) == 0 && inner->data_addr == 0);
    SceGxmTexture strided = texture;
    assert((uint32_t)sceGxmTextureSetLodBias(&texture, 0) == SCE_GXM_ERROR_UNSUPPORTED);
    assert((uint32_t)sceGxmTextureSetLodMin(&texture, 3) == SCE_GXM_ERROR_UNSUPPORTED);
    assert((uint32_t)sceGxmTextureSetMipFilter(&texture, SCE_GXM_TEXTURE_MIP_FILTER_ENABLED) == SCE_GXM_ERROR_UNSUPPORTED);
    assert((uint32_t)sceGxmTextureSetMipmapCount(&texture, 3) == SCE_GXM_ERROR_UNSUPPORTED);
    assert(!memcmp(&texture, &strided, sizeof(texture)));
    assert(!sceGxmTextureInitSwizzled(&texture, NULL, SCE_GXM_TEXTURE_FORMAT_R8, 32, 32, 1));
    assert(!sceGxmTextureSetMipmapCount(&texture, 0) && inner->mip_count == 15);
    assert(!sceGxmTextureSetMipmapCount(&texture, 13) && inner->mip_count == 12);
    assert(!sceGxmTextureSetMipFilter(&texture, SCE_GXM_TEXTURE_MIP_FILTER_ENABLED) && inner->mip_filter == 1);
    assert(!sceGxmTextureSetLodBias(&texture, 63) && inner->lod_bias == 63);
    assert(!sceGxmTextureSetLodMin(&texture, 15) && inner->lod_min0 == 3 && inner->lod_min1 == 3);
    SceGxmTexture mip_saved = texture;
    assert((uint32_t)sceGxmTextureSetMipmapCount(&texture, 14) == SCE_GXM_ERROR_INVALID_VALUE);
    assert((uint32_t)sceGxmTextureSetLodBias(&texture, 64) == SCE_GXM_ERROR_INVALID_VALUE);
    assert((uint32_t)sceGxmTextureSetMipFilter(&texture, (SceGxmTextureMipFilter)1) == SCE_GXM_ERROR_INVALID_VALUE);
    assert(!memcmp(&texture, &mip_saved, sizeof(texture)));
    assert((uint32_t)sceGxmTextureSetMipmapCount(NULL, 1) == SCE_GXM_ERROR_INVALID_POINTER);
    puts("GXM texture-state regression checks passed.");
    return 0;
}
