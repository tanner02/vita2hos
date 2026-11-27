#ifndef GXM_SURFACE_H
#define GXM_SURFACE_H
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>
#include <psp2/gxm.h>

/* Runtime-owned interpretation of the opaque 48-byte color descriptor. Use a
 * guest address rather than a native pointer so host validation has the same
 * layout as ARM32 and ordinary descriptor copies remain self-contained. */
typedef struct __attribute__((may_alias)) {
    uint32_t disabled : 1;
    uint32_t downscale : 1;
    uint32_t gamma : 2;
    uint32_t pad : 28;
    uint32_t width, height, strideInPixels;
    uint32_t data;
    SceGxmColorFormat colorFormat;
    SceGxmColorSurfaceType surfaceType;
    uint32_t outputRegisterSize;
    SceGxmTexture backgroundTex;
} SceGxmColorSurfaceInner;
static_assert(sizeof(SceGxmColorSurfaceInner)==sizeof(SceGxmColorSurface),"Color surface ABI");
static_assert(offsetof(SceGxmColorSurfaceInner,data)==16,"Color surface guest address offset");
static_assert(offsetof(SceGxmColorSurfaceInner,backgroundTex)==32,"Color surface texture offset");

static inline bool gxm_color_surface_enabled(const SceGxmColorSurfaceInner *surface)
{
    return surface && !surface->disabled;
}
static inline void *gxm_color_surface_data(const SceGxmColorSurfaceInner *surface)
{
    return surface ? (void *)(uintptr_t)surface->data : NULL;
}

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
