#include "deko_utils.h"

#include "display/display_to_dk.h"
#include "gxm/gxm_to_dk.h"
#include "modules/SceSysmem.h"

void dk_surface_create(DkDevice device, dk_surface_t *surface, uint32_t width, uint32_t height,
                       DkImageFormat format, uint32_t flags)
{
    DkImageLayoutMaker maker;
    DkImageLayout layout;
    uint32_t alignment;

    dkImageLayoutMakerDefaults(&maker, device);
    maker.flags = flags;
    maker.format = format;
    maker.dimensions[0] = width;
    maker.dimensions[1] = height;
    dkImageLayoutInitialize(&layout, &maker);

    alignment = dkImageLayoutGetAlignment(&layout);
    surface->size = dkImageLayoutGetSize(&layout);
    surface->width = width;
    surface->height = height;
    surface->memblock = dk_alloc_memblock(device, ALIGN(surface->size, alignment),
                                          DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image);
    dkImageInitialize(&surface->image, &layout, surface->memblock, 0);
    dkImageViewDefaults(&surface->view, &surface->image);
}

void dk_surface_destroy(dk_surface_t *surface)
{
    dkMemBlockDestroy(surface->memblock);
}

void dk_cmdbuf_copy_image(DkCmdBuf cmdbuf, DkImage const *src_image, uint32_t src_width,
                          uint32_t src_height, DkImage const *dst_image, uint32_t dst_width,
                          uint32_t dst_height)
{
    DkImageView src_view, dst_view;
    DkImageRect src_rect, dst_rect;

    src_rect.x = src_rect.y = src_rect.z = 0;
    src_rect.width = src_width;
    src_rect.height = src_height;
    src_rect.depth = 1;

    dst_rect.x = dst_rect.y = dst_rect.z = 0;
    dst_rect.width = dst_width;
    dst_rect.height = dst_height;
    dst_rect.depth = 1;

    dkImageViewDefaults(&src_view, src_image);
    dkImageViewDefaults(&dst_view, dst_image);
    dkCmdBufBlitImage(cmdbuf, &src_view, &src_rect, &dst_view, &dst_rect, 0, 1);
}

bool dk_image_for_gxm_color_surface(DkDevice device, DkImage *image,
                                    const SceGxmColorSurfaceInner *surface)
{
    DkImageLayoutMaker maker;
    DkImageLayout layout;
    if (!surface || surface->disabled || !surface->data)
        return false;
    /* Vita tiled/swizzled storage is not native NVIDIA block-linear storage.
     * Those layouts require an explicit conversion, not a native image view. */
    if (surface->surfaceType != SCE_GXM_COLOR_SURFACE_LINEAR ||
        surface->colorFormat != SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR)
        return false;
    VitaMemBlockInfo *info = SceSysmem_get_vita_memblock_info_for_addr(surface->data);
    if (!info || !info->dk_memblock) return false;
    DkMemBlock block = info->dk_memblock;
    uintptr_t address = (uintptr_t)surface->data, base = (uintptr_t)info->base;
    if (address < base || address - base >= info->size) return false;
    uint32_t offset = address - base;
    uint32_t pitch;
    if (gxm_color_surface_linear_pitch(surface->width, surface->height, surface->strideInPixels,
                                        info->size - offset, &pitch))
        return false;
    dkImageLayoutMakerDefaults(&maker, device);
    maker.flags =
        gxm_color_surface_type_to_dk_image_flags(surface->surfaceType) | DkImageFlags_Usage2DEngine;
    maker.format = gxm_color_format_to_dk_image_format(surface->colorFormat);
    maker.dimensions[0] = surface->width;
    maker.dimensions[1] = surface->height;
    maker.pitchStride = pitch;
    dkImageLayoutInitialize(&layout, &maker);
    if (dkImageLayoutGetSize(&layout) > info->size - offset ||
        offset % dkImageLayoutGetAlignment(&layout))
        return false;
    dkImageInitialize(image, &layout, block, offset);

    return true;
}

bool dk_image_for_gxm_ds_surface(DkDevice device, DkImage *image, uint32_t width, uint32_t height,
                                 const SceGxmDepthStencilSurface *surface)
{
    DkImageLayoutMaker maker;
    DkImageLayout layout;
    DkMemBlock block = SceSysmem_get_dk_memblock_for_addr(surface->depthData);

    if (!block)
        return false;

    dkImageLayoutMakerDefaults(&maker, device);
    maker.flags = gxm_ds_surface_type_to_dk_image_flags(gxm_ds_surface_get_type(surface)) |
                  DkImageFlags_Usage2DEngine;
    maker.format = gxm_ds_format_to_dk_image_format(gxm_ds_surface_get_format(surface));
    maker.dimensions[0] = width;
    maker.dimensions[1] = height;
    dkImageLayoutInitialize(&layout, &maker);
    dkImageInitialize(image, &layout, block,
                      dk_memblock_cpu_addr_offset(block, surface->depthData));

    return true;
}

bool dk_image_for_existing_framebuffer(DkDevice device, DkImage *image, const void *addr,
                                       uint32_t width, uint32_t height, uint32_t stride,
                                       SceDisplayPixelFormat pixelfmt)
{
    DkImageLayoutMaker maker;
    DkImageLayout layout;
    DkMemBlock block = SceSysmem_get_dk_memblock_for_addr(addr);

    if (!block)
        return false;

    dkImageLayoutMakerDefaults(&maker, device);
    maker.flags = DkImageFlags_PitchLinear | DkImageFlags_Usage2DEngine;
    maker.format = display_pixelformat_to_dk_image_format(pixelfmt);
    maker.dimensions[0] = width;
    maker.dimensions[1] = height;
    maker.pitchStride = stride * display_pixelformat_bytes_per_pixel(pixelfmt);
    dkImageLayoutInitialize(&layout, &maker);
    dkImageInitialize(image, &layout, block, dk_memblock_cpu_addr_offset(block, addr));

    return true;
}
