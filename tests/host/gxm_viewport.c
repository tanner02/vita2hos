#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../../source/gxm_viewport.c"

static void close_to(float actual, float expected) { assert(fabsf(actual - expected) < 0.0001f); }

/* Execute the position arithmetic emitted by the pinned GLSLOpenGL translator,
 * then the device's Y-down, zero-to-one native viewport transform. */
static void check_transform(GxmViewportState *state, float x, float y, float z, float w)
{
    DkViewport v;
    GxmViewportUniform u;
    gxm_viewport_translate(state, 960, 544, &v, &u);
    float ndc_x = x / w * u.viewport_flip[0];
    float ndc_y = y / w * u.viewport_flip[1];
    float ndc_z = 2 * (u.z_offset + u.z_scale * z / w) - 1;
    close_to(v.x + v.width * 0.5f * (1 + ndc_x), state->x_offset + state->x_scale * x / w);
    close_to(v.y + v.height * 0.5f * (1 - ndc_y), state->y_offset + state->y_scale * y / w);
    close_to(ndc_z, state->z_offset + state->z_scale * z / w);
}

int main(void)
{
    GxmViewportState state = {0};
    DkViewport v;
    GxmViewportUniform u;
    gxm_viewport_translate(&state, 960, 544, &v, &u);
    assert(v.x == 0 && v.y == 0 && v.width == 960 && v.height == 544);
    close_to(2 * (u.z_offset - u.z_scale) - 1, 0);
    close_to(2 * (u.z_offset + u.z_scale) - 1, 1);
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2) {
            assert(gxm_viewport_set(&state, 400, sx * 80, 200, sy * 40, 0.3f, -0.2f));
            check_transform(&state, 0.6f, -0.8f, 0.4f, 2);
            check_transform(&state, -1, 1, -1, 1);
        }
    state.disabled = true;
    gxm_viewport_translate(&state, 960, 544, &v, &u);
    assert(u.viewport_flag == 0 && v.width == 960 && v.height == 544);
    /* Disabled mode converts screen x/y directly and forces clip z=w. */
    close_to(v.x + v.width * 0.5f * (1 + (2 * 37.0f / u.screen_width - 1)), 37);
    close_to(v.y + v.height * 0.5f * (1 - (1 - 2 * 83.0f / u.screen_height)), 83);
    assert(gxm_viewport_set(&state, 100, 50, 100, -50, 0.5f, 0.5f) && state.disabled);
    GxmViewportState saved = state;
    assert(!gxm_viewport_set(&state, NAN, 1, 2, 3, 4, 5));
    assert(!gxm_viewport_set(&state, 1, FLT_MAX, 2, 3, 4, 5));
    assert(!memcmp(&saved, &state, sizeof(state)));
    state.disabled = false;
    check_transform(&state, 1, -1, 1, 1);
    puts("Viewport screen-coordinate and depth transform checks passed.");
}
