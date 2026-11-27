#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../source/gxm_surface.c"
#include "../../source/module.c"

DECLARE_LIBRARY(SceGxm, 0xf76b66bd);

int main(void)
{
    SceGxmDepthStencilSurface surface = {0};
    surface.zlsControl = 0xaabbcc11;
    surface.depthData = (void *)(uintptr_t)0x1000;
    surface.stencilData = (void *)(uintptr_t)0x2000;
    surface.backgroundControl = 0xfeed0100;
    SceGxmDepthStencilSurface before = surface;
    void (*set_depth)(SceGxmDepthStencilSurface *, float) =
        (void (*)(SceGxmDepthStencilSurface *, float))module_get_func_export(0xf76b66bd, 0x32F280F0);
    void (*set_stencil)(SceGxmDepthStencilSurface *, unsigned char) =
        (void (*)(SceGxmDepthStencilSurface *, unsigned char))module_get_func_export(0xf76b66bd, 0xF5D3F3E8);
    assert(set_depth && set_stencil);
    for (unsigned value = 0; value < 256; ++value) {
        set_depth(&surface, (float)value / 255.0f);
        set_stencil(&surface, value);
        assert(sceGxmDepthStencilSurfaceGetBackgroundStencil(&surface) == value);
        assert(sceGxmDepthStencilSurfaceGetBackgroundDepth(&surface) == (float)value / 255.0f);
        assert(sceGxmDepthStencilSurfaceGetBackgroundMask(&surface));
        sceGxmDepthStencilSurfaceSetBackgroundMask(&surface, false);
        assert(!sceGxmDepthStencilSurfaceGetBackgroundMask(&surface));
        assert(sceGxmDepthStencilSurfaceGetBackgroundStencil(&surface) == value);
        sceGxmDepthStencilSurfaceSetBackgroundMask(&surface, true);
        assert((surface.backgroundControl & ~0xffu) == before.backgroundControl);
        assert(surface.zlsControl == before.zlsControl);
        assert(surface.depthData == before.depthData && surface.stencilData == before.stencilData);
    }
    const uint32_t nids[] = {0x269B56BE, 0x32F280F0, 0xAAFC062B, 0xF5D3F3E8, 0x85D4DEFE, 0xF28A688A};
    for (unsigned i = 0; i < sizeof(nids) / sizeof(nids[0]); ++i)
        assert(module_get_func_export(0xf76b66bd, nids[i]));
    puts("GXM surface clear values and export checks passed.");
    return 0;
}
