#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../source/gxm_render_surface.c"
static uint64_t image_size=12345;
static uint32_t image_alignment=512;
static bool fail;
static unsigned live,initialized;
static unsigned allocation_calls,fail_on_call,layout_count;
static DkImageLayoutMaker layouts[3];
static unsigned command_count,resolve_count;
static const DkImage *expected_source,*expected_resolve;
void dkCmdBufBarrier(DkCmdBuf cmd,DkBarrier barrier,uint32_t flags)
{
    assert(cmd && barrier==DkBarrier_Full && flags==DkInvalidateFlags_Image);
    assert(command_count==0 || command_count==2);++command_count;
}
void dkCmdBufResolveImage(DkCmdBuf cmd,const DkImageView *src,const DkImageView *dst)
{
    assert(cmd && command_count==1 && src->pImage==expected_source && dst->pImage==expected_resolve);
    ++command_count;++resolve_count;
}
void dkImageLayoutInitialize(DkImageLayout *layout,const DkImageLayoutMaker *maker)
{
    assert(maker->dimensions[0]==64 && maker->dimensions[1]==32);
    layouts[layout_count++%3]=*maker;
    *layout=(DkImageLayout){image_size,image_alignment};
}
uint64_t dkImageLayoutGetSize(const DkImageLayout *layout) { return layout->size; }
uint32_t dkImageLayoutGetAlignment(const DkImageLayout *layout) { return layout->alignment; }
DkMemBlock dkMemBlockCreate(const DkMemBlockMaker *maker)
{
    assert(maker->size==16384 && maker->flags==(DkMemBlockFlags_GpuCached|DkMemBlockFlags_Image));
    ++allocation_calls;
    if (fail || (fail_on_call && allocation_calls==fail_on_call)) return NULL;
    ++live;return malloc(1);
}
void dkMemBlockDestroy(DkMemBlock block) { assert(block && live);--live;free(block); }
void dkImageInitialize(DkImage *image,const DkImageLayout *layout,DkMemBlock block,uint32_t offset)
{ assert(block && !offset && layout->size==12345);image->unused=17;++initialized; }
void dkImageViewDefaults(DkImageView *view,const DkImage *image) { *view=(DkImageView){.pImage=image}; }
int main(void)
{
    dk_surface_t surface={0};
    assert(dk_surface_create(NULL,&surface,64,32,DkImageFormat_RGBA8_Unorm,0));
    assert(live==1 && initialized==1 && surface.size==12345 && surface.view.pImage==&surface.image);
    dk_surface_destroy(&surface);assert(!live && !surface.memblock && !surface.width);
    dk_surface_destroy(&surface);assert(!live);
    fail=true;
    assert(!dk_surface_create(NULL,&surface,64,32,DkImageFormat_RGBA8_Unorm,0));
    assert(!live && initialized==1 && !surface.memblock);
    fail=false;image_size=UINT64_MAX;
    assert(!dk_surface_create(NULL,&surface,64,32,DkImageFormat_RGBA8_Unorm,0) && !live);
    image_size=12345;image_alignment=0;
    assert(!dk_surface_create(NULL,&surface,64,32,DkImageFormat_RGBA8_Unorm,0) && !live);
    image_alignment=3;
    assert(!dk_surface_create(NULL,&surface,64,32,DkImageFormat_RGBA8_Unorm,0) && !live);
    image_size=12345;image_alignment=8192;
    assert(dk_surface_create(NULL,&surface,64,32,DkImageFormat_RGBA8_Unorm,0));
    dk_surface_destroy(&surface);assert(!live);
    assert(!dk_surface_create(NULL,&surface,0,32,DkImageFormat_RGBA8_Unorm,0));
    assert(!dk_surface_create(NULL,NULL,64,32,DkImageFormat_RGBA8_Unorm,0));
    dk_surface_destroy(NULL);
    image_alignment=512;image_size=12345;
    for (DkMsMode mode=DkMsMode_1x;mode<=DkMsMode_4x;++mode) {
        layout_count=0;
        assert(dk_surface_create_ms(NULL,&surface,64,32,DkImageFormat_RGBA8_Unorm,0,mode));
        assert(layout_count==1 && layouts[0].msMode==mode &&
            layouts[0].type==(mode==DkMsMode_1x ? DkImageType_2D : DkImageType_2DMS) &&
            surface.ms_mode==mode && surface.view.pImage==&surface.image);
        dk_surface_destroy(&surface);
        dk_surface_t color={0},depth={0},resolve={0};
        layout_count=allocation_calls=0;
        assert(dk_render_surfaces_create(NULL,&color,&depth,&resolve,64,32,mode));
        const unsigned expected=mode==DkMsMode_1x ? 2 : 3;
        assert(live==expected && layout_count==expected && allocation_calls==expected);
        assert(layouts[0].msMode==mode && layouts[1].msMode==mode &&
            layouts[0].format==DkImageFormat_RGBA8_Unorm && layouts[1].format==DkImageFormat_ZF32_X24S8);
        assert(color.view.pImage==&color.image && depth.view.pImage==&depth.image);
        if (expected==3)
            assert(layouts[2].msMode==DkMsMode_1x && layouts[2].type==DkImageType_2D &&
                resolve.ms_mode==DkMsMode_1x && resolve.view.pImage==&resolve.image);
        else assert(!resolve.memblock);
        command_count=resolve_count=0;
        expected_source=&color.image;expected_resolve=&resolve.image;
        assert(dk_surface_resolve_color((DkCmdBuf)1,&color,&resolve)==
            (mode==DkMsMode_1x ? &color.image : &resolve.image));
        assert(command_count==(mode==DkMsMode_1x ? 1 : 3) &&
               resolve_count==(mode==DkMsMode_1x ? 0 : 1));
        if (mode!=DkMsMode_1x) {
            command_count=resolve_count=0;
            resolve.width=32;
            assert(!dk_surface_resolve_color((DkCmdBuf)1,&color,&resolve));
            resolve.width=64;resolve.ms_mode=DkMsMode_2x;
            assert(!dk_surface_resolve_color((DkCmdBuf)1,&color,&resolve));
            resolve.ms_mode=DkMsMode_1x;
            assert(!dk_surface_resolve_color((DkCmdBuf)1,&color,NULL));
            assert(!command_count && !resolve_count);
        }
        dk_surface_destroy(&resolve);dk_surface_destroy(&depth);dk_surface_destroy(&color);
        assert(!live);
        for (unsigned step=1;step<=expected;++step) {
            allocation_calls=layout_count=0;fail_on_call=step;
            assert(!dk_render_surfaces_create(NULL,&color,&depth,&resolve,64,32,mode));
            assert(!live && allocation_calls==step && !color.memblock && !depth.memblock && !resolve.memblock);
            assert(!color.width && !depth.width && !resolve.width);
        }
        fail_on_call=0;
    }
    const unsigned before=initialized;
    assert(!dk_surface_create_ms(NULL,&surface,64,32,DkImageFormat_RGBA8_Unorm,0,DkMsMode_8x));
    assert(!dk_surface_create_ms(NULL,&surface,64,32,DkImageFormat_RGBA8_Unorm,0,(DkMsMode)-1));
    assert(initialized==before && !surface.memblock && !live);
    puts("Render surfaces: image sizing, page alignment, failed allocation before initialization, overflow and idempotent teardown passed.");
}
