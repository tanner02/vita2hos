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
    if (registers!=SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT &&
        registers!=SCE_GXM_OUTPUT_REGISTER_SIZE_64BIT) return SCE_GXM_ERROR_UNSUPPORTED;
    if (stride & 1) return SCE_GXM_ERROR_INVALID_ALIGNMENT;
    if (stride<width || ((uintptr_t)data & 3)) return SCE_GXM_ERROR_INVALID_VALUE;
    if (type==SCE_GXM_COLOR_SURFACE_SWIZZLED &&
        ((width & (width-1)) || (height & (height-1)))) return SCE_GXM_ERROR_INVALID_ALIGNMENT;
    SceGxmColorSurfaceInner value={0};
    value.downscale=scale==SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE;
    value.width=width;value.height=height;value.strideInPixels=stride;
    value.clip_x_max=width-1;value.clip_y_max=height-1;
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

EXPORT(SceGxm,0x86456F7B,void,sceGxmColorSurfaceSetClip,SceGxmColorSurface *surface,
       unsigned x_min,unsigned y_min,unsigned x_max,unsigned y_max)
{
    if (!surface) return;
    SceGxmColorSurfaceInner *inner=(void *)surface;
    /* Native getters/setters retain exactly twelve bits per coordinate. */
    inner->clip_x_min=x_min&0xfff;inner->clip_y_min=y_min&0xfff;
    inner->clip_x_max=x_max&0xfff;inner->clip_y_max=y_max&0xfff;
}

EXPORT(SceGxm,0x07DFEE4B,void,sceGxmColorSurfaceGetClip,const SceGxmColorSurface *surface,
       unsigned *x_min,unsigned *y_min,unsigned *x_max,unsigned *y_max)
{
    if (!surface) return;
    const SceGxmColorSurfaceInner *inner=(const void *)surface;
    if (x_min) *x_min=inner->clip_x_min;
    if (y_min) *y_min=inner->clip_y_min;
    if (x_max) *x_max=inner->clip_x_max;
    if (y_max) *y_max=inner->clip_y_max;
}

int gxm_color_surface_store_rect(const SceGxmColorSurfaceInner *surface,
    uint32_t source_width,uint32_t source_height,GxmColorStoreRect *rect)
{
    if (!surface || !rect) return SCE_GXM_ERROR_INVALID_POINTER;
    GxmColorStoreRect result={0};
    uint32_t right=surface->clip_x_max+1,bottom=surface->clip_y_max+1;
    if (right>surface->width) right=surface->width;
    if (bottom>surface->height) bottom=surface->height;
    if (!surface->disabled && right>surface->clip_x_min && bottom>surface->clip_y_min) {
        result=(GxmColorStoreRect){surface->clip_x_min,surface->clip_y_min,
            right-surface->clip_x_min,bottom-surface->clip_y_min};
        bool full=!result.x && !result.y && result.width==surface->width &&
            result.height==surface->height;
        if (!full && (surface->width!=source_width || surface->height!=source_height))
            return SCE_GXM_ERROR_UNSUPPORTED;
    }
    *rect=result;
    return 0;
}

EXPORT(SceGxm,0xEE0B4DF0,SceGxmColorSurfaceGammaMode,sceGxmColorSurfaceGetGammaMode,
       const SceGxmColorSurface *surface)
{
    return surface ? (SceGxmColorSurfaceGammaMode)(((const SceGxmColorSurfaceInner *)surface)->gamma<<12)
                   : SCE_GXM_COLOR_SURFACE_GAMMA_NONE;
}
EXPORT(SceGxm,0xF5C89643,int,sceGxmColorSurfaceSetGammaMode,SceGxmColorSurface *surface,
       SceGxmColorSurfaceGammaMode mode)
{
    if (!surface) return SCE_GXM_ERROR_INVALID_POINTER;
    if (mode!=SCE_GXM_COLOR_SURFACE_GAMMA_NONE && mode!=SCE_GXM_COLOR_SURFACE_GAMMA_BGR &&
        mode!=SCE_GXM_COLOR_SURFACE_GAMMA_GR) return SCE_GXM_ERROR_INVALID_VALUE;
    SceGxmColorSurfaceInner *inner=(SceGxmColorSurfaceInner *)surface;
    /* Native sRGB encodes RGB together; selective GR cannot use that view.
     * Keep unsupported descriptors unchanged rather than silently rendering
     * linear output. Other render-target formats/layouts lack native support. */
    if (mode==SCE_GXM_COLOR_SURFACE_GAMMA_GR ||
        (mode!=SCE_GXM_COLOR_SURFACE_GAMMA_NONE &&
         (inner->surfaceType!=SCE_GXM_COLOR_SURFACE_LINEAR ||
          inner->colorFormat!=SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR)))
        return SCE_GXM_ERROR_UNSUPPORTED;
    inner->gamma=(unsigned)mode>>12;
    return 0;
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

EXPORT(SceGxm,0x0C44ACD7,void,sceGxmDepthStencilSurfaceSetForceLoadMode,
       SceGxmDepthStencilSurface *surface,SceGxmDepthStencilForceLoadMode mode)
{
    if (surface) surface->zlsControl=(surface->zlsControl&~SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_ENABLED)|
        (mode ? SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_ENABLED : 0);
}
EXPORT(SceGxm,0x12AAA7AF,void,sceGxmDepthStencilSurfaceSetForceStoreMode,
       SceGxmDepthStencilSurface *surface,SceGxmDepthStencilForceStoreMode mode)
{
    if (surface) surface->zlsControl=(surface->zlsControl&~SCE_GXM_DEPTH_STENCIL_FORCE_STORE_ENABLED)|
        (mode ? SCE_GXM_DEPTH_STENCIL_FORCE_STORE_ENABLED : 0);
}
EXPORT(SceGxm,0x2F5CC20C,SceGxmDepthStencilForceLoadMode,sceGxmDepthStencilSurfaceGetForceLoadMode,
       const SceGxmDepthStencilSurface *surface)
{
    return surface ? surface->zlsControl&SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_ENABLED : 0;
}
EXPORT(SceGxm,0x544AA05A,SceGxmDepthStencilForceStoreMode,sceGxmDepthStencilSurfaceGetForceStoreMode,
       const SceGxmDepthStencilSurface *surface)
{
    return surface ? surface->zlsControl&SCE_GXM_DEPTH_STENCIL_FORCE_STORE_ENABLED : 0;
}
