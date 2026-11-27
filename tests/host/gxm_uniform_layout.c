#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../source/gxm_uniform_layout.c"

int main(void)
{
    uint32_t counts[15] = {3, 1, 0, 0, 5};
    GxmUniformLayout layout;
    assert(gxm_uniform_layout_build(counts, &layout));
    assert(layout.bytes[0] == 12 && layout.bytes[1] == 4 && layout.bytes[4] == 20);
    assert(layout.offsets[0] == 0 && layout.offsets[1] == 16 && layout.offsets[4] == 32);
    assert(layout.size == 64);
    counts[0] = UINT32_MAX;
    memset(&layout, 0xff, sizeof(layout));
    assert(!gxm_uniform_layout_build(counts, &layout));
    GxmUniformLayout empty = {0};
    assert(!memcmp(&layout, &empty, sizeof(layout)));
    memset(counts, 0, sizeof(counts));
    counts[0] = counts[1] = UINT32_MAX / 8;
    assert(!gxm_uniform_layout_build(counts, &layout));
    assert(!memcmp(&layout, &empty, sizeof(layout)));
    memset(counts, 0, sizeof(counts));
    assert(gxm_uniform_layout_build(counts, &layout) && !layout.size);
    assert(!gxm_uniform_layout_build(NULL, &layout));
    assert(!gxm_uniform_layout_build(counts, NULL));
    puts("Uniform container layout checks passed.");
}
