#ifndef GXM_VIEWPORT_H
#define GXM_VIEWPORT_H
#include <stdbool.h>
#include <deko3d.h>

typedef struct {
    float x_offset, x_scale, y_offset, y_scale, z_offset, z_scale;
    bool configured, disabled;
} GxmViewportState;

/* Matches the pinned Vita3K vertex render-info uniform layout. */
typedef struct {
    float viewport_flip[4];
    float viewport_flag, screen_width, screen_height, z_offset, z_scale;
} GxmViewportUniform;

bool gxm_viewport_set(GxmViewportState *state, float x_offset, float x_scale,
                      float y_offset, float y_scale, float z_offset, float z_scale);
void gxm_viewport_translate(const GxmViewportState *state, float width, float height,
                            DkViewport *viewport, GxmViewportUniform *uniform);
#endif
