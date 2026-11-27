#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <psp2/gxm.h>
#include "gxm/surface.h"
#include "module.h"

EXPORT(SceGxm,0xED0F6E25,int,sceGxmColorSurfaceInit,SceGxmColorSurface *surface,
       SceGxmColorFormat format,SceGxmColorSurfaceType type,SceGxmColorSurfaceScaleMode scale,
       SceGxmOutputRegisterSize registers,unsigned width,unsigned height,unsigned stride,void *data)
{
    if (!surface || !data) return SCE_GXM_ERROR_INVALID_POINTER;
#if UINTPTR_MAX>UINT32_MAX
    if ((uintptr_t)data>UINT32_MAX) return SCE_GXM_ERROR_INVALID_POINTER;
#endif
    if (!width || width>4096 || !height || height>4096) return SCE_GXM_ERROR_INVALID_VALUE;
    if (stride & 1) return SCE_GXM_ERROR_INVALID_ALIGNMENT;
    if (stride<width || ((uintptr_t)data & 3)) return SCE_GXM_ERROR_INVALID_VALUE;
    if (type==SCE_GXM_COLOR_SURFACE_SWIZZLED &&
        ((width & (width-1)) || (height & (height-1)))) return SCE_GXM_ERROR_INVALID_ALIGNMENT;
    SceGxmColorSurfaceInner value={0};
    value.downscale=scale==SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE;
    value.width=width;value.height=height;value.strideInPixels=stride;
    value.data=(uint32_t)(uintptr_t)data;
    value.colorFormat=format;value.surfaceType=type;value.outputRegisterSize=registers;
    memcpy(surface,&value,sizeof(value));
    return 0;
}

EXPORT(SceGxm,0x613639FA,int,sceGxmColorSurfaceInitDisabled,SceGxmColorSurface *surface)
{
    if (!surface) return SCE_GXM_ERROR_INVALID_POINTER;
    SceGxmColorSurfaceInner value={.disabled=1};
    int result=sceGxmTextureInitLinear(&value.backgroundTex,NULL,
                                      SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_BGRA,1,1,0);
    if (result) return result;
    memcpy(surface,&value,sizeof(value));
    return 0;
}

EXPORT(SceGxm,0x0E0EBB57,SceBool,sceGxmColorSurfaceIsEnabled,const SceGxmColorSurface *surface)
{
    return gxm_color_surface_enabled((const SceGxmColorSurfaceInner *)surface);
}
EXPORT(SceGxm,0x2DB6026C,void *,sceGxmColorSurfaceGetData,const SceGxmColorSurface *surface)
{
    return gxm_color_surface_data((const SceGxmColorSurfaceInner *)surface);
}
EXPORT(SceGxm,0xF33D9980,unsigned,sceGxmColorSurfaceGetStrideInPixels,const SceGxmColorSurface *surface)
{
    return surface ? ((const SceGxmColorSurfaceInner *)surface)->strideInPixels : 0;
}

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
