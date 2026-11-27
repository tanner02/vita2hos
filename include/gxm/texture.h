#ifndef GXM_TEXTURE_H
#define GXM_TEXTURE_H

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <psp2/gxm.h>

/* This internal bit-field view aliases the public SceGxmTexture descriptor. */
typedef struct __attribute__((may_alias)) {
    // Control Word 0
    uint32_t unk0 : 3;
    uint32_t vaddr_mode : 3;
    uint32_t uaddr_mode : 3;
    uint32_t mip_filter : 1;
    uint32_t min_filter : 2;
    uint32_t mag_filter : 2;
    uint32_t unk1 : 3;
    uint32_t mip_count : 4;
    uint32_t lod_bias : 6;
    uint32_t gamma_mode : 2;
    uint32_t unk2 : 2;
    uint32_t format0 : 1;
    // Control Word 1
    union {
        struct {
            uint32_t height : 12;
            uint32_t width : 12;
        };

        struct {
            uint32_t height_base2 : 4;
            uint32_t unknown1 : 12;
            uint32_t width_base2 : 4;
            uint32_t unknown2 : 4;
        };

        struct {
            uint32_t whblock : 24;
            uint32_t base_format : 5;
            uint32_t type : 3;
        };
    };
    // Control Word 2
    uint32_t lod_min0 : 2;
    uint32_t data_addr : 30;
    // Control Word 3
    uint32_t palette_addr : 26;
    uint32_t lod_min1 : 2;
    uint32_t swizzle_format : 3;
    uint32_t normalize_mode : 1;
} SceGxmTextureInner;
static_assert(sizeof(SceGxmTextureInner) == sizeof(SceGxmTexture), "Incorrect size");

/* Fragment and vertex stages share a descriptor table, with distinct ranges. */
#define GXM_TEXTURE_BINDING_COUNT (2 * SCE_GXM_MAX_TEXTURE_UNITS)
int gxm_texture_set_binding(SceGxmTextureInner bindings[GXM_TEXTURE_BINDING_COUNT],
                            bool vertex, unsigned int unit, const SceGxmTexture *texture);

#endif

