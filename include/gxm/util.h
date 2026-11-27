#ifndef GXM_UTIL_H
#define GXM_UTIL_H

#include <psp2/gxm.h>
#include <switch.h>
#include "gxm/program.h"
#include "gxm/texture.h"
#include "gxm/surface.h"

#define SCE_GXM_MAX_SCENES_PER_RENDERTARGET 8

#define SCE_GXM_NOTIFICATION_COUNT 512

#define SCE_GXM_COLOR_BASE_FORMAT_MASK   0xF1800000U
#define SCE_GXM_TEXTURE_BASE_FORMAT_MASK 0x9f000000U







static inline uint32_t gxm_parameter_type_size(SceGxmParameterType type)
{
    switch (type) {
    case SCE_GXM_PARAMETER_TYPE_U8:
    case SCE_GXM_PARAMETER_TYPE_S8:
        return 1;
    case SCE_GXM_PARAMETER_TYPE_F16:
    case SCE_GXM_PARAMETER_TYPE_U16:
    case SCE_GXM_PARAMETER_TYPE_S16:
        return 2;
    case SCE_GXM_PARAMETER_TYPE_F32:
    case SCE_GXM_PARAMETER_TYPE_U32:
    case SCE_GXM_PARAMETER_TYPE_S32:
    default:
        return 4;
    }
}

static inline void *gxm_texture_get_data(const SceGxmTextureInner *texture)
{
    return (void *)(uintptr_t)((uint32_t)texture->data_addr << 2);
}

static inline uint32_t gxm_texture_get_type(const SceGxmTextureInner *texture)
{
    return (uint32_t)texture->type << 29;
}

static inline size_t gxm_texture_get_width(const SceGxmTextureInner *texture)
{
    if (gxm_texture_get_type(texture) != SCE_GXM_TEXTURE_SWIZZLED &&
        gxm_texture_get_type(texture) != SCE_GXM_TEXTURE_CUBE)
        return texture->width + 1;
    return 1ull << (texture->width_base2 & 0xF);
}

static inline size_t gxm_texture_get_height(const SceGxmTextureInner *texture)
{
    if (gxm_texture_get_type(texture) != SCE_GXM_TEXTURE_SWIZZLED &&
        gxm_texture_get_type(texture) != SCE_GXM_TEXTURE_CUBE)
        return texture->height + 1;
    return 1ull << (texture->height_base2 & 0xF);
}

static inline SceGxmTextureFormat gxm_texture_get_format(const SceGxmTextureInner *texture)
{
    return (SceGxmTextureFormat)((uint32_t)texture->base_format << 24 |
                                 (uint32_t)texture->format0 << 31 |
                                 (uint32_t)texture->swizzle_format << 12);
}

static inline SceGxmTextureBaseFormat gxm_texture_get_base_format(SceGxmTextureFormat src)
{
    return (SceGxmTextureBaseFormat)(src & SCE_GXM_TEXTURE_BASE_FORMAT_MASK);
}

static inline size_t gxm_texture_get_stride_in_bytes(const SceGxmTextureInner *texture)
{
    return ((texture->mip_filter | (texture->min_filter << 1) | (texture->mip_count << 3) |
             (texture->lod_bias << 7)) +
            1) *
           4;
}

static inline bool gxm_base_format_is_paletted_format(SceGxmTextureBaseFormat base_format)
{
    return base_format == SCE_GXM_TEXTURE_BASE_FORMAT_P8 ||
           base_format == SCE_GXM_TEXTURE_BASE_FORMAT_P4;
}

#endif
