#include <stdbool.h>
#include <string.h>

#include "gxm/texture.h"
#include "module.h"

static uint32_t texture_type(const SceGxmTextureInner *texture)
{
    return (uint32_t)texture->type << 29;
}

EXPORT(SceGxm, 0x4811AECB, int, sceGxmTextureInitLinear, SceGxmTexture *texture, const void *data,
       SceGxmTextureFormat format, unsigned int width, unsigned int height,
       unsigned int mip_count)
{
    if (!texture)
        return SCE_GXM_ERROR_INVALID_POINTER;
    if (width == 0 || height == 0 || width > 4096 || height > 4096 || mip_count > 13)
        return SCE_GXM_ERROR_INVALID_VALUE;

    SceGxmTextureInner *inner = (SceGxmTextureInner *)texture;
    memset(texture, 0, sizeof(*texture));
    inner->mip_count = mip_count == 0 ? 15 : mip_count - 1;
    inner->format0 = ((uint32_t)format >> 31) & 1;
    inner->lod_bias = 31;
    inner->uaddr_mode = SCE_GXM_TEXTURE_ADDR_CLAMP;
    inner->vaddr_mode = SCE_GXM_TEXTURE_ADDR_CLAMP;
    inner->height = height - 1;
    inner->width = width - 1;
    inner->base_format = ((uint32_t)format >> 24) & 0x1f;
    inner->type = SCE_GXM_TEXTURE_LINEAR >> 29;
    inner->data_addr = (uintptr_t)data >> 2;
    inner->swizzle_format = ((uint32_t)format >> 12) & 7;
    inner->normalize_mode = 1;
    inner->min_filter = SCE_GXM_TEXTURE_FILTER_POINT;
    inner->mag_filter = SCE_GXM_TEXTURE_FILTER_POINT;
    return 0;
}

EXPORT(SceGxm, 0xFA695FD7, int, sceGxmTextureSetMagFilter, SceGxmTexture *texture,
       SceGxmTextureFilter filter)
{
    if (!texture)
        return SCE_GXM_ERROR_INVALID_POINTER;
    if (filter != SCE_GXM_TEXTURE_FILTER_POINT && filter != SCE_GXM_TEXTURE_FILTER_LINEAR)
        return SCE_GXM_ERROR_UNSUPPORTED;
    ((SceGxmTextureInner *)texture)->mag_filter = filter;
    return 0;
}

EXPORT(SceGxm, 0x416764E3, int, sceGxmTextureSetMinFilter, SceGxmTexture *texture,
       SceGxmTextureFilter filter)
{
    if (!texture)
        return SCE_GXM_ERROR_INVALID_POINTER;
    SceGxmTextureInner *inner = (SceGxmTextureInner *)texture;
    if (texture_type(inner) == SCE_GXM_TEXTURE_LINEAR_STRIDED ||
        (filter != SCE_GXM_TEXTURE_FILTER_POINT && filter != SCE_GXM_TEXTURE_FILTER_LINEAR))
        return SCE_GXM_ERROR_UNSUPPORTED;
    inner->min_filter = filter;
    return 0;
}

static int set_address_mode(SceGxmTexture *texture, SceGxmTextureAddrMode mode, bool u_axis)
{
    if (!texture)
        return SCE_GXM_ERROR_INVALID_POINTER;
    if ((uint32_t)mode > SCE_GXM_TEXTURE_ADDR_CLAMP_HALF_BORDER)
        return SCE_GXM_ERROR_INVALID_VALUE;
    SceGxmTextureInner *inner = (SceGxmTextureInner *)texture;
    uint32_t type = texture_type(inner);
    if (type == SCE_GXM_TEXTURE_CUBE || type == SCE_GXM_TEXTURE_CUBE_ARBITRARY) {
        if (mode != SCE_GXM_TEXTURE_ADDR_CLAMP)
            return SCE_GXM_ERROR_UNSUPPORTED;
    } else if ((mode == SCE_GXM_TEXTURE_ADDR_MIRROR ||
                mode >= SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER) &&
               type != SCE_GXM_TEXTURE_SWIZZLED) {
        return SCE_GXM_ERROR_UNSUPPORTED;
    }

    /* Border-ignore modes still need renderer translation. */
    if (mode == SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER ||
        mode == SCE_GXM_TEXTURE_ADDR_CLAMP_IGNORE_BORDER ||
        mode == SCE_GXM_TEXTURE_ADDR_CLAMP_HALF_BORDER)
        return SCE_GXM_ERROR_UNSUPPORTED;
    if (u_axis)
        inner->uaddr_mode = mode;
    else
        inner->vaddr_mode = mode;
    return 0;
}

EXPORT(SceGxm, 0x4281763E, int, sceGxmTextureSetUAddrMode, SceGxmTexture *texture,
       SceGxmTextureAddrMode mode)
{
    return set_address_mode(texture, mode, true);
}
EXPORT(SceGxm, 0x8699ECF4, int, sceGxmTextureSetUAddrModeSafe, SceGxmTexture *texture,
       SceGxmTextureAddrMode mode)
{
    return set_address_mode(texture, mode, true);
}
EXPORT(SceGxm, 0x126CDAA3, int, sceGxmTextureSetVAddrMode, SceGxmTexture *texture,
       SceGxmTextureAddrMode mode)
{
    return set_address_mode(texture, mode, false);
}
EXPORT(SceGxm, 0xFA22F6CC, int, sceGxmTextureSetVAddrModeSafe, SceGxmTexture *texture,
       SceGxmTextureAddrMode mode)
{
    return set_address_mode(texture, mode, false);
}

EXPORT(SceGxm, 0x855814C4, int, sceGxmTextureSetData, SceGxmTexture *texture, const void *data)
{
    if (!texture)
        return SCE_GXM_ERROR_INVALID_POINTER;
    ((SceGxmTextureInner *)texture)->data_addr = (uintptr_t)data >> 2;
    return 0;
}
