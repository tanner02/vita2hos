#include <string.h>
#include "gxm/render_surface.h"
bool dk_surface_create(DkDevice device,dk_surface_t *surface,uint32_t width,uint32_t height,
                       DkImageFormat format,uint32_t flags)
{
    if (!surface) return false;
    *surface=(dk_surface_t){0};
    if (!width || width>4096 || !height || height>4096) return false;
    DkImageLayoutMaker maker;
    DkImageLayout layout;
    dkImageLayoutMakerDefaults(&maker,device);
    maker.flags=flags;maker.format=format;
    maker.dimensions[0]=width;maker.dimensions[1]=height;
    dkImageLayoutInitialize(&layout,&maker);
    uint64_t bytes=dkImageLayoutGetSize(&layout);
    uint32_t alignment=dkImageLayoutGetAlignment(&layout);
    if (!alignment || (alignment&(alignment-1))) return false;
    if (alignment<DK_MEMBLOCK_ALIGNMENT) alignment=DK_MEMBLOCK_ALIGNMENT;
    if (!bytes || !alignment || (alignment&(alignment-1)) || bytes>UINT32_MAX-(alignment-1))
        return false;
    uint32_t size=((uint32_t)bytes+alignment-1)&~(alignment-1);
    DkMemBlockMaker memory;
    dkMemBlockMakerDefaults(&memory,device,size);
    memory.flags=DkMemBlockFlags_GpuCached|DkMemBlockFlags_Image;
    DkMemBlock block=dkMemBlockCreate(&memory);
    if (!block) return false;
    surface->size=bytes;surface->width=width;surface->height=height;surface->memblock=block;
    dkImageInitialize(&surface->image,&layout,block,0);
    dkImageViewDefaults(&surface->view,&surface->image);
    return true;
}
void dk_surface_destroy(dk_surface_t *surface)
{
    if (!surface) return;
    if (surface->memblock) dkMemBlockDestroy(surface->memblock);
    memset(surface,0,sizeof(*surface));
}
