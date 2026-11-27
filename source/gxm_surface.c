#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <psp2/gxm.h>
#include "gxm/surface.h"
#include "module.h"

int gxm_color_surface_linear_pitch(uint32_t width, uint32_t height, uint32_t stride,
                                    size_t available, uint32_t *pitch)
{
    if (!pitch) return SCE_GXM_ERROR_INVALID_POINTER;
    if (!width || width > 4096 || !height || height > 4096 || stride < width)
        return SCE_GXM_ERROR_INVALID_VALUE;
    uint64_t bytes = (uint64_t)stride * 4;
    if (bytes > UINT32_MAX || bytes * height > available)
        return SCE_GXM_ERROR_INVALID_VALUE;
    if (bytes % 32) return SCE_GXM_ERROR_INVALID_ALIGNMENT;
    *pitch = (uint32_t)bytes;
    return 0;
}

EXPORT(SceGxm, 0xCA9D41D1, int, sceGxmDepthStencilSurfaceInit,
       SceGxmDepthStencilSurface *surface, SceGxmDepthStencilFormat format,
       SceGxmDepthStencilSurfaceType type, unsigned int stride,
       void *depth, void *stencil)
{
    if (!surface) return SCE_GXM_ERROR_INVALID_POINTER;
    if ((uintptr_t)depth % SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT)
        return SCE_GXM_ERROR_INVALID_ALIGNMENT;
    if (!stride || stride > 8192 || stride % SCE_GXM_TILE_SIZEX)
        return SCE_GXM_ERROR_INVALID_VALUE;
    if (type != SCE_GXM_DEPTH_STENCIL_SURFACE_LINEAR && type != SCE_GXM_DEPTH_STENCIL_SURFACE_TILED)
        return SCE_GXM_ERROR_INVALID_VALUE;
    switch (format) {
    case SCE_GXM_DEPTH_STENCIL_FORMAT_DF32:
    case SCE_GXM_DEPTH_STENCIL_FORMAT_S8:
    case SCE_GXM_DEPTH_STENCIL_FORMAT_DF32_S8:
    case SCE_GXM_DEPTH_STENCIL_FORMAT_DF32M:
    case SCE_GXM_DEPTH_STENCIL_FORMAT_DF32M_S8:
    case SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24:
    case SCE_GXM_DEPTH_STENCIL_FORMAT_D16: break;
    default: return SCE_GXM_ERROR_INVALID_VALUE;
    }
    *surface = (SceGxmDepthStencilSurface){
        .zlsControl = 1u | (((stride >> 5) - 1u) << 3) | type | format | 0x100000u,
        .depthData = depth, .stencilData = stencil,
        .backgroundDepth = 1.0f, .backgroundControl = 0x300u,
    };
    return 0;
}

EXPORT(SceGxm, 0xA41DB0D6, int, sceGxmDepthStencilSurfaceInitDisabled,
       SceGxmDepthStencilSurface *surface)
{
    if (!surface) return SCE_GXM_ERROR_INVALID_POINTER;
    *surface = (SceGxmDepthStencilSurface){.backgroundDepth = 1.0f, .backgroundControl = 0x300u};
    return 0;
}

EXPORT(SceGxm, 0x082200E1, SceBool, sceGxmDepthStencilSurfaceIsEnabled,
       const SceGxmDepthStencilSurface *surface)
{
    return surface && gxm_ds_surface_get_format(surface) != 0;
}

float sceGxmDepthStencilSurfaceGetBackgroundDepth(const SceGxmDepthStencilSurface *surface)
{
    return surface ? surface->backgroundDepth : 1.0f;
}

void sceGxmDepthStencilSurfaceSetBackgroundDepth(SceGxmDepthStencilSurface *surface, float depth)
{
    if (surface) surface->backgroundDepth = depth;
}

EXPORT(SceGxm, 0xAAFC062B, unsigned char, sceGxmDepthStencilSurfaceGetBackgroundStencil,
       const SceGxmDepthStencilSurface *surface)
{
    return surface ? surface->backgroundControl & SCE_GXM_DEPTH_STENCIL_BG_CTRL_STENCIL_MASK : 0;
}

EXPORT(SceGxm, 0xF5D3F3E8, void, sceGxmDepthStencilSurfaceSetBackgroundStencil,
       SceGxmDepthStencilSurface *surface, unsigned char stencil)
{
    if (surface)
        surface->backgroundControl =
            (surface->backgroundControl & ~SCE_GXM_DEPTH_STENCIL_BG_CTRL_STENCIL_MASK) | stencil;
}

EXPORT(SceGxm, 0x85D4DEFE, bool, sceGxmDepthStencilSurfaceGetBackgroundMask,
       const SceGxmDepthStencilSurface *surface)
{
    return surface && (surface->backgroundControl & SCE_GXM_DEPTH_STENCIL_BG_CTRL_MASK_BIT);
}

EXPORT(SceGxm, 0xF28A688A, void, sceGxmDepthStencilSurfaceSetBackgroundMask,
       SceGxmDepthStencilSurface *surface, bool mask)
{
    if (surface)
        surface->backgroundControl =
            (surface->backgroundControl & ~SCE_GXM_DEPTH_STENCIL_BG_CTRL_MASK_BIT) |
            (mask ? SCE_GXM_DEPTH_STENCIL_BG_CTRL_MASK_BIT : 0);
}
