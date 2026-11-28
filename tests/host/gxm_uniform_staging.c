#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include "../../source/gxm_uniform_layout.c"
#include "../../source/gxm_uniform_staging.c"

typedef struct { void *data; uint32_t size; } Block;
static unsigned live_blocks, create_calls;
static bool fail_create, fail_mapping;
DkMemBlock dkMemBlockCreate(const DkMemBlockMaker *maker)
{
    ++create_calls;
    assert(maker->size % DK_MEMBLOCK_ALIGNMENT == 0);
    assert(maker->flags == (DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached));
    if (fail_create) return NULL;
    Block *block = malloc(sizeof(*block));
    assert(block);
    block->data = malloc(maker->size);
    assert(block->data);
    block->size = maker->size;
    memset(block->data, 0xcc, maker->size);
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
    free(block->data); free(block);
}

int main(void)
{
    uint32_t counts[15] = {3, 1, 0, 0, 5};
    uint32_t defaults[] = {1, 2, 3}, named[] = {4, 5, 6, 7, 8};
    GxmUniformLayout layout;
    assert(gxm_uniform_layout_build(counts, &layout));
    GxmUniformSource sources[15] = {{defaults, sizeof(defaults)}, {named, 4},
        {0}, {0}, {named, sizeof(named)}};
    GxmUniformStaging *list = NULL;
    DkMemBlock first, second, out;
    assert(!gxm_uniform_stage((DkDevice)1, &list, &layout, sources, &first));
    unsigned char expected[64] = {0};
    memcpy(expected, defaults, 12); memcpy(expected + 16, named, 4); memcpy(expected + 32, named, 20);
    assert(!memcmp(((Block *)first)->data, expected, 64));
    for (unsigned i = 64; i < ((Block *)first)->size; ++i)
        assert(((unsigned char *)((Block *)first)->data)[i] == 0);
    defaults[0] = 99;
    assert(!gxm_uniform_stage((DkDevice)1, &list, &layout, sources, &second));
    assert(first != second && live_blocks == 2);
    assert(((uint32_t *)((Block *)first)->data)[0] == 1);
    assert(((uint32_t *)((Block *)second)->data)[0] == 99);
    GxmUniformStaging *saved = list;
    unsigned calls = create_calls;
    sources[4].size--;
    assert((uint32_t)gxm_uniform_stage((DkDevice)1, &list, &layout, sources, &out)
            == SCE_GXM_ERROR_INVALID_VALUE && !out && list == saved);
    sources[4].size++;
    GxmUniformLayout invalid = layout;
    invalid.offsets[4] = 16;
    assert((uint32_t)gxm_uniform_stage((DkDevice)1, &list, &invalid, sources, &out)
            == SCE_GXM_ERROR_INVALID_VALUE && !out);
    invalid = layout; invalid.size = 32;
    assert((uint32_t)gxm_uniform_stage((DkDevice)1, &list, &invalid, sources, &out)
            == SCE_GXM_ERROR_INVALID_VALUE);
    assert(create_calls == calls);
    sources[4].data = NULL;
    assert((uint32_t)gxm_uniform_stage((DkDevice)1, &list, &layout, sources, &out)
            == SCE_GXM_ERROR_INVALID_POINTER && !out && list == saved);
    sources[4].data = named;
    fail_create = true;
    assert((uint32_t)gxm_uniform_stage((DkDevice)1, &list, &layout, sources, &out)
            == SCE_GXM_ERROR_OUT_OF_MEMORY && !out && list == saved);
    fail_create = false; fail_mapping = true;
    assert((uint32_t)gxm_uniform_stage((DkDevice)1, &list, &layout, sources, &out)
            == SCE_GXM_ERROR_INVALID_POINTER && !out && list == saved && live_blocks == 2);
    fail_mapping = false;
    GxmUniformLayout empty = {0};
    assert(!gxm_uniform_stage((DkDevice)1, &list, &empty, sources, &out) && !out);
    empty.size = UINT32_MAX;
    assert((uint32_t)gxm_uniform_stage((DkDevice)1, &list, &empty, sources, &out)
            == SCE_GXM_ERROR_OUT_OF_MEMORY && !out);
    gxm_uniform_staging_release(&list);
    assert(!list && !live_blocks);
    gxm_uniform_staging_release(&list);
    gxm_uniform_staging_release(NULL);
    GxmStagingPool pool={.max_bytes=2*DK_MEMBLOCK_ALIGNMENT,.max_count=2};
    calls=create_calls;
    for(unsigned scene=0;scene<100;++scene) {
        defaults[0]=11;
        assert(!gxm_uniform_stage_pooled((DkDevice)1,&list,&pool,&layout,sources,&first));
        defaults[0]=22;
        assert(!gxm_uniform_stage_pooled((DkDevice)1,&list,&pool,&layout,sources,&second));
        assert(first!=second && live_blocks==2 && create_calls==calls+2);
        assert(((uint32_t *)((Block *)first)->data)[0]==11);
        assert(((uint32_t *)((Block *)second)->data)[0]==22);
        gxm_staging_retire(&pool,&list);
        assert(!list && pool.count==2 && pool.bytes==2*DK_MEMBLOCK_ALIGNMENT);
    }
    uint32_t small_counts[15]={1};GxmUniformLayout small;
    assert(gxm_uniform_layout_build(small_counts,&small));
    assert(!gxm_uniform_stage_pooled((DkDevice)1,&list,&pool,&small,sources,&first));
    assert(create_calls==calls+2);
    for(unsigned i=4;i<((Block *)first)->size;i++)
        assert(!((unsigned char *)((Block *)first)->data)[i]);
    gxm_staging_retire(&pool,&list);
    fail_mapping=true;
    assert((uint32_t)gxm_uniform_stage_pooled((DkDevice)1,&list,&pool,&small,sources,&out)
        ==SCE_GXM_ERROR_INVALID_POINTER && !out && !list && pool.count==1 && live_blocks==1);
    fail_mapping=false;
    gxm_staging_pool_release(&pool);
    assert(!pool.bytes && !pool.count && !live_blocks);
    puts("Uniform snapshot packing, isolation and cleanup checks passed.");
}
