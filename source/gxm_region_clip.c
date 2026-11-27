#include "gxm/region_clip.h"

int gxm_region_clip_set(GxmRegionClip *clip, SceGxmRegionClipMode mode,
                        uint32_t x_min, uint32_t y_min, uint32_t x_max, uint32_t y_max)
{
    if (!clip) return SCE_GXM_ERROR_INVALID_POINTER;
    if (mode != SCE_GXM_REGION_CLIP_NONE && mode != SCE_GXM_REGION_CLIP_ALL &&
        mode != SCE_GXM_REGION_CLIP_OUTSIDE && mode != SCE_GXM_REGION_CLIP_INSIDE)
        return SCE_GXM_ERROR_INVALID_VALUE;
    *clip = (GxmRegionClip){mode, x_min, y_min, x_max, y_max};
    return 0;
}

static uint32_t tile_end(uint32_t maximum, uint32_t tile, uint32_t limit)
{
    /* Match the pinned renderer's upper-bound alignment without uint32 wrap. */
    uint64_t end = ((uint64_t)maximum + tile - 1) & ~(uint64_t)(tile - 1);
    return end < limit ? (uint32_t)end : limit;
}

static void append(DkScissor out[4], unsigned *count,
                    uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    if (width && height) out[(*count)++] = (DkScissor){x, y, width, height};
}

unsigned gxm_region_clip_scissors(const GxmRegionClip *clip, uint32_t width,
                                  uint32_t height, DkScissor out[4])
{
    unsigned count = 0;
    if (!width || !height || clip->mode == SCE_GXM_REGION_CLIP_ALL) return 0;
    if (clip->mode == SCE_GXM_REGION_CLIP_NONE) {
        append(out, &count, 0, 0, width, height);
        return count;
    }
    uint32_t left = clip->x_min & ~(SCE_GXM_TILE_SIZEX - 1);
    uint32_t top = clip->y_min & ~(SCE_GXM_TILE_SIZEY - 1);
    if (left > width) left = width;
    if (top > height) top = height;
    uint32_t right = tile_end(clip->x_max, SCE_GXM_TILE_SIZEX, width);
    uint32_t bottom = tile_end(clip->y_max, SCE_GXM_TILE_SIZEY, height);
    if (clip->x_min > clip->x_max || clip->y_min > clip->y_max ||
        right <= left || bottom <= top) {
        if (clip->mode == SCE_GXM_REGION_CLIP_INSIDE)
            append(out, &count, 0, 0, width, height);
        return count;
    }
    if (clip->mode == SCE_GXM_REGION_CLIP_OUTSIDE) {
        append(out, &count, left, top, right - left, bottom - top);
    } else if (clip->mode == SCE_GXM_REGION_CLIP_INSIDE) {
        /* Disjoint bands preserve blending/depth/stencil ordering per pixel. */
        append(out, &count, 0, 0, width, top);
        append(out, &count, 0, bottom, width, height - bottom);
        append(out, &count, 0, top, left, bottom - top);
        append(out, &count, right, top, width - right, bottom - top);
    }
    return count;
}
