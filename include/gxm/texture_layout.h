#ifndef GXM_TEXTURE_LAYOUT_H
#define GXM_TEXTURE_LAYOUT_H

#include <deko3d.h>
#include <stdbool.h>
#include "gxm/texture.h"

typedef struct {
    DkImageFormat format;
    DkImageSwizzle swizzle[4];
    uint32_t width, height, pitch, size;
    uint32_t source_type, source_width, source_height, source_pitch, source_size;
    uint32_t bytes_per_pixel;
    uint32_t block_bytes; /* Nonzero for 4x4 BC blocks; source dimensions are block counts. */
} GxmTextureLayout;

#define GXM_TEXTURE_MAX_MIP_LEVELS 13
typedef struct {
    uint32_t count, source_size;
    uint32_t offsets[GXM_TEXTURE_MAX_MIP_LEVELS];
    GxmTextureLayout levels[GXM_TEXTURE_MAX_MIP_LEVELS];
} GxmTextureMipChain;

/* Source metadata for supported 2D layouts. This does not upload
 * the chain or enable mip filtering in the renderer. */
int gxm_texture_get_mip_chain(const SceGxmTextureInner *texture, GxmTextureMipChain *chain);

int gxm_texture_get_linear_layout(const SceGxmTextureInner *texture, GxmTextureLayout *layout);
/* Native tiled images with six to eight physical rows need an explicit one-GOB
 * base tile in the installed SDK, so level offsets and transfer tiling agree. */
bool gxm_texture_native_one_gob(const GxmTextureLayout *layout);
/* Layout must come from the function above; source and destination must not overlap. */
int gxm_texture_pack_linear(const GxmTextureLayout *layout, const void *source, size_t source_size,
                            void *destination, size_t destination_size);

#endif
