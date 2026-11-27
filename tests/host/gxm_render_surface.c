#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../source/gxm_render_surface.c"
static uint64_t image_size=12345;
static uint32_t image_alignment=512;
static bool fail;
static unsigned live,initialized;
void dkImageLayoutInitialize(DkImageLayout *layout,const DkImageLayoutMaker *maker)
{ assert(maker->dimensions[0]==64 && maker->dimensions[1]==32);*layout=(DkImageLayout){image_size,image_alignment}; }
uint64_t dkImageLayoutGetSize(const DkImageLayout *layout) { return layout->size; }
uint32_t dkImageLayoutGetAlignment(const DkImageLayout *layout) { return layout->alignment; }
DkMemBlock dkMemBlockCreate(const DkMemBlockMaker *maker)
{
    assert(maker->size==16384 && maker->flags==(DkMemBlockFlags_GpuCached|DkMemBlockFlags_Image));
    if (fail) return NULL;
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
    puts("Render surfaces: image sizing, page alignment, failed allocation before initialization, overflow and idempotent teardown passed.");
}
