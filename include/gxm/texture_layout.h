#ifndef GXM_TEXTURE_LAYOUT_H
#define GXM_TEXTURE_LAYOUT_H

#include <deko3d.h>
#include "gxm/texture.h"

typedef struct {
    DkImageFormat format;
    DkImageSwizzle swizzle[4];
    uint32_t width, height, pitch, size;
    uint32_t source_type, source_width, source_height, source_pitch, source_size;
    uint32_t bytes_per_pixel;
} GxmTextureLayout;

int gxm_texture_get_linear_layout(const SceGxmTextureInner *texture, GxmTextureLayout *layout);
/* Layout must come from the function above; source and destination must not overlap. */
int gxm_texture_pack_linear(const GxmTextureLayout *layout, const void *source, size_t source_size,
                            void *destination, size_t destination_size);

#endif
