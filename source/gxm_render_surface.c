#include <string.h>
#include "gxm/render_surface.h"
bool dk_surface_create(DkDevice device,dk_surface_t *surface,uint32_t width,uint32_t height,
                       DkImageFormat format,uint32_t flags)
{
    return dk_surface_create_ms(device,surface,width,height,format,flags,DkMsMode_1x);
}
bool dk_surface_create_ms(DkDevice device,dk_surface_t *surface,uint32_t width,uint32_t height,
                          DkImageFormat format,uint32_t flags,DkMsMode mode)
{
    if (!surface) return false;
    *surface=(dk_surface_t){0};
    if (!width || width>4096 || !height || height>4096 ||
        (mode!=DkMsMode_1x && mode!=DkMsMode_2x && mode!=DkMsMode_4x)) return false;
    DkImageLayoutMaker maker;
    DkImageLayout layout;
    dkImageLayoutMakerDefaults(&maker,device);
    maker.flags=flags;maker.format=format;
    maker.type=mode==DkMsMode_1x ? DkImageType_2D : DkImageType_2DMS;
    maker.msMode=mode;
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
    surface->ms_mode=mode;
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
bool dk_render_surfaces_create(DkDevice device,dk_surface_t *color,dk_surface_t *depth,
    dk_surface_t *resolve,uint32_t width,uint32_t height,DkMsMode mode)
{
    if (!color || !depth || !resolve || color==depth || color==resolve || depth==resolve)
        return false;
    *color=(dk_surface_t){0};*depth=(dk_surface_t){0};*resolve=(dk_surface_t){0};
    if (dk_surface_create_ms(device,color,width,height,DkImageFormat_RGBA8_Unorm,
            DkImageFlags_UsageRender|DkImageFlags_Usage2DEngine|DkImageFlags_HwCompression,mode) &&
        dk_surface_create_ms(device,depth,width,height,DkImageFormat_ZF32_X24S8,
            DkImageFlags_UsageRender|DkImageFlags_Usage2DEngine,mode) &&
        (mode==DkMsMode_1x || dk_surface_create(device,resolve,width,height,
            DkImageFormat_RGBA8_Unorm,DkImageFlags_Usage2DEngine))) return true;
    dk_surface_destroy(resolve);dk_surface_destroy(depth);dk_surface_destroy(color);
    return false;
}
const DkImage *dk_surface_resolve_color(DkCmdBuf cmd,const dk_surface_t *color,
    const dk_surface_t *resolve)
{
    if (!cmd || !color || !color->memblock || !color->width || !color->height) return NULL;
    if (color->ms_mode!=DkMsMode_1x &&
        ((color->ms_mode!=DkMsMode_2x && color->ms_mode!=DkMsMode_4x) ||
         !resolve || !resolve->memblock || resolve->ms_mode!=DkMsMode_1x ||
         color->width!=resolve->width || color->height!=resolve->height)) return NULL;
    /* The 2D engine must wait for rendering before reading sample storage. */
    dkCmdBufBarrier(cmd,DkBarrier_Full,DkInvalidateFlags_Image);
    if (color->ms_mode==DkMsMode_1x) return &color->image;
    dkCmdBufResolveImage(cmd,&color->view,&resolve->view);
    dkCmdBufBarrier(cmd,DkBarrier_Full,DkInvalidateFlags_Image);
    return &resolve->image;
}
