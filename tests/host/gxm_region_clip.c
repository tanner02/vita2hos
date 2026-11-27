#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "../../source/gxm_region_clip.c"

static void coverage(GxmRegionClip clip, unsigned left, unsigned top,
                      unsigned right, unsigned bottom)
{
    DkScissor out[4];
    unsigned count = gxm_region_clip_scissors(&clip, 97, 73, out);
    assert(count <= 4);
    for (unsigned y = 0; y < 73; ++y) for (unsigned x = 0; x < 97; ++x) {
        unsigned hits = 0;
        for (unsigned i = 0; i < count; ++i) {
            assert(out[i].width && out[i].height);
            assert(out[i].x + out[i].width <= 97 && out[i].y + out[i].height <= 73);
            hits += x >= out[i].x && y >= out[i].y &&
                    x < out[i].x + out[i].width && y < out[i].y + out[i].height;
        }
        bool inside = x >= left && x < right && y >= top && y < bottom;
        bool expected = clip.mode == SCE_GXM_REGION_CLIP_NONE ||
            (clip.mode == SCE_GXM_REGION_CLIP_OUTSIDE && inside) ||
            (clip.mode == SCE_GXM_REGION_CLIP_INSIDE && !inside);
        assert(hits == (unsigned)expected);
    }
}

int main(void)
{
    const struct { uint32_t min, max, begin, end; } cases[] = {
        {0, 31, 0, 32}, {1, 32, 0, 32}, {33, 63, 32, 64},
        {32, 96, 32, 96}, {65, UINT32_MAX, 64, 97},
        {128, UINT32_MAX, 97, 97}, {70, 20, 0, 0}, {0, 0, 0, 0},
    };
    const SceGxmRegionClipMode modes[] = {SCE_GXM_REGION_CLIP_NONE,
        SCE_GXM_REGION_CLIP_ALL, SCE_GXM_REGION_CLIP_OUTSIDE, SCE_GXM_REGION_CLIP_INSIDE};
    GxmRegionClip clip = {0};
    for (unsigned m = 0; m < 4; ++m) for (unsigned i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
        assert(!gxm_region_clip_set(&clip, modes[m], cases[i].min, 33, cases[i].max, 63));
        coverage(clip, cases[i].begin, 32, cases[i].end, 64);
    }
    for (unsigned m = 0; m < 4; ++m) {
        assert(!gxm_region_clip_set(&clip, modes[m], 0, 0, UINT32_MAX, UINT32_MAX));
        coverage(clip, 0, 0, 97, 73);
        assert(!gxm_region_clip_set(&clip, modes[m], 0, 128, UINT32_MAX, UINT32_MAX));
        coverage(clip, 0, 0, 0, 0);
    }
    GxmRegionClip saved = clip;
    assert((uint32_t)gxm_region_clip_set(&clip, (SceGxmRegionClipMode)1, 0, 0, 1, 1)
            == SCE_GXM_ERROR_INVALID_VALUE);
    assert(!memcmp(&saved, &clip, sizeof(clip)));
    DkScissor out[4];
    assert(!gxm_region_clip_scissors(&clip, 0, 73, out));
    puts("Region clip coverage and non-overlap checks passed.");
}
