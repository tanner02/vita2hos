#include <psp2/gxm.h>
#include "module.h"

/* Vita GXM's native ABI passes scalar floats in VFP registers. The softfp
 * Vita SDK inserts shims before its import stubs to call that ABI, so changing
 * only the guest compiler flags does not change the import boundary.
 * Keep the runtime's internal/base-PCS functions and adapt only these exports.
 * SceLibm is different: its SDK stubs use the base PCS and need no adapter. */
#ifdef __arm__
#define GXM_VFP __attribute__((pcs("aapcs-vfp")))
#else
#define GXM_VFP
#endif

static void GXM_VFP vita_gxm_set_viewport(SceGxmContext *context,
    float xo, float xs, float yo, float ys, float zo, float zs)
{
    sceGxmSetViewport(context, xo, xs, yo, ys, zo, zs);
}
EXPORT_FUNC(SceGxm, 0x3EB3380B, vita_gxm_set_viewport)

static void GXM_VFP vita_gxm_set_background_depth(SceGxmDepthStencilSurface *surface, float depth)
{
    sceGxmDepthStencilSurfaceSetBackgroundDepth(surface, depth);
}
EXPORT_FUNC(SceGxm, 0x32F280F0, vita_gxm_set_background_depth)

static float GXM_VFP vita_gxm_get_background_depth(const SceGxmDepthStencilSurface *surface)
{
    return sceGxmDepthStencilSurfaceGetBackgroundDepth(surface);
}
EXPORT_FUNC(SceGxm, 0x269B56BE, vita_gxm_get_background_depth)
