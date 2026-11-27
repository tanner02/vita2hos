#include <math.h>
#include "gxm/viewport.h"

bool gxm_viewport_set(GxmViewportState *state, float x_offset, float x_scale,
                      float y_offset, float y_scale, float z_offset, float z_scale)
{
    if (!state || !isfinite(x_offset) || !isfinite(x_scale) ||
        !isfinite(y_offset) || !isfinite(y_scale) || !isfinite(z_offset) ||
        !isfinite(z_scale) || !isfinite(x_offset - fabsf(x_scale)) ||
        !isfinite(y_offset - fabsf(y_scale)) || !isfinite(2 * fabsf(x_scale)) ||
        !isfinite(2 * fabsf(y_scale)))
        return false;
    bool disabled = state->disabled;
    *state = (GxmViewportState){x_offset, x_scale, y_offset, y_scale, z_offset, z_scale,
                                true, disabled};
    return true;
}

void gxm_viewport_translate(const GxmViewportState *state, float width, float height,
                            DkViewport *viewport, GxmViewportUniform *uniform)
{
    float xo = width * 0.5f, xs = xo, yo = height * 0.5f, ys = -yo;
    float zo = 0.5f, zs = 0.5f;
    if (state->configured && !state->disabled) {
        xo = state->x_offset; xs = state->x_scale;
        yo = state->y_offset; ys = state->y_scale;
        zo = state->z_offset; zs = state->z_scale;
    }
    *viewport = (DkViewport){xo - fabsf(xs), yo - fabsf(ys),
                             2 * fabsf(xs), 2 * fabsf(ys), 0, 1};
    *uniform = (GxmViewportUniform){
        /* Native YAxisPointsDown uses a negative Y viewport scale. */
        .viewport_flip = {xs < 0 ? -1 : 1, ys > 0 ? -1 : 1, 1, 1},
        .viewport_flag = state->disabled ? 0 : 1,
        .screen_width = width, .screen_height = height,
        /* GLSLOpenGL translation emits 2*(offset+scale*z/w)-1.
         * Compensate for that conversion: the device consumes [0,1] depth. */
        .z_offset = 0.5f * zo + 0.5f, .z_scale = 0.5f * zs,
    };
}
