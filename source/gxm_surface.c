#include <stdbool.h>
#include <psp2/gxm.h>
#include "gxm/surface.h"
#include "module.h"

EXPORT(SceGxm, 0x269B56BE, float, sceGxmDepthStencilSurfaceGetBackgroundDepth,
       const SceGxmDepthStencilSurface *surface)
{
    return surface ? surface->backgroundDepth : 1.0f;
}

EXPORT(SceGxm, 0x32F280F0, void, sceGxmDepthStencilSurfaceSetBackgroundDepth,
       SceGxmDepthStencilSurface *surface, float depth)
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
