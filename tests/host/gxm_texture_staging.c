#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../source/gxm_texture.c"
#include "../../source/gxm_texture_layout.c"
#include "../../source/gxm_texture_staging.c"

typedef struct { unsigned char *cpu, *gpu; uint32_t flags, size; } TestBlock;
static unsigned int live_blocks, flush_calls, create_calls;
static bool fail_create, fail_mapping, fail_flush;
static GxmTextureStaging **pending_list, *pending_head;
static DkMemBlock *pending_output;
DkMemBlock dkMemBlockCreate(const DkMemBlockMaker *maker)
{
    ++create_calls;
    assert(maker->size % DK_MEMBLOCK_ALIGNMENT == 0);
    bool cached = (maker->flags & DkMemBlockFlags_CpuCached) != 0;
    assert(cached != ((maker->flags & DkMemBlockFlags_CpuUncached) != 0));
    if (fail_create) return NULL;
    TestBlock *block = malloc(sizeof(*block));
    assert(block);
    *block = (TestBlock){.flags=maker->flags,.size=maker->size};
    block->cpu = malloc(maker->size);
    block->gpu = cached ? malloc(maker->size) : block->cpu;
    assert(block->cpu && block->gpu);
    memset(block->cpu, 0xa5, maker->size);
    if (cached) memset(block->gpu, 0xcc, maker->size);
    live_blocks++;
    return block;
}
void *dkMemBlockGetCpuAddr(DkMemBlock block)
{ return fail_mapping ? NULL : ((TestBlock *)block)->cpu; }
static const unsigned char *gpu_bytes(DkMemBlock block)
{ return ((TestBlock *)block)->gpu; }
DkResult dkMemBlockFlushCpuCache(DkMemBlock handle, uint32_t offset, uint32_t size)
{
    TestBlock *block=handle;
    assert(block->flags & DkMemBlockFlags_CpuCached);
    assert(offset<=block->size && size<=block->size-offset);
    if(pending_list) {
        assert(*pending_list==pending_head && pending_output && !*pending_output);
        assert(offset==0 && size==block->size); /* Publish the whole allocation. */
    }
    ++flush_calls;
    if(fail_flush) return DkResult_Fail;
    memcpy(block->gpu+offset,block->cpu+offset,size);
    return DkResult_Success;
}
void dkMemBlockDestroy(DkMemBlock handle)
{
    TestBlock *block = handle;
    assert(live_blocks > 0);
    if(block->gpu!=block->cpu) free(block->gpu);
    free(block->cpu);
    free(block);
    live_blocks--;
}
static int checked_stage(DkDevice device,GxmTextureStaging **list,const GxmTextureLayout *layout,
                         const void *source,size_t source_size,DkMemBlock *output)
{
    unsigned before=live_blocks;
    pending_list=list;pending_head=*list;pending_output=output;
    int result=gxm_texture_stage(device,list,layout,source,source_size,output);
    pending_list=NULL;pending_output=NULL;
    if(!result) {
        TestBlock *block=*output;
        assert((block->flags & DkMemBlockFlags_CpuCached) && !(block->flags & DkMemBlockFlags_CpuUncached));
        assert(live_blocks==before+1);
    } else {
        assert(!*output && *list==pending_head && live_blocks==before);
    }
    return result;
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

    /* The fake cache is observably incoherent until a flush, independently
     * of the staging implementation and its packing code. */
    DkMemBlockMaker maker;
    dkMemBlockMakerDefaults(&maker,NULL,DK_MEMBLOCK_ALIGNMENT);
    maker.flags=DkMemBlockFlags_CpuCached | DkMemBlockFlags_GpuCached;
    DkMemBlock probe=dkMemBlockCreate(&maker);
    unsigned char *probe_cpu=dkMemBlockGetCpuAddr(probe);
    probe_cpu[7]=0x5a;
    assert(probe_cpu[7]==0x5a && gpu_bytes(probe)[7]==0xcc);
    assert(dkMemBlockFlushCpuCache(probe,7,1)==DkResult_Success);
    assert(gpu_bytes(probe)[7]==0x5a && gpu_bytes(probe)[6]==0xcc);
    dkMemBlockDestroy(probe);
    assert(!live_blocks);

    /* API-built strided descriptor must copy visible pixels from each row,
     * ignoring source padding and clearing the Maxwell-aligned destination. */
    unsigned char rows[40];memset(rows,0xee,sizeof(rows));
    for (unsigned y=0;y<2;++y) for (unsigned x=0;x<12;++x) rows[y*20+x]=(unsigned char)(y*32+x);
    assert(!sceGxmTextureInitLinearStrided(&texture,NULL,SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR,3,2,20));
    assert(!gxm_texture_get_linear_layout(inner,&layout));
    assert(!gxm_texture_pack_linear(&layout,rows,sizeof(rows),destination,sizeof(destination)));
    for (unsigned y=0;y<2;++y) {
        assert(!memcmp(destination+y*32,rows+y*20,12));
        for (unsigned x=12;x<32;++x) assert(!destination[y*32+x]);
    }
    assert(!sceGxmTextureSetWidth(&texture,2) && !sceGxmTextureSetHeight(&texture,1));
    assert(!gxm_texture_get_linear_layout(inner,&layout));
    assert(layout.source_pitch==20 && layout.source_size==20 && layout.size==32);
    assert(!gxm_texture_pack_linear(&layout,rows,20,destination,sizeof(destination)));
    assert(!memcmp(destination,rows,8));
    for (unsigned x=8;x<32;++x) assert(!destination[x]);

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
            GxmTextureStaging *fixture_list=NULL;
            DkMemBlock fixture;
            assert(!checked_stage(NULL,&fixture_list,&layout,source,cursor,&fixture));
            assert(!memcmp(gpu_bytes(fixture),destination,layout.size));
            for (unsigned int y = 0; y < height; y++) {
                for (unsigned int x = 0; x < width; x++)
                    assert(destination[y * layout.pitch + x] == (unsigned char)(y * 16 + x));
                for (unsigned int x = width; x < layout.pitch; x++)
                    assert(destination[y * layout.pitch + x] == 0);
            }
            gxm_texture_staging_release(&fixture_list);
            assert(!fixture_list && !live_blocks);
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
    assert(checked_stage(NULL, &list, &layout, source, cursor, &block) == 0);
    for (unsigned int y = 0; y < 3; y++)
        for (unsigned int x = 0; x < 5; x++)
            assert(gpu_bytes(block)[y * layout.pitch + x] == y * 16 + x);

    for (unsigned int i = 0; i < 64; i++)
        source[i] = i;
    assert(sceGxmTextureInitLinear(&texture, NULL, SCE_GXM_TEXTURE_FORMAT_R8, 8, 8, 1) == 0);
    assert(gxm_texture_get_linear_layout(inner, &layout) == 0);
    assert(checked_stage(NULL, &list, &layout, source, 64, &block) == 0);
    assert(live_blocks == 2);
    for (unsigned int y = 0; y < 8; y++)
        assert(memcmp(gpu_bytes(block) + y * 32, source + y * 8, 8) == 0);
    for(unsigned y=0;y<8;++y) for(unsigned x=8;x<32;++x)
        assert(gpu_bytes(block)[y*32+x]==0);
    memset(source,0xee,64);
    for(unsigned y=0;y<8;++y) for(unsigned x=0;x<8;++x)
        assert(gpu_bytes(block)[y*32+x]==y*8+x);
    GxmTextureStaging *saved_images=list;
    unsigned saved_flushes=flush_calls;
    fail_flush=true;
    assert((uint32_t)checked_stage(NULL,&list,&layout,source,64,&block)==SCE_GXM_ERROR_INVALID_VALUE);
    assert(!block && list==saved_images && live_blocks==2 && flush_calls==saved_flushes+1);
    fail_flush=false;fail_mapping=true;saved_flushes=flush_calls;
    assert((uint32_t)checked_stage(NULL,&list,&layout,source,64,&block)==SCE_GXM_ERROR_INVALID_POINTER);
    assert(!block && list==saved_images && live_blocks==2 && flush_calls==saved_flushes);
    fail_mapping=false;
    assert((uint32_t)checked_stage(NULL, &list, &layout, source, 63, &block) ==
           SCE_GXM_ERROR_INVALID_VALUE);
    fail_create = true;
    assert((uint32_t)checked_stage(NULL, &list, &layout, source, 64, &block) ==
           SCE_GXM_ERROR_OUT_OF_MEMORY);
    assert(live_blocks == 2);
    fail_create = false;
    layout.source_type = SCE_GXM_TEXTURE_CUBE;
    assert((uint32_t)checked_stage(NULL, &list, &layout, source, 64, &block) ==
           SCE_GXM_ERROR_UNSUPPORTED);
    assert(live_blocks == 2 && !block && list==saved_images && flush_calls==saved_flushes);
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
    assert(!checked_stage(NULL,&list,&layout,source,sizeof(source),&block));
    for(unsigned y=0;y<33;++y) {
        for(unsigned x=0;x<35;++x) assert(gpu_bytes(block)[y*64+x]==x+y);
        for(unsigned x=35;x<64;++x) assert(!gpu_bytes(block)[y*64+x]);
    }
    memset(source,0xee,sizeof(source));
    assert(gpu_bytes(block)[32*64+34]==66);
    gxm_texture_staging_release(&list);
    assert(!list && !live_blocks);

    /* BC1 uses compressed blocks; source row padding is excluded on the GPU. */
    assert(!sceGxmTextureInitLinear(&texture,NULL,SCE_GXM_TEXTURE_FORMAT_UBC1_ABGR,9,5,1));
    assert(!gxm_texture_get_linear_layout(inner,&layout));
    assert(layout.source_size==64 && layout.pitch==32);
    memset(source,0xee,sizeof(source));
    for(unsigned y=0;y<2;++y) memset(source+y*32,0x31+y,24);
    assert(!checked_stage(NULL,&list,&layout,source,64,&block));
    memset(source,0xab,64);
    for(unsigned y=0;y<2;++y) {
        for(unsigned x=0;x<24;++x) assert(gpu_bytes(block)[y*32+x]==0x31+y);
        for(unsigned x=24;x<32;++x) assert(!gpu_bytes(block)[y*32+x]);
    }
    gxm_texture_staging_release(&list);
    assert(!list && !live_blocks);

    /* Descriptor changes must not overwrite an earlier recorded snapshot. */
    DkMemBlock first, second;
    unsigned char descriptors[17];
    memset(descriptors, 0x12, sizeof(descriptors));
    assert(!gxm_texture_stage_data(NULL, &list, descriptors, sizeof(descriptors), &first));
    memset(descriptors, 0x34, sizeof(descriptors));
    assert(!gxm_texture_stage_data(NULL, &list, descriptors, sizeof(descriptors), &second));
    assert(first != second && live_blocks == 2);
    assert(((TestBlock *)first)->flags==(DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached));
    assert(((TestBlock *)second)->cpu==((TestBlock *)second)->gpu);
    for (unsigned i = 0; i < sizeof(descriptors); ++i) {
        assert(gpu_bytes(first)[i] == 0x12);
        assert(gpu_bytes(second)[i] == 0x34);
    }
    for (unsigned i = sizeof(descriptors); i < DK_MEMBLOCK_ALIGNMENT; ++i)
        assert(gpu_bytes(first)[i] == 0);
    GxmTextureStaging *saved = list;
    fail_create = true;
    assert((uint32_t)gxm_texture_stage_data(NULL, &list, descriptors, sizeof(descriptors), &block)
            == SCE_GXM_ERROR_OUT_OF_MEMORY && !block && list == saved);
    fail_create = false; fail_mapping = true;
    assert((uint32_t)gxm_texture_stage_data(NULL, &list, descriptors, sizeof(descriptors), &block)
            == SCE_GXM_ERROR_INVALID_POINTER && !block && live_blocks == 2 && list == saved);
    fail_mapping = false;
    assert((uint32_t)gxm_texture_stage_data(NULL, &list, descriptors, UINT32_MAX, &block)
            == SCE_GXM_ERROR_OUT_OF_MEMORY && !block);
    assert((uint32_t)gxm_texture_stage_data(NULL, &list, descriptors, 0, &block)
            == SCE_GXM_ERROR_INVALID_VALUE && !block);
    gxm_texture_staging_release(&list);
    assert(!list && !live_blocks);
    saved_flushes=flush_calls;
    assert(!gxm_texture_allocate_image(NULL, &list, 8193, &block) && live_blocks == 1);
    assert(((TestBlock *)block)->flags==(DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image));
    assert(((TestBlock *)block)->cpu==((TestBlock *)block)->gpu && flush_calls==saved_flushes);
    saved = list;
    assert((uint32_t)gxm_texture_allocate_image(NULL, &list, UINT64_MAX, &block)
            == SCE_GXM_ERROR_OUT_OF_MEMORY && !block && list == saved);
    assert((uint32_t)gxm_texture_allocate_image(NULL, &list, 0, &block)
            == SCE_GXM_ERROR_INVALID_VALUE && !block);
    fail_create = true;
    assert((uint32_t)gxm_texture_allocate_image(NULL, &list, 4096, &block)
            == SCE_GXM_ERROR_OUT_OF_MEMORY && !block && live_blocks == 1);
    fail_create = false;
    gxm_texture_staging_release(&list);
    assert(!list && !live_blocks);
    /* Each scene retains distinct immutable snapshots, then reuses the exact
     * allocations after simulated GPU retirement. No source-data cache is used. */
    GxmStagingPool pool={.max_bytes=3*DK_MEMBLOCK_ALIGNMENT,.max_count=3};
    unsigned calls=create_calls;
    for(unsigned scene=0;scene<100;++scene) {
        memset(descriptors,0x12,sizeof(descriptors));
        assert(!gxm_texture_stage_data_pooled(NULL,&list,&pool,descriptors,sizeof(descriptors),&first));
        memset(descriptors,0x34,sizeof(descriptors));
        assert(!gxm_texture_stage_data_pooled(NULL,&list,&pool,descriptors,sizeof(descriptors),&second));
        assert(first!=second && gpu_bytes(first)[0]==0x12 && gpu_bytes(second)[0]==0x34);
        assert(create_calls==calls+2 && live_blocks==2 && !pool.count);
        gxm_staging_retire(&pool,&list);
        assert(!list && pool.count==2 && pool.bytes==2*DK_MEMBLOCK_ALIGNMENT);
    }
    /* Identical byte size cannot alias an image allocation with descriptors. */
    assert(!gxm_texture_allocate_image_pooled(NULL,&list,&pool,1,&block));
    assert(create_calls==calls+3 && pool.count==2);
    gxm_staging_retire(&pool,&list);
    assert(pool.count==3);
    gxm_staging_pool_release(&pool);
    assert(!live_blocks && !pool.bytes && !pool.count);

    assert(!sceGxmTextureInitLinear(&texture,NULL,SCE_GXM_TEXTURE_FORMAT_UBC1_ABGR,9,5,1));
    assert(!gxm_texture_get_linear_layout(inner,&layout));
    memset(source,0x55,64);calls=create_calls;saved_flushes=flush_calls;
    assert(!gxm_texture_stage_pooled(NULL,&list,&pool,&layout,source,64,&first));
    assert(gpu_bytes(first)[0]==0x55);
    gxm_staging_retire(&pool,&list);
    memset(source,0x66,64);
    assert(!gxm_texture_stage_pooled(NULL,&list,&pool,&layout,source,64,&second));
    assert(first==second && create_calls==calls+1 && flush_calls==saved_flushes+2);
    assert(gpu_bytes(second)[0]==0x66);
    gxm_staging_retire(&pool,&list);
    fail_flush=true;
    assert((uint32_t)gxm_texture_stage_pooled(NULL,&list,&pool,&layout,source,64,&block)
        ==SCE_GXM_ERROR_INVALID_VALUE && !block && !list && !pool.count && !live_blocks);
    fail_flush=false;
    assert(!gxm_texture_stage_data_pooled(NULL,&list,&pool,descriptors,sizeof(descriptors),&block));
    gxm_staging_retire(&pool,&list);
    fail_mapping=true;
    assert((uint32_t)gxm_texture_stage_data_pooled(NULL,&list,&pool,descriptors,sizeof(descriptors),&block)
        ==SCE_GXM_ERROR_INVALID_POINTER && !block && !list && !pool.count && !live_blocks);
    fail_mapping=false;
    gxm_staging_pool_release(&pool);
    puts("GXM texture staging and descriptor snapshot regression checks passed.");
    return 0;
}
