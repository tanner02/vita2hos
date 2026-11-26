#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../source/gxm_texture.c"
#include "../../source/gxm_texture_layout.c"
#include "../../source/gxm_texture_staging.c"

typedef struct { void *data; } TestBlock;
static unsigned int live_blocks;
static bool fail_create;
DkMemBlock dkMemBlockCreate(const DkMemBlockMaker *maker)
{
    assert(maker->size % DK_MEMBLOCK_ALIGNMENT == 0);
    assert(maker->flags & DkMemBlockFlags_CpuUncached);
    if (fail_create)
        return NULL;
    TestBlock *block = malloc(sizeof(*block));
    assert(block);
    block->data = malloc(maker->size);
    assert(block->data);
    live_blocks++;
    return block;
}
void *dkMemBlockGetCpuAddr(DkMemBlock block) { return ((TestBlock *)block)->data; }
void dkMemBlockDestroy(DkMemBlock handle)
{
    TestBlock *block = handle;
    assert(live_blocks > 0);
    free(block->data);
    free(block);
    live_blocks--;
}

/* Independent fixture builder: visit rectangular Morton blocks recursively,
 * with the Y coordinate in the low interleaved bit. */
static void morton_fixture(unsigned char *data, unsigned int *cursor, unsigned int x,
                            unsigned int y, unsigned int width, unsigned int height)
{
    if (width == 1 && height == 1) {
        data[(*cursor)++] = (unsigned char)(y * 16 + x);
    } else if (width > height) {
        morton_fixture(data, cursor, x, y, width / 2, height);
        morton_fixture(data, cursor, x + width / 2, y, width / 2, height);
    } else if (height > width) {
        morton_fixture(data, cursor, x, y, width, height / 2);
        morton_fixture(data, cursor, x, y + height / 2, width, height / 2);
    } else {
        morton_fixture(data, cursor, x, y, width / 2, height / 2);
        morton_fixture(data, cursor, x, y + height / 2, width / 2, height / 2);
        morton_fixture(data, cursor, x + width / 2, y, width / 2, height / 2);
        morton_fixture(data, cursor, x + width / 2, y + height / 2, width / 2, height / 2);
    }
}

int main(void)
{
    SceGxmTexture texture;
    SceGxmTextureInner *inner = (SceGxmTextureInner *)&texture;
    GxmTextureLayout layout;
    unsigned char source[4096], destination[4096];
    GxmTextureStaging *list = NULL;
    DkMemBlock block;

    for (unsigned int width = 1; width <= 16; width *= 2) {
        for (unsigned int height = 1; height <= 16; height *= 2) {
            unsigned int cursor = 0;
            morton_fixture(source, &cursor, 0, 0, width, height);
            assert(cursor == width * height);
            assert(sceGxmTextureInitSwizzled(&texture, NULL, SCE_GXM_TEXTURE_FORMAT_R8,
                                            width, height, 1) == 0);
            assert(gxm_texture_get_linear_layout(inner, &layout) == 0);
            assert(gxm_texture_pack_linear(&layout, source, cursor, destination,
                                            sizeof(destination)) == 0);
            for (unsigned int y = 0; y < height; y++) {
                for (unsigned int x = 0; x < width; x++)
                    assert(destination[y * layout.pitch + x] == (unsigned char)(y * 16 + x));
                for (unsigned int x = width; x < layout.pitch; x++)
                    assert(destination[y * layout.pitch + x] == 0);
            }
        }
    }
    assert((uint32_t)sceGxmTextureInitSwizzled(&texture, NULL, SCE_GXM_TEXTURE_FORMAT_R8,
                                             3, 4, 1) == SCE_GXM_ERROR_INVALID_VALUE);
    const SceGxmTextureFormat formats[] = {
        SCE_GXM_TEXTURE_FORMAT_R8, SCE_GXM_TEXTURE_FORMAT_F16_000R,
        SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR, SCE_GXM_TEXTURE_FORMAT_F16F16F16F16_ABGR,
    };
    for (unsigned int n = 0; n < 4; n++) {
        unsigned int bytes = 1U << n;
        for (unsigned int p = 0; p < 4; p++)
            for (unsigned int c = 0; c < bytes; c++)
                source[p * bytes + c] = p * 10 + c;
        assert(sceGxmTextureInitSwizzled(&texture, NULL, formats[n], 2, 2, 1) == 0);
        assert(gxm_texture_get_linear_layout(inner, &layout) == 0);
        assert(layout.bytes_per_pixel == bytes);
        assert(gxm_texture_pack_linear(&layout, source, 4 * bytes, destination,
                                        sizeof(destination)) == 0);
        const unsigned int indices[] = {0, 2, 1, 3};
        for (unsigned int p = 0; p < 4; p++)
            assert(memcmp(destination + (p / 2) * layout.pitch + (p % 2) * bytes,
                           source + indices[p] * bytes, bytes) == 0);
    }
    unsigned int cursor = 0;
    morton_fixture(source, &cursor, 0, 0, 8, 4);
    assert(sceGxmTextureInitSwizzledArbitrary(&texture, NULL, SCE_GXM_TEXTURE_FORMAT_R8,
                                            5, 3, 1) == 0);
    assert(gxm_texture_get_linear_layout(inner, &layout) == 0);
    assert(layout.source_size == 32 && layout.width == 5 && layout.height == 3);
    assert(gxm_texture_stage(NULL, &list, &layout, source, cursor, &block) == 0);
    for (unsigned int y = 0; y < 3; y++)
        for (unsigned int x = 0; x < 5; x++)
            assert(((unsigned char *)dkMemBlockGetCpuAddr(block))[y * layout.pitch + x] == y * 16 + x);

    for (unsigned int i = 0; i < 64; i++)
        source[i] = i;
    assert(sceGxmTextureInitLinear(&texture, NULL, SCE_GXM_TEXTURE_FORMAT_R8, 8, 8, 1) == 0);
    assert(gxm_texture_get_linear_layout(inner, &layout) == 0);
    assert(gxm_texture_stage(NULL, &list, &layout, source, 64, &block) == 0);
    assert(live_blocks == 2);
    for (unsigned int y = 0; y < 8; y++)
        assert(memcmp((unsigned char *)dkMemBlockGetCpuAddr(block) + y * 32, source + y * 8, 8) == 0);
    assert((uint32_t)gxm_texture_stage(NULL, &list, &layout, source, 63, &block) ==
           SCE_GXM_ERROR_INVALID_VALUE);
    fail_create = true;
    assert((uint32_t)gxm_texture_stage(NULL, &list, &layout, source, 64, &block) ==
           SCE_GXM_ERROR_OUT_OF_MEMORY);
    assert(live_blocks == 2);
    fail_create = false;
    layout.source_type = SCE_GXM_TEXTURE_CUBE;
    assert((uint32_t)gxm_texture_stage(NULL, &list, &layout, source, 64, &block) ==
           SCE_GXM_ERROR_UNSUPPORTED);
    assert(live_blocks == 2);
    gxm_texture_staging_release(&list);
    assert(list == NULL && live_blocks == 0);
    gxm_texture_staging_release(&list);

    /* A tiled texture crossing both horizontal and vertical 32-pixel tiles. */
    assert(sceGxmTextureInitTiled(&texture, NULL, SCE_GXM_TEXTURE_FORMAT_R8, 35, 33, 1) == 0);
    assert(gxm_texture_get_linear_layout(inner, &layout) == 0);
    assert(layout.source_size == 4096 && layout.pitch == 64);
    memset(source, 0, sizeof(source));
    for (unsigned int y = 0; y < 33; y++)
        for (unsigned int x = 0; x < 35; x++)
            source[((y / 32) * 2 + x / 32) * 1024 + (y % 32) * 32 + x % 32] = x + y;
    assert(gxm_texture_pack_linear(&layout, source, sizeof(source), destination,
                                    sizeof(destination)) == 0);
    for (unsigned int y = 0; y < 33; y++)
        for (unsigned int x = 0; x < 35; x++)
            assert(destination[y * 64 + x] == x + y);
    assert((uint32_t)gxm_texture_pack_linear(&layout, source, 4095, destination,
                                             sizeof(destination)) == SCE_GXM_ERROR_INVALID_VALUE);
    assert((uint32_t)gxm_texture_pack_linear(&layout, source, sizeof(source), destination,
                                             layout.size - 1) == SCE_GXM_ERROR_INVALID_VALUE);
    puts("GXM texture staging regression checks passed.");
    return 0;
}
