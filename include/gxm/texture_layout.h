#ifndef GXM_TEXTURE_LAYOUT_H
#define GXM_TEXTURE_LAYOUT_H

#include <deko3d.h>
#include "gxm/texture.h"

typedef struct {
    DkImageFormat format;
    DkImageSwizzle swizzle[4];
    uint32_t width, height, pitch, size;
} GxmTextureLayout;

int gxm_texture_get_linear_layout(const SceGxmTextureInner *texture, GxmTextureLayout *layout);

#endif
