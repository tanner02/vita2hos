#include <string.h>

#include "gxm/texture_layout.h"

#define R DkImageSwizzle_Red
#define G DkImageSwizzle_Green
#define B DkImageSwizzle_Blue
#define A DkImageSwizzle_Alpha
#define Z DkImageSwizzle_Zero
#define O DkImageSwizzle_One

static const DkImageSwizzle swizzle1[8][4] = {
    {R,Z,Z,O}, {R,Z,Z,Z}, {R,O,O,O}, {R,R,R,R},
    {R,R,R,Z}, {R,R,R,O}, {Z,Z,Z,R}, {O,O,O,R},
};
static const DkImageSwizzle swizzle2[6][4] = {
    {R,G,Z,O}, {R,G,Z,Z}, {R,R,R,G}, {G,G,G,R}, {R,G,R,G}, {G,R,Z,Z},
};
static const DkImageSwizzle swizzle4[8][4] = {
    {R,G,B,A}, {B,G,R,A}, {A,B,G,R}, {G,B,A,R},
    {R,G,B,O}, {B,G,R,O}, {A,B,G,O}, {G,B,A,O},
};

#undef R
#undef G
#undef B
#undef A
#undef Z
#undef O

int gxm_texture_get_linear_layout(const SceGxmTextureInner *texture, GxmTextureLayout *layout)
{
    if (!texture || !layout)
        return SCE_GXM_ERROR_INVALID_POINTER;
    const uint32_t type = (uint32_t)texture->type << 29;
    if (type != SCE_GXM_TEXTURE_LINEAR && type != SCE_GXM_TEXTURE_LINEAR_STRIDED)
        return SCE_GXM_ERROR_UNSUPPORTED;
    if (texture->gamma_mode != 0)
        return SCE_GXM_ERROR_UNSUPPORTED;
    if (texture->mag_filter > SCE_GXM_TEXTURE_FILTER_LINEAR ||
        (type != SCE_GXM_TEXTURE_LINEAR_STRIDED &&
         texture->min_filter > SCE_GXM_TEXTURE_FILTER_LINEAR))
        return SCE_GXM_ERROR_UNSUPPORTED;
    if (texture->uaddr_mode == SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER ||
        texture->uaddr_mode >= SCE_GXM_TEXTURE_ADDR_CLAMP_IGNORE_BORDER ||
        texture->vaddr_mode == SCE_GXM_TEXTURE_ADDR_REPEAT_IGNORE_BORDER ||
        texture->vaddr_mode >= SCE_GXM_TEXTURE_ADDR_CLAMP_IGNORE_BORDER)
        return SCE_GXM_ERROR_UNSUPPORTED;

    GxmTextureLayout result = {0};
    uint32_t bytes, components;
    const uint32_t base = ((uint32_t)texture->base_format << 24) |
                          ((uint32_t)texture->format0 << 31);
    switch (base) {
#define FORMAT(gxm, dk, bpp, count) \
    case SCE_GXM_TEXTURE_BASE_FORMAT_##gxm: \
        result.format = DkImageFormat_##dk; bytes = bpp; components = count; break
    FORMAT(U8, R8_Unorm, 1, 1);
    FORMAT(S8, R8_Snorm, 1, 1);
    FORMAT(U16, R16_Unorm, 2, 1);
    FORMAT(S16, R16_Snorm, 2, 1);
    FORMAT(F16, R16_Float, 2, 1);
    FORMAT(F32, R32_Float, 4, 1);
    FORMAT(U8U8, RG8_Unorm, 2, 2);
    FORMAT(S8S8, RG8_Snorm, 2, 2);
    FORMAT(U16U16, RG16_Unorm, 4, 2);
    FORMAT(S16S16, RG16_Snorm, 4, 2);
    FORMAT(F16F16, RG16_Float, 4, 2);
    FORMAT(F32F32, RG32_Float, 8, 2);
    FORMAT(U8U8U8U8, RGBA8_Unorm, 4, 4);
    FORMAT(S8S8S8S8, RGBA8_Snorm, 4, 4);
    FORMAT(U16U16U16U16, RGBA16_Unorm, 8, 4);
    FORMAT(S16S16S16S16, RGBA16_Snorm, 8, 4);
    FORMAT(F16F16F16F16, RGBA16_Float, 8, 4);
#undef FORMAT
    default:
        return SCE_GXM_ERROR_UNSUPPORTED;
    }

    const uint32_t swizzle = texture->swizzle_format;
    if (components == 2) {
        if (swizzle >= 6)
            return SCE_GXM_ERROR_INVALID_VALUE;
        memcpy(result.swizzle, swizzle2[swizzle], sizeof(result.swizzle));
    } else {
        memcpy(result.swizzle, components == 1 ? swizzle1[swizzle] : swizzle4[swizzle],
               sizeof(result.swizzle));
    }

    result.width = texture->width + 1;
    result.height = texture->height + 1;
    if (type == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        result.pitch = ((texture->mip_filter | (texture->min_filter << 1) |
                        (texture->mip_count << 3) | (texture->lod_bias << 7)) + 1) * 4;
        if (result.pitch < result.width * bytes)
            return SCE_GXM_ERROR_INVALID_VALUE;
    } else {
        result.pitch = ((result.width + 7) & ~7U) * bytes;
    }
    /* Maxwell pitch-linear texture descriptors encode stride in 32-byte units.
     * Smaller pitches need a staging copy, not silently rounded source rows. */
    if (result.pitch % 32 != 0)
        return SCE_GXM_ERROR_UNSUPPORTED;
    result.size = result.pitch * result.height;
    *layout = result;
    return 0;
}
