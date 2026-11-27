#include <stdbool.h>
#include <string.h>

#include "gxm/texture.h"
#include "module.h"

int gxm_texture_set_binding(SceGxmTextureInner bindings[GXM_TEXTURE_BINDING_COUNT],
                            bool vertex, unsigned int unit, const SceGxmTexture *texture)
{
    if (!bindings || !texture) return SCE_GXM_ERROR_INVALID_POINTER;
    if (unit >= SCE_GXM_MAX_TEXTURE_UNITS) return SCE_GXM_ERROR_INVALID_VALUE;
    unsigned index = gxm_texture_binding_index(vertex, unit);
    memcpy(&bindings[index], texture, sizeof(*texture));
    return 0;
}

static uint32_t texture_type(const SceGxmTextureInner *texture)
{
    return (uint32_t)texture->type << 29;
}

static int init_texture(SceGxmTexture *texture, const void *data, SceGxmTextureFormat format,
                         unsigned int width, unsigned int height, unsigned int mip_count,
                         SceGxmTextureType type)
{
    if (!texture)
        return SCE_GXM_ERROR_INVALID_POINTER;
    if (width == 0 || height == 0 || width > 4096 || height > 4096 || mip_count > 13)
        return SCE_GXM_ERROR_INVALID_VALUE;
    if (type == SCE_GXM_TEXTURE_SWIZZLED &&
        ((width & (width - 1)) != 0 || (height & (height - 1)) != 0))
        return SCE_GXM_ERROR_INVALID_VALUE;

    SceGxmTextureInner *inner = (SceGxmTextureInner *)texture;
    memset(texture, 0, sizeof(*texture));
    inner->mip_count = mip_count == 0 ? 15 : mip_count - 1;
    inner->format0 = ((uint32_t)format >> 31) & 1;
    inner->lod_bias = 31;
    inner->uaddr_mode = SCE_GXM_TEXTURE_ADDR_CLAMP;
    inner->vaddr_mode = SCE_GXM_TEXTURE_ADDR_CLAMP;
    if (type == SCE_GXM_TEXTURE_SWIZZLED) {
        inner->uaddr_mode = inner->vaddr_mode = SCE_GXM_TEXTURE_ADDR_MIRROR;
        while ((1U << inner->width_base2) < width)
            inner->width_base2++;
        while ((1U << inner->height_base2) < height)
            inner->height_base2++;
    } else {
        inner->height = height - 1;
        inner->width = width - 1;
    }
    inner->base_format = ((uint32_t)format >> 24) & 0x1f;
    inner->type = (uint32_t)type >> 29;
    inner->data_addr = (uintptr_t)data >> 2;
    inner->swizzle_format = ((uint32_t)format >> 12) & 7;
    inner->normalize_mode = 1;
    inner->min_filter = SCE_GXM_TEXTURE_FILTER_POINT;
    inner->mag_filter = SCE_GXM_TEXTURE_FILTER_POINT;
    return 0;
}

EXPORT(SceGxm, 0x4811AECB, int, sceGxmTextureInitLinear, SceGxmTexture *texture, const void *data,
       SceGxmTextureFormat format, unsigned int width, unsigned int height, unsigned int mip_count)
{
    return init_texture(texture, data, format, width, height, mip_count, SCE_GXM_TEXTURE_LINEAR);
}
EXPORT(SceGxm, 0xD572D547, int, sceGxmTextureInitSwizzled, SceGxmTexture *texture, const void *data,
       SceGxmTextureFormat format, unsigned int width, unsigned int height, unsigned int mip_count)
{
    return init_texture(texture, data, format, width, height, mip_count, SCE_GXM_TEXTURE_SWIZZLED);
}
EXPORT(SceGxm, 0x5DBFBA2C, int, sceGxmTextureInitSwizzledArbitrary, SceGxmTexture *texture,
       const void *data, SceGxmTextureFormat format, unsigned int width, unsigned int height,
       unsigned int mip_count)
{
    return init_texture(texture, data, format, width, height, mip_count,
                         SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY);
}
EXPORT(SceGxm, 0xE6F0DB27, int, sceGxmTextureInitTiled, SceGxmTexture *texture, const void *data,
       SceGxmTextureFormat format, unsigned int width, unsigned int height, unsigned int mip_count)
{
    return init_texture(texture, data, format, width, height, mip_count, SCE_GXM_TEXTURE_TILED);
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

EXPORT(SceGxm, 0x6679BEF0, int, sceGxmTextureInitLinearStrided, SceGxmTexture *texture,
       const void *data, SceGxmTextureFormat format, unsigned int width,
       unsigned int height, unsigned int byteStride)
{
    if (!texture) return SCE_GXM_ERROR_INVALID_POINTER;
    if (!width || !height || width > 4096 || height > 4096 ||
        byteStride < 4 || byteStride > 131072) return SCE_GXM_ERROR_INVALID_VALUE;
    if (byteStride & 3) return SCE_GXM_ERROR_INVALID_ALIGNMENT;
    /* The pinned descriptor codec exposes 13 stride bits. Do not truncate
     * larger API-valid strides until their additional encoding is known. */
    if (byteStride > 32768) return SCE_GXM_ERROR_UNSUPPORTED;
    SceGxmTexture result;
    int ret=init_texture(&result,data,format,width,height,1,SCE_GXM_TEXTURE_LINEAR_STRIDED);
    if (ret) return ret;
    SceGxmTextureInner *inner=(SceGxmTextureInner *)&result;
    uint32_t encoded=(byteStride >> 2)-1;
    inner->mip_filter=encoded & 1;
    inner->min_filter=(encoded >> 1) & 3;
    inner->mip_count=(encoded >> 3) & 15;
    inner->lod_bias=(encoded >> 7) & 63;
    *texture=result;
    return 0;
}

static int set_dimension(SceGxmTexture *texture, unsigned int value, bool width)
{
    if (!texture) return SCE_GXM_ERROR_INVALID_POINTER;
    if (!value || value > 4096) return SCE_GXM_ERROR_INVALID_VALUE;
    SceGxmTextureInner *inner=(SceGxmTextureInner *)texture;
    uint32_t type=texture_type(inner);
    if (type == SCE_GXM_TEXTURE_SWIZZLED) {
        if (value & (value-1)) return SCE_GXM_ERROR_INVALID_VALUE;
        unsigned int exponent=0;
        while ((1U << exponent) < value) ++exponent;
        if (width) inner->width_base2=exponent;
        else inner->height_base2=exponent;
    } else if (type == SCE_GXM_TEXTURE_LINEAR || type == SCE_GXM_TEXTURE_LINEAR_STRIDED ||
               type == SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY || type == SCE_GXM_TEXTURE_TILED) {
        if (width) inner->width=value-1;
        else inner->height=value-1;
    } else return SCE_GXM_ERROR_UNSUPPORTED;
    return 0;
}

EXPORT(SceGxm, 0x2EA178BE, int, sceGxmTextureSetWidth, SceGxmTexture *texture, unsigned int width)
{
    return set_dimension(texture,width,true);
}
EXPORT(SceGxm, 0xAEE7FDD1, int, sceGxmTextureSetHeight, SceGxmTexture *texture, unsigned int height)
{
    return set_dimension(texture,height,false);
}
EXPORT(SceGxm, 0x5A690B60, int, _sceGxmTextureSetWidth, SceGxmTexture *texture, unsigned int width)
{
    return set_dimension(texture,width,true);
}
EXPORT(SceGxm, 0x1B20D5DF, int, _sceGxmTextureSetHeight, SceGxmTexture *texture, unsigned int height)
{
    return set_dimension(texture,height,false);
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

EXPORT(SceGxm, 0xB65EE6F7, int, sceGxmTextureSetLodBias, SceGxmTexture *texture, unsigned int bias)
{
    if (!texture) return SCE_GXM_ERROR_INVALID_POINTER;
    SceGxmTextureInner *inner = (SceGxmTextureInner *)texture;
    if (texture_type(inner) == SCE_GXM_TEXTURE_LINEAR_STRIDED) return SCE_GXM_ERROR_UNSUPPORTED;
    if (bias > 63) return SCE_GXM_ERROR_INVALID_VALUE;
    inner->lod_bias = bias;
    return 0;
}

EXPORT(SceGxm, 0xB79E43DD, int, sceGxmTextureSetLodMin, SceGxmTexture *texture, unsigned int lodMin)
{
    if (!texture) return SCE_GXM_ERROR_INVALID_POINTER;
    SceGxmTextureInner *inner = (SceGxmTextureInner *)texture;
    if (texture_type(inner) == SCE_GXM_TEXTURE_LINEAR_STRIDED) return SCE_GXM_ERROR_UNSUPPORTED;
    inner->lod_min0 = lodMin & 3; inner->lod_min1 = lodMin >> 2;
    return 0;
}

EXPORT(SceGxm, 0x1CA9FE0B, int, sceGxmTextureSetMipFilter, SceGxmTexture *texture,
       SceGxmTextureMipFilter filter)
{
    if (!texture) return SCE_GXM_ERROR_INVALID_POINTER;
    SceGxmTextureInner *inner = (SceGxmTextureInner *)texture;
    if (texture_type(inner) == SCE_GXM_TEXTURE_LINEAR_STRIDED) return SCE_GXM_ERROR_UNSUPPORTED;
    if (filter != SCE_GXM_TEXTURE_MIP_FILTER_DISABLED && filter != SCE_GXM_TEXTURE_MIP_FILTER_ENABLED)
        return SCE_GXM_ERROR_INVALID_VALUE;
    inner->mip_filter = filter == SCE_GXM_TEXTURE_MIP_FILTER_ENABLED;
    return 0;
}

EXPORT(SceGxm, 0xD2DC4643, int, sceGxmTextureSetMipmapCount, SceGxmTexture *texture, unsigned int count)
{
    if (!texture) return SCE_GXM_ERROR_INVALID_POINTER;
    SceGxmTextureInner *inner = (SceGxmTextureInner *)texture;
    if (texture_type(inner) == SCE_GXM_TEXTURE_LINEAR_STRIDED) return SCE_GXM_ERROR_UNSUPPORTED;
    if (count > 13) return SCE_GXM_ERROR_INVALID_VALUE;
    inner->mip_count = count ? count - 1 : 15;
    return 0;
}

EXPORT(SceGxm, 0x855814C4, int, sceGxmTextureSetData, SceGxmTexture *texture, const void *data)
{
    if (!texture)
        return SCE_GXM_ERROR_INVALID_POINTER;
    ((SceGxmTextureInner *)texture)->data_addr = (uintptr_t)data >> 2;
    return 0;
}

EXPORT(SceGxm, 0xA6D9F4DA, int, sceGxmTextureSetGammaMode, SceGxmTexture *texture,
       SceGxmTextureGammaMode mode)
{
    if (!texture)
        return SCE_GXM_ERROR_INVALID_POINTER;
    /* Match the packed descriptor operation; conversion support is checked
     * separately when the renderer builds its native image layout. */
    ((SceGxmTextureInner *)texture)->gamma_mode = (uint32_t)mode >> 27;
    return 0;
}
