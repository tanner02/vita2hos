#ifndef GXM_REGION_CLIP_H
#define GXM_REGION_CLIP_H
#include <stdint.h>
#include <psp2/gxm.h>
#include <deko3d.h>

typedef struct {
    SceGxmRegionClipMode mode;
    uint32_t x_min, y_min, x_max, y_max;
} GxmRegionClip;

int gxm_region_clip_set(GxmRegionClip *clip, SceGxmRegionClipMode mode,
                        uint32_t x_min, uint32_t y_min, uint32_t x_max, uint32_t y_max);
/* Returns disjoint native scissors. INSIDE uses up to four draw passes. */
unsigned gxm_region_clip_scissors(const GxmRegionClip *clip, uint32_t width,
                                  uint32_t height, DkScissor scissors[4]);
#endif
