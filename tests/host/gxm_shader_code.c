#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include "../../source/gxm_shader_code.c"

typedef struct { void *data; uint32_t size; } Block;
static unsigned int live_blocks, create_calls;
static bool fail_create, fail_mapping;
DkMemBlock dkMemBlockCreate(const DkMemBlockMaker *maker)
{
    ++create_calls;
    assert(maker->size && maker->size % DK_MEMBLOCK_ALIGNMENT == 0);
    /* Invalid size admission must fail before requesting a huge host block. */
    assert(maker->size <= 128 * 1024);
    assert(maker->flags == (DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Code));
    if (fail_create) return NULL;
    Block *block = malloc(sizeof(*block));
    assert(block);
    block->data = malloc(maker->size);
    assert(block->data);
    memset(block->data, 0xcc, maker->size);
    block->size = maker->size;
    ++live_blocks;
    return block;
}
void *dkMemBlockGetCpuAddr(DkMemBlock block)
{ return fail_mapping ? NULL : ((Block *)block)->data; }
void dkMemBlockDestroy(DkMemBlock handle)
{
    Block *block = handle;
    assert(live_blocks);
    --live_blocks;
    free(block->data);
    free(block);
}

int main(void)
{
    GxmShaderCode *list = NULL;
    DkDevice device = (DkDevice)1;
    unsigned char source[70000];
    memset(source, 0xab, sizeof(source));
    DkMemBlock first, second, output = (DkMemBlock)1;
    assert(gxm_shader_code_upload(device, &list, source, sizeof(source), &first) == 0);
    assert(live_blocks == 1 && list && ((Block *)first)->size >= sizeof(source) + DK_SHADER_CODE_UNUSABLE_SIZE);
    assert(!memcmp(((Block *)first)->data, source, sizeof(source)));
    for (size_t i = sizeof(source); i < ((Block *)first)->size; ++i)
        assert(((unsigned char *)((Block *)first)->data)[i] == 0);
    assert(gxm_shader_code_upload(device, &list, source, 4, &second) == 0);
    assert(first != second && live_blocks == 2);
    /* Match deko3d's code-end predicate, especially at page/tail boundaries.
     * Real LBP compiled a 4096-byte DKSH that failed this requirement. */
    const uint32_t sizes[] = {1, DK_MEMBLOCK_ALIGNMENT - DK_SHADER_CODE_UNUSABLE_SIZE,
        DK_MEMBLOCK_ALIGNMENT - DK_SHADER_CODE_UNUSABLE_SIZE + 1,
        DK_MEMBLOCK_ALIGNMENT - 1, DK_MEMBLOCK_ALIGNMENT, DK_MEMBLOCK_ALIGNMENT + 1,
        2 * DK_MEMBLOCK_ALIGNMENT - DK_SHADER_CODE_UNUSABLE_SIZE,
        2 * DK_MEMBLOCK_ALIGNMENT - DK_SHADER_CODE_UNUSABLE_SIZE + 1};
    for (unsigned i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        GxmShaderCode *temporary = NULL;
        DkMemBlock handle;
        assert(gxm_shader_code_upload(device, &temporary, source, sizes[i], &handle) == 0);
        Block *native = handle;
        assert(sizes[i] <= native->size - DK_SHADER_CODE_UNUSABLE_SIZE);
        assert(!memcmp(native->data, source, sizes[i]));
        for (uint32_t j = sizes[i]; j < native->size; ++j)
            assert(((unsigned char *)native->data)[j] == 0);
        gxm_shader_code_clear(&temporary);
        assert(!temporary && live_blocks == 2);
    }
    GxmShaderCode *original_list = list;
    fail_create = true;
    assert((uint32_t)gxm_shader_code_upload(device, &list, source, 4, &output) == SCE_GXM_ERROR_OUT_OF_MEMORY);
    assert(output == NULL && list == original_list && live_blocks == 2);
    fail_create = false;
    fail_mapping = true;
    assert((uint32_t)gxm_shader_code_upload(device, &list, source, 4, &output) == SCE_GXM_ERROR_INVALID_POINTER);
    assert(output == NULL && list == original_list && live_blocks == 2);
    fail_mapping = false;
    unsigned int calls = create_calls;
    assert((uint32_t)gxm_shader_code_upload(device, &list, source, UINT32_MAX, &output) ==
           SCE_GXM_ERROR_OUT_OF_MEMORY);
    assert((uint32_t)gxm_shader_code_upload(device, &list, source,
        UINT32_MAX - (DK_MEMBLOCK_ALIGNMENT - 1U), &output) == SCE_GXM_ERROR_OUT_OF_MEMORY);
    assert((uint32_t)gxm_shader_code_upload(device, &list, source,
        UINT32_MAX - DK_SHADER_CODE_UNUSABLE_SIZE - (DK_MEMBLOCK_ALIGNMENT - 1U) + 1U,
        &output) == SCE_GXM_ERROR_OUT_OF_MEMORY);
    assert((uint32_t)gxm_shader_code_upload(device, &list, source, 0, &output) == SCE_GXM_ERROR_INVALID_VALUE);
    assert((uint32_t)gxm_shader_code_upload(device, NULL, source, 4, &output) == SCE_GXM_ERROR_INVALID_POINTER);
    assert((uint32_t)gxm_shader_code_upload(device, &list, NULL, 4, &output) == SCE_GXM_ERROR_INVALID_POINTER);
    assert(create_calls == calls && list == original_list);
    gxm_shader_code_clear(&list);
    assert(list == NULL && live_blocks == 0);
    gxm_shader_code_clear(&list);
    gxm_shader_code_clear(NULL);
    puts("Shader GPU allocation, unusable tail, overflow and cleanup checks passed.");
    return 0;
}
