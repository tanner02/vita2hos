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

static uint32_t next_power_of_two(uint32_t value)
{
    uint32_t result = 1;
    while (result < value)
        result <<= 1;
    return result;
}

int gxm_texture_get_linear_layout(const SceGxmTextureInner *texture, GxmTextureLayout *layout)
{
    if (!texture || !layout)
        return SCE_GXM_ERROR_INVALID_POINTER;
    const uint32_t type = (uint32_t)texture->type << 29;
    if (type != SCE_GXM_TEXTURE_LINEAR && type != SCE_GXM_TEXTURE_LINEAR_STRIDED &&
        type != SCE_GXM_TEXTURE_SWIZZLED && type != SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY &&
        type != SCE_GXM_TEXTURE_TILED)
        return SCE_GXM_ERROR_UNSUPPORTED;
    if (texture->gamma_mode != 0)
        return SCE_GXM_ERROR_UNSUPPORTED;
    if (type != SCE_GXM_TEXTURE_LINEAR_STRIDED && texture->mip_filter != 0)
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

    result.source_type = type;
    result.bytes_per_pixel = bytes;
    if (type == SCE_GXM_TEXTURE_SWIZZLED) {
        if (texture->width_base2 > 12 || texture->height_base2 > 12)
            return SCE_GXM_ERROR_INVALID_VALUE;
        result.width = 1U << texture->width_base2;
        result.height = 1U << texture->height_base2;
    } else {
        result.width = texture->width + 1;
        result.height = texture->height + 1;
    }
    result.source_width = result.width;
    result.source_height = result.height;
    if (type == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        result.source_pitch = ((texture->mip_filter | (texture->min_filter << 1) |
                        (texture->mip_count << 3) | (texture->lod_bias << 7)) + 1) * 4;
        if (result.source_pitch < result.width * bytes)
            return SCE_GXM_ERROR_INVALID_VALUE;
    } else if (type == SCE_GXM_TEXTURE_SWIZZLED || type == SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY) {
        result.source_width = next_power_of_two(result.width);
        result.source_height = next_power_of_two(result.height);
        result.source_pitch = result.source_width * bytes;
    } else if (type == SCE_GXM_TEXTURE_TILED) {
        result.source_width = (result.width + 31) & ~31U;
        result.source_height = (result.height + 31) & ~31U;
        result.source_pitch = result.source_width * bytes;
    } else {
        result.source_pitch = ((result.width + 7) & ~7U) * bytes;
    }
    result.source_size = result.source_pitch * result.source_height;
    /* A staging copy makes small source rows representable by Maxwell's
     * 32-byte stride units without changing how source pixels are addressed. */
    result.pitch = (result.width * bytes + 31) & ~31U;
    if ((type == SCE_GXM_TEXTURE_LINEAR || type == SCE_GXM_TEXTURE_LINEAR_STRIDED) &&
        result.source_pitch % 32 == 0)
        result.pitch = result.source_pitch;
    result.size = result.pitch * result.height;
    *layout = result;
    return 0;
}

int gxm_texture_get_mip_chain(const SceGxmTextureInner *texture, GxmTextureMipChain *chain)
{
    if (!chain) return SCE_GXM_ERROR_INVALID_POINTER;
    memset(chain, 0, sizeof(*chain));
    if (!texture) return SCE_GXM_ERROR_INVALID_POINTER;
    SceGxmTextureInner level = *texture;
    uint32_t type = (uint32_t)texture->type << 29;
    /* Strided textures reuse the sampler fields to encode row stride. */
    if (type != SCE_GXM_TEXTURE_LINEAR_STRIDED) level.mip_filter = 0;
    GxmTextureMipChain result = {0};
    int ret = gxm_texture_get_linear_layout(&level, &result.levels[0]);
    if (ret) return ret;
    uint32_t width = result.levels[0].width, height = result.levels[0].height;
    uint32_t count = type == SCE_GXM_TEXTURE_LINEAR_STRIDED ? 1 : (texture->mip_count + 1) & 15;
    if (!count) count = 1;
    uint32_t shortest = width < height ? width : height, maximum = 0;
    while (shortest) { ++maximum; shortest >>= 1; }
    if (count > maximum) count = maximum;
    if (count > GXM_TEXTURE_MAX_MIP_LEVELS) return SCE_GXM_ERROR_INVALID_VALUE;

    /* The pinned renderer advances levels using a power-of-two allocation
     * footprint. Visible dimensions and row pitches can be smaller. */
    uint32_t storage_width = next_power_of_two(width), storage_height = next_power_of_two(height);
    uint64_t offset = 0;
    for (unsigned i = 0; i < count; ++i) {
        if (i) {
            if (type == SCE_GXM_TEXTURE_SWIZZLED) {
                level.width_base2 = texture->width_base2 - i;
                level.height_base2 = texture->height_base2 - i;
            } else {
                level.width = (width >> i) - 1;
                level.height = (height >> i) - 1;
            }
            ret = gxm_texture_get_linear_layout(&level, &result.levels[i]);
            if (ret) return ret;
        }
        uint64_t end = offset + result.levels[i].source_size;
        if (end > UINT32_MAX) return SCE_GXM_ERROR_INVALID_VALUE;
        result.offsets[i] = (uint32_t)offset;
        result.source_size = (uint32_t)end;
        uint32_t row = storage_width, rows = storage_height;
        if (type == SCE_GXM_TEXTURE_LINEAR) row = (row + 7) & ~7U;
        if (type == SCE_GXM_TEXTURE_TILED) { row = (row + 31) & ~31U; rows = (rows + 31) & ~31U; }
        offset += (uint64_t)row * rows * result.levels[i].bytes_per_pixel;
        storage_width >>= 1; storage_height >>= 1;
    }
    result.count = count;
    *chain = result;
    return 0;
}

static uint32_t spread_bits(uint32_t value)
{
    value = (value | (value << 8)) & 0x00ff00ffU;
    value = (value | (value << 4)) & 0x0f0f0f0fU;
    value = (value | (value << 2)) & 0x33333333U;
    return (value | (value << 1)) & 0x55555555U;
}

int gxm_texture_pack_linear(const GxmTextureLayout *layout, const void *source, size_t source_size,
                            void *destination, size_t destination_size)
{
    if (!layout || !source || !destination)
        return SCE_GXM_ERROR_INVALID_POINTER;
    if (source_size < layout->source_size || destination_size < layout->size)
        return SCE_GXM_ERROR_INVALID_VALUE;

    const unsigned char *src = source;
    unsigned char *dst = destination;
    const uint32_t bytes = layout->bytes_per_pixel;
    memset(dst, 0, layout->size);
    if (layout->source_type == SCE_GXM_TEXTURE_LINEAR ||
        layout->source_type == SCE_GXM_TEXTURE_LINEAR_STRIDED) {
        for (uint32_t y = 0; y < layout->height; y++)
            memcpy(dst + y * layout->pitch, src + y * layout->source_pitch, layout->width * bytes);
    } else if (layout->source_type == SCE_GXM_TEXTURE_TILED) {
        const uint32_t tiles_per_row = layout->source_width / 32;
        for (uint32_t y = 0; y < layout->height; y++) {
            for (uint32_t x = 0; x < layout->width; x += 32) {
                uint32_t count = layout->width - x < 32 ? layout->width - x : 32;
                uint32_t offset = ((y / 32) * tiles_per_row + x / 32) * 1024 + (y % 32) * 32;
                memcpy(dst + y * layout->pitch + x * bytes, src + offset * bytes, count * bytes);
            }
        }
    } else if (layout->source_type == SCE_GXM_TEXTURE_SWIZZLED ||
               layout->source_type == SCE_GXM_TEXTURE_SWIZZLED_ARBITRARY) {
        uint32_t minimum = layout->source_width < layout->source_height
                               ? layout->source_width : layout->source_height;
        uint32_t bits = 0;
        while ((1U << bits) < minimum)
            bits++;
        for (uint32_t y = 0; y < layout->height; y++) {
            uint32_t y_part = spread_bits(y & (minimum - 1));
            y_part |= (y >> bits) << (2 * bits);
            for (uint32_t x = 0; x < layout->width; x++) {
                uint32_t index = y_part | (spread_bits(x & (minimum - 1)) << 1) |
                                 ((x >> bits) << (2 * bits));
                memcpy(dst + y * layout->pitch + x * bytes, src + index * bytes, bytes);
            }
        }
    } else {
        return SCE_GXM_ERROR_UNSUPPORTED;
    }
    return 0;
}
