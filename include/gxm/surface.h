#ifndef GXM_SURFACE_H
#define GXM_SURFACE_H
#include <stddef.h>
#include <stdint.h>
#include <psp2/gxm.h>

/* Native ABGR8 linear-image fast path; available starts at the surface data. */
int gxm_color_surface_linear_pitch(uint32_t width, uint32_t height, uint32_t stride,
                                    size_t available, uint32_t *pitch);

/* Packed ZLS descriptor fields, matching the pinned GXM reference layout. */
#define GXM_DS_FORMAT_MASK 0x07eee000u
#define GXM_DS_TYPE_MASK 0x00011000u

static inline SceGxmDepthStencilFormat
gxm_ds_surface_get_format(const SceGxmDepthStencilSurface *surface)
{
    return surface->zlsControl & GXM_DS_FORMAT_MASK;
}

static inline SceGxmDepthStencilSurfaceType
gxm_ds_surface_get_type(const SceGxmDepthStencilSurface *surface)
{
    return surface->zlsControl & GXM_DS_TYPE_MASK;
}

#define SCE_GXM_DEPTH_STENCIL_BG_CTRL_STENCIL_MASK 0xFFu
#define SCE_GXM_DEPTH_STENCIL_BG_CTRL_MASK_BIT     0x100u

#endif
