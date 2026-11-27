#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "gxm/util.h"
#include "../../source/gxm_texture.c"
#include "../../source/gxm_surface.c"
static SceGxmContext *viewport_context;
static float viewport_arguments[6];
void sceGxmSetViewport(SceGxmContext *context, float xo, float xs, float yo, float ys, float zo, float zs)
{
    viewport_context = context;
    const float arguments[6] = {xo, xs, yo, ys, zo, zs};
    memcpy(viewport_arguments, arguments, sizeof(arguments));
}
#include "../../source/gxm_vfp.c"
#include "../../source/module.c"

DECLARE_LIBRARY(SceGxm, 0xf76b66bd);

int main(void)
{
    void (*set_viewport)(SceGxmContext *, float, float, float, float, float, float) =
        (void (*)(SceGxmContext *, float, float, float, float, float, float))
        module_get_func_export(0xf76b66bd, 0x3EB3380B);
    assert(set_viewport);
    SceGxmContext *fake_context = (SceGxmContext *)(uintptr_t)0x1234;
    const float transform[6] = {480, -480, 272, 272, 0.25f, -0.75f};
    set_viewport(fake_context, 480, -480, 272, 272, 0.25f, -0.75f);
    assert(viewport_context == fake_context);
    assert(memcmp(viewport_arguments, transform, sizeof(transform)) == 0);
    int (*disable_color)(SceGxmColorSurface *)=(void *)module_get_func_export(0xf76b66bd,0x613639FA);
    void *(*color_data)(const SceGxmColorSurface *)=(void *)module_get_func_export(0xf76b66bd,0x2DB6026C);
    unsigned (*color_stride)(const SceGxmColorSurface *)=(void *)module_get_func_export(0xf76b66bd,0xF33D9980);
    SceBool (*color_enabled)(const SceGxmColorSurface *)=(void *)module_get_func_export(0xf76b66bd,0x0E0EBB57);
    assert(disable_color && color_data && color_stride && color_enabled);
    assert(!color_data(NULL) && !color_stride(NULL) && !color_enabled(NULL));
    struct { uint32_t before;SceGxmColorSurface color;uint32_t after; } guarded_color=
        {.before=0x12345678,.after=0xabcdef01};
    SceGxmColorSurface *color=&guarded_color.color;
    assert(!sceGxmColorSurfaceInit(color,SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR,
        SCE_GXM_COLOR_SURFACE_LINEAR,SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE,
        SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,960,544,1024,(void *)0x81000000));
    assert(color_enabled(color) && color_data(color)==(void *)0x81000000 && color_stride(color)==1024);
    assert(color->pbeSidebandWord==2 && color->pbeEmitWords[3]==0x81000000);
    SceGxmColorSurface copy=*color;
    assert(color_data(&copy)==color_data(color) && color_stride(&copy)==color_stride(color));
    void (*set_clip)(SceGxmColorSurface *,unsigned,unsigned,unsigned,unsigned)=
        (void *)module_get_func_export(0xf76b66bd,0x86456F7B);
    void (*get_clip)(const SceGxmColorSurface *,unsigned *,unsigned *,unsigned *,unsigned *)=
        (void *)module_get_func_export(0xf76b66bd,0x07DFEE4B);
    assert(set_clip && get_clip);
    unsigned bounds[4]={99,99,99,99};
    get_clip(color,bounds,bounds+1,bounds+2,bounds+3);
    assert(bounds[0]==0 && bounds[1]==0 && bounds[2]==959 && bounds[3]==543);
    set_clip(color,0x1007,0x200b,0x3028,0x402b);
    get_clip(color,bounds,bounds+1,bounds+2,bounds+3);
    assert(bounds[0]==7 && bounds[1]==11 && bounds[2]==40 && bounds[3]==43);
    get_clip(color,NULL,NULL,NULL,NULL);
    GxmColorStoreRect rect={99,99,99,99};
    assert(!gxm_color_surface_store_rect((const void *)color,960,544,&rect));
    assert(rect.x==7 && rect.y==11 && rect.width==34 && rect.height==33);
    SceGxmColorSurface clipped_copy=*color;
    set_clip(color,1,2,1,2);
    assert(!gxm_color_surface_store_rect((const void *)color,960,544,&rect));
    assert(rect.x==1 && rect.y==2 && rect.width==1 && rect.height==1);
    get_clip(&clipped_copy,NULL,NULL,bounds+2,bounds+3);
    assert(bounds[2]==40 && bounds[3]==43);
    const SceGxmColorSurfaceInner *inner=(const void *)color;
    assert(inner->width==960 && inner->height==544 && inner->strideInPixels==1024 &&
        inner->data==0x81000000 && inner->downscale && !inner->disabled &&
        inner->colorFormat==SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR &&
        inner->surfaceType==SCE_GXM_COLOR_SURFACE_LINEAR && !inner->outputRegisterSize);
    rect=(GxmColorStoreRect){99,98,97,96};
    GxmColorStoreRect unchanged_rect=rect;
    assert((uint32_t)gxm_color_surface_store_rect((const void *)color,480,272,&rect)
        ==SCE_GXM_ERROR_UNSUPPORTED);
    assert(!memcmp(&rect,&unchanged_rect,sizeof(rect)));
    set_clip(color,40,20,39,30);
    assert(!gxm_color_surface_store_rect((const void *)color,480,272,&rect) && !rect.width && !rect.height);
    set_clip(color,UINT32_MAX,0,UINT32_MAX,UINT32_MAX);
    get_clip(color,bounds,NULL,bounds+2,bounds+3);
    assert(bounds[0]==4095 && bounds[2]==4095 && bounds[3]==4095);
    assert(!gxm_color_surface_store_rect((const void *)color,960,544,&rect) && !rect.width);
    set_clip(color,0,0,4095,4095);
    assert(!gxm_color_surface_store_rect((const void *)color,480,272,&rect));
    assert(rect.width==960 && rect.height==544);
    get_clip(NULL,bounds,NULL,NULL,NULL);set_clip(NULL,0,0,0,0);
    assert((uint32_t)gxm_color_surface_store_rect(NULL,1,1,&rect)==SCE_GXM_ERROR_INVALID_POINTER);
    *color=copy;
    int (*set_gamma)(SceGxmColorSurface *,SceGxmColorSurfaceGammaMode)=
        (void *)module_get_func_export(0xf76b66bd,0xF5C89643);
    SceGxmColorSurfaceGammaMode (*get_gamma)(const SceGxmColorSurface *)=
        (void *)module_get_func_export(0xf76b66bd,0xEE0B4DF0);
    assert(set_gamma && get_gamma && get_gamma(color)==SCE_GXM_COLOR_SURFACE_GAMMA_NONE);
    assert((uint32_t)set_gamma(NULL,0)==SCE_GXM_ERROR_INVALID_POINTER);
    assert(!set_gamma(color,SCE_GXM_COLOR_SURFACE_GAMMA_BGR));
    assert(get_gamma(color)==SCE_GXM_COLOR_SURFACE_GAMMA_BGR);
    SceGxmColorSurface gamma_copy=*color;
    assert(get_gamma(&gamma_copy)==SCE_GXM_COLOR_SURFACE_GAMMA_BGR);
    assert((uint32_t)set_gamma(color,SCE_GXM_COLOR_SURFACE_GAMMA_GR)==SCE_GXM_ERROR_UNSUPPORTED);
    assert((uint32_t)set_gamma(color,(SceGxmColorSurfaceGammaMode)0x2000)==SCE_GXM_ERROR_INVALID_VALUE);
    assert(!memcmp(color,&gamma_copy,sizeof(gamma_copy)));
    assert(!set_gamma(color,SCE_GXM_COLOR_SURFACE_GAMMA_NONE));
    assert(!memcmp(color,&copy,sizeof(copy)));
    ((SceGxmColorSurfaceInner *)color)->surfaceType=SCE_GXM_COLOR_SURFACE_TILED;
    SceGxmColorSurface tiled=*color;
    assert((uint32_t)set_gamma(color,SCE_GXM_COLOR_SURFACE_GAMMA_BGR)==SCE_GXM_ERROR_UNSUPPORTED);
    assert(!memcmp(color,&tiled,sizeof(tiled)));*color=copy;
    const struct { unsigned width,height,stride;uintptr_t data;SceGxmColorSurfaceType type; } bad_color[]={
        {0,544,1024,0x1000,0},{4097,544,4104,0x1000,0},
        {960,0,1024,0x1000,0},{960,4097,1024,0x1000,0},
        {960,544,961,0x1000,0},{960,544,958,0x1000,0},
        {960,544,1024,0x1001,0},{960,544,1024,0,0},
        {96,64,96,0x1000,SCE_GXM_COLOR_SURFACE_SWIZZLED}};
    for (unsigned i=0;i<sizeof(bad_color)/sizeof(bad_color[0]);++i) {
        assert(sceGxmColorSurfaceInit(color,SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR,
            bad_color[i].type,SCE_GXM_COLOR_SURFACE_SCALE_NONE,SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
            bad_color[i].width,bad_color[i].height,bad_color[i].stride,(void *)bad_color[i].data)!=0);
        assert(!memcmp(color,&copy,sizeof(copy)));
    }
#if UINTPTR_MAX>UINT32_MAX
    assert((uint32_t)sceGxmColorSurfaceInit(color,SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR,0,0,0,
        960,544,1024,(void *)(uintptr_t)(UINT64_C(1)<<32))==SCE_GXM_ERROR_INVALID_POINTER);
    assert(!memcmp(color,&copy,sizeof(copy)));
#endif
    assert((uint32_t)sceGxmColorSurfaceInit(NULL,0,0,0,0,1,1,2,(void *)0x1000)==SCE_GXM_ERROR_INVALID_POINTER);
    assert((uint32_t)disable_color(NULL)==SCE_GXM_ERROR_INVALID_POINTER);
    assert(!disable_color(color) && !color_enabled(color) && !color_data(color) && !color_stride(color));
    const SceGxmColorSurfaceInner *color_inner=(const void *)color;
    assert(!color_inner->width && !color_inner->height && !color_inner->outputRegisterSize);
    const SceGxmTextureInner *background=(const void *)&color->backgroundTex;
    assert(gxm_texture_get_type(background)==SCE_GXM_TEXTURE_LINEAR);
    assert(gxm_texture_get_width(background)==1 && gxm_texture_get_height(background)==1);
    assert(gxm_texture_get_format(background)==SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_BGRA);
    assert(!gxm_texture_get_data(background) && background->mip_count==15);
    /* A disabled descriptor stays depth-only even if its stored data is nonzero. */
    ((SceGxmColorSurfaceInner *)color)->data=0x2000;
    assert(!gxm_color_surface_enabled((const void *)color) && color_data(color)==(void *)0x2000);
    assert(!gxm_color_surface_enabled(NULL));
    assert(guarded_color.before==0x12345678 && guarded_color.after==0xabcdef01);
    assert(gxm_color_surface_enabled((const void *)&copy));
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
    float (*get_depth)(const SceGxmDepthStencilSurface *) =
        (float (*)(const SceGxmDepthStencilSurface *))module_get_func_export(0xf76b66bd, 0x269B56BE);
    assert(get_depth);
    for (unsigned value = 0; value < 256; ++value) {
        set_depth(&surface, (float)value / 255.0f);
        set_stencil(&surface, value);
        assert(sceGxmDepthStencilSurfaceGetBackgroundStencil(&surface) == value);
        assert(sceGxmDepthStencilSurfaceGetBackgroundDepth(&surface) == (float)value / 255.0f);
        assert(get_depth(&surface) == (float)value / 255.0f);
        assert(sceGxmDepthStencilSurfaceGetBackgroundMask(&surface));
        sceGxmDepthStencilSurfaceSetBackgroundMask(&surface, false);
        assert(!sceGxmDepthStencilSurfaceGetBackgroundMask(&surface));
        assert(sceGxmDepthStencilSurfaceGetBackgroundStencil(&surface) == value);
        sceGxmDepthStencilSurfaceSetBackgroundMask(&surface, true);
        assert((surface.backgroundControl & ~0xffu) == before.backgroundControl);
        assert(surface.zlsControl == before.zlsControl);
        assert(surface.depthData == before.depthData && surface.stencilData == before.stencilData);
    }
    SceGxmDepthStencilSurface flags_saved=surface;
    for (unsigned load=0;load<2;++load) for (unsigned store=0;store<2;++store) {
        sceGxmDepthStencilSurfaceSetForceLoadMode(&surface,load ? SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_ENABLED : 0);
        sceGxmDepthStencilSurfaceSetForceStoreMode(&surface,store ? SCE_GXM_DEPTH_STENCIL_FORCE_STORE_ENABLED : 0);
        assert(sceGxmDepthStencilSurfaceGetForceLoadMode(&surface)==(load ? 2 : 0));
        assert(sceGxmDepthStencilSurfaceGetForceStoreMode(&surface)==(store ? 4 : 0));
        assert((surface.zlsControl&~6u)==(flags_saved.zlsControl&~6u));
        assert(!memcmp((char *)&surface+4,(char *)&flags_saved+4,sizeof(surface)-4));
    }
    sceGxmDepthStencilSurfaceSetForceLoadMode(NULL,0);sceGxmDepthStencilSurfaceSetForceStoreMode(NULL,0);
    assert(!sceGxmDepthStencilSurfaceGetForceLoadMode(NULL) && !sceGxmDepthStencilSurfaceGetForceStoreMode(NULL));
    const uint32_t nids[] = {0x269B56BE, 0x32F280F0, 0xAAFC062B, 0xF5D3F3E8, 0x85D4DEFE,
                           0xF28A688A, 0xCA9D41D1, 0xA41DB0D6, 0x082200E1, 0xED0F6E25,
                           0x613639FA, 0x0E0EBB57, 0x2DB6026C, 0xF33D9980,
                           0x0C44ACD7, 0x12AAA7AF, 0x2F5CC20C, 0x544AA05A};
    for (unsigned i = 0; i < sizeof(nids) / sizeof(nids[0]); ++i)
        assert(module_get_func_export(0xf76b66bd, nids[i]));
    puts("GXM surface clear values and export checks passed.");
    return 0;
}
