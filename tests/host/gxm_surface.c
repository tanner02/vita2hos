#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../source/gxm_surface.c"
#include "../../source/module.c"

DECLARE_LIBRARY(SceGxm, 0xf76b66bd);

int main(void)
{
    uint32_t pitch = 0;
    assert(gxm_color_surface_linear_pitch(960, 544, 960, 960 * 544 * 4, &pitch) == 0);
    assert(pitch == 3840);
    assert(gxm_color_surface_linear_pitch(959, 544, 960, 960 * 544 * 4, &pitch) == 0);
    pitch = 123;
    assert(gxm_color_surface_linear_pitch(960, 544, 960, 960 * 544 * 4 - 1, &pitch) != 0);
    assert(pitch == 123);
    assert(gxm_color_surface_linear_pitch(960, 544, 959, SIZE_MAX, &pitch) != 0);
    assert(gxm_color_surface_linear_pitch(960, 544, 962, SIZE_MAX, &pitch) != 0);
    assert(gxm_color_surface_linear_pitch(0, 544, 960, SIZE_MAX, &pitch) != 0);
    assert(gxm_color_surface_linear_pitch(960, 0, 960, SIZE_MAX, &pitch) != 0);
    assert(gxm_color_surface_linear_pitch(4097, 544, 4104, SIZE_MAX, &pitch) != 0);
    assert(gxm_color_surface_linear_pitch(960, 4097, 960, SIZE_MAX, &pitch) != 0);
    assert(gxm_color_surface_linear_pitch(960, 544, UINT32_MAX, SIZE_MAX, &pitch) != 0);
    assert(gxm_color_surface_linear_pitch(960, 544, 960, SIZE_MAX, NULL) != 0);
    const SceGxmDepthStencilFormat formats[] = {
        SCE_GXM_DEPTH_STENCIL_FORMAT_DF32, SCE_GXM_DEPTH_STENCIL_FORMAT_S8,
        SCE_GXM_DEPTH_STENCIL_FORMAT_DF32_S8, SCE_GXM_DEPTH_STENCIL_FORMAT_DF32M,
        SCE_GXM_DEPTH_STENCIL_FORMAT_DF32M_S8, SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24,
        SCE_GXM_DEPTH_STENCIL_FORMAT_D16,
    };
    SceGxmDepthStencilSurface descriptor;
    for (unsigned f = 0; f < sizeof(formats) / sizeof(formats[0]); ++f) {
        for (unsigned tiled = 0; tiled < 2; ++tiled) {
            SceGxmDepthStencilSurfaceType type = tiled ? SCE_GXM_DEPTH_STENCIL_SURFACE_TILED :
                                                       SCE_GXM_DEPTH_STENCIL_SURFACE_LINEAR;
            for (unsigned stride = 32; stride <= 8192; stride += 32) {
                assert(sceGxmDepthStencilSurfaceInit(&descriptor, formats[f], type, stride,
                                                     (void *)(uintptr_t)0x1000, NULL) == 0);
                assert(gxm_ds_surface_get_format(&descriptor) == formats[f]);
                assert(gxm_ds_surface_get_type(&descriptor) == type);
                assert((((descriptor.zlsControl >> 3) & 255u) + 1u) * 32u == stride);
                assert(sceGxmDepthStencilSurfaceIsEnabled(&descriptor));
                assert(descriptor.backgroundDepth == 1.0f && descriptor.backgroundControl == 0x300);
            }
        }
    }
    SceGxmDepthStencilSurface saved = descriptor;
    const unsigned invalid_strides[] = {0, 1, 31, 33, 8193, 8224, ~0u};
    for (unsigned i = 0; i < sizeof(invalid_strides) / sizeof(invalid_strides[0]); ++i) {
        assert((uint32_t)sceGxmDepthStencilSurfaceInit(&descriptor, formats[0], 0,
               invalid_strides[i], NULL, NULL) == SCE_GXM_ERROR_INVALID_VALUE);
        assert(memcmp(&descriptor, &saved, sizeof(saved)) == 0);
    }
    assert((uint32_t)sceGxmDepthStencilSurfaceInit(&descriptor, formats[0], 0, 32,
            (void *)(uintptr_t)1, NULL) == SCE_GXM_ERROR_INVALID_ALIGNMENT);
    assert((uint32_t)sceGxmDepthStencilSurfaceInit(&descriptor, 0, 0, 32, NULL, NULL) ==
           SCE_GXM_ERROR_INVALID_VALUE);
    assert(sceGxmDepthStencilSurfaceInitDisabled(&descriptor) == 0);
    assert(!sceGxmDepthStencilSurfaceIsEnabled(&descriptor));
    assert(descriptor.zlsControl == 0 && !descriptor.depthData && !descriptor.stencilData);
    assert(descriptor.backgroundDepth == 1.0f && descriptor.backgroundControl == 0x300);
    assert((uint32_t)sceGxmDepthStencilSurfaceInitDisabled(NULL) == SCE_GXM_ERROR_INVALID_POINTER);
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
    const uint32_t nids[] = {0x269B56BE, 0x32F280F0, 0xAAFC062B, 0xF5D3F3E8, 0x85D4DEFE,
                           0xF28A688A, 0xCA9D41D1, 0xA41DB0D6, 0x082200E1};
    for (unsigned i = 0; i < sizeof(nids) / sizeof(nids[0]); ++i)
        assert(module_get_func_export(0xf76b66bd, nids[i]));
    puts("GXM surface clear values and export checks passed.");
    return 0;
}
