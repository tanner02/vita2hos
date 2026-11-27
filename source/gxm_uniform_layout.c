#include <stddef.h>
#include <string.h>
#include "gxm/uniform_layout.h"

bool gxm_uniform_layout_build(const uint32_t counts[GXM_UNIFORM_LAYOUT_SLOTS],
                              GxmUniformLayout *layout)
{
    if (!layout) return false;
    memset(layout, 0, sizeof(*layout));
    if (!counts) return false;
    GxmUniformLayout result = {{0}, {0}, 0};
    uint64_t offset = 0;
    for (unsigned i = 0; i < GXM_UNIFORM_LAYOUT_SLOTS; ++i) {
        uint64_t bytes = (uint64_t)counts[i] * sizeof(float);
        uint64_t padded = (bytes + 15) & ~(uint64_t)15;
        if (bytes > UINT32_MAX || offset + padded > UINT32_MAX) return false;
        result.bytes[i] = (uint32_t)bytes;
        result.offsets[i] = (uint32_t)offset;
        offset += padded;
    }
    result.size = (uint32_t)offset;
    *layout = result;
    return true;
}
