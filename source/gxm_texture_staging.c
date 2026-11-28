#include <stdlib.h>
#include <string.h>

#include "gxm/texture_staging.h"

int gxm_texture_allocate_image_pooled(DkDevice device, GxmTextureStaging **list,
    GxmStagingPool *pool, uint64_t size, DkMemBlock *block)
{
    if (!block) return SCE_GXM_ERROR_INVALID_POINTER;
    *block = NULL;
    if (!list) return SCE_GXM_ERROR_INVALID_POINTER;
    if (!size) return SCE_GXM_ERROR_INVALID_VALUE;
    if (size > UINT32_MAX - (DK_MEMBLOCK_ALIGNMENT - 1U))
        return SCE_GXM_ERROR_OUT_OF_MEMORY;
    uint32_t allocation = ((uint32_t)size + DK_MEMBLOCK_ALIGNMENT - 1U) & ~(DK_MEMBLOCK_ALIGNMENT - 1U);
    GxmTextureStaging *entry=gxm_staging_acquire(pool,device,allocation,
        DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image);
    if (!entry) return SCE_GXM_ERROR_OUT_OF_MEMORY;
    entry->next = *list; *list = entry; *block = entry->block;
    return 0;
}

int gxm_texture_stage_data_pooled(DkDevice device, GxmTextureStaging **list,
    GxmStagingPool *pool, const void *data, uint32_t size, DkMemBlock *block)
{
    if (!block) return SCE_GXM_ERROR_INVALID_POINTER;
    *block = NULL;
    if (!list || !data) return SCE_GXM_ERROR_INVALID_POINTER;
    if (!size) return SCE_GXM_ERROR_INVALID_VALUE;
    if (size > UINT32_MAX - (DK_MEMBLOCK_ALIGNMENT - 1U))
        return SCE_GXM_ERROR_OUT_OF_MEMORY;
    uint32_t allocation = (size + DK_MEMBLOCK_ALIGNMENT - 1U) & ~(DK_MEMBLOCK_ALIGNMENT - 1U);
    GxmTextureStaging *entry=gxm_staging_acquire(pool,device,allocation,
        DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached);
    if (!entry) return SCE_GXM_ERROR_OUT_OF_MEMORY;
    void *destination = dkMemBlockGetCpuAddr(entry->block);
    if (!destination) {
        gxm_staging_block_destroy(entry);
        return SCE_GXM_ERROR_INVALID_POINTER;
    }
    memcpy(destination, data, size);
    memset((char *)destination + size, 0, allocation - size);
    entry->next = *list;
    *list = entry;
    *block = entry->block;
    return 0;
}

int gxm_texture_stage_pooled(DkDevice device, GxmTextureStaging **list,
    GxmStagingPool *pool, const GxmTextureLayout *layout,
    const void *source, size_t source_size, DkMemBlock *block)
{
    if (!list || !layout || !source || !block)
        return SCE_GXM_ERROR_INVALID_POINTER;
    *block = NULL;
    if (source_size < layout->source_size || layout->size == 0)
        return SCE_GXM_ERROR_INVALID_VALUE;
    if (layout->size > UINT32_MAX - (DK_MEMBLOCK_ALIGNMENT - 1))
        return SCE_GXM_ERROR_OUT_OF_MEMORY;

    const uint32_t size = (layout->size + DK_MEMBLOCK_ALIGNMENT - 1) & ~(DK_MEMBLOCK_ALIGNMENT - 1);
    GxmTextureStaging *entry=gxm_staging_acquire(pool,device,size,
        DkMemBlockFlags_CpuCached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image);
    if (!entry) return SCE_GXM_ERROR_OUT_OF_MEMORY;

    void *destination=dkMemBlockGetCpuAddr(entry->block);
    if (!destination) {
        gxm_staging_block_destroy(entry);
        return SCE_GXM_ERROR_INVALID_POINTER;
    }

    int ret = gxm_texture_pack_linear(layout, source, source_size,
                                      destination, size);
    /* Pack through the CPU cache, then make every write visible before the
     * block can be published to commands that read it on the GPU. */
    if (!ret && dkMemBlockFlushCpuCache(entry->block, 0, size) != DkResult_Success)
        ret = SCE_GXM_ERROR_INVALID_VALUE;
    if (ret != 0) {
        gxm_staging_block_destroy(entry);
        return ret;
    }
    entry->next = *list;
    *list = entry;
    *block = entry->block;
    return 0;
}

void gxm_texture_staging_release(GxmTextureStaging **list)
{
    gxm_staging_retire(NULL,list);
}

int gxm_texture_allocate_image(DkDevice device,GxmTextureStaging **list,
    uint64_t size,DkMemBlock *block)
{ return gxm_texture_allocate_image_pooled(device,list,NULL,size,block); }

int gxm_texture_stage_data(DkDevice device,GxmTextureStaging **list,
    const void *data,uint32_t size,DkMemBlock *block)
{ return gxm_texture_stage_data_pooled(device,list,NULL,data,size,block); }

int gxm_texture_stage(DkDevice device,GxmTextureStaging **list,const GxmTextureLayout *layout,
    const void *source,size_t source_size,DkMemBlock *block)
{ return gxm_texture_stage_pooled(device,list,NULL,layout,source,source_size,block); }
