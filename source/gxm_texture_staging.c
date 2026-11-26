#include <stdlib.h>

#include "gxm/texture_staging.h"

struct GxmTextureStaging {
    DkMemBlock block;
    GxmTextureStaging *next;
};

int gxm_texture_stage(DkDevice device, GxmTextureStaging **list, const GxmTextureLayout *layout,
                      const void *source, size_t source_size, DkMemBlock *block)
{
    if (!list || !layout || !source || !block)
        return SCE_GXM_ERROR_INVALID_POINTER;
    *block = NULL;
    if (source_size < layout->source_size || layout->size == 0)
        return SCE_GXM_ERROR_INVALID_VALUE;
    if (layout->size > UINT32_MAX - (DK_MEMBLOCK_ALIGNMENT - 1))
        return SCE_GXM_ERROR_OUT_OF_MEMORY;

    GxmTextureStaging *entry = malloc(sizeof(*entry));
    if (!entry)
        return SCE_GXM_ERROR_OUT_OF_MEMORY;

    const uint32_t size = (layout->size + DK_MEMBLOCK_ALIGNMENT - 1) & ~(DK_MEMBLOCK_ALIGNMENT - 1);
    DkMemBlockMaker maker;
    dkMemBlockMakerDefaults(&maker, device, size);
    maker.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image;
    entry->block = dkMemBlockCreate(&maker);
    if (!entry->block) {
        free(entry);
        return SCE_GXM_ERROR_OUT_OF_MEMORY;
    }

    int ret = gxm_texture_pack_linear(layout, source, source_size,
                                      dkMemBlockGetCpuAddr(entry->block), size);
    if (ret != 0) {
        dkMemBlockDestroy(entry->block);
        free(entry);
        return ret;
    }
    entry->next = *list;
    *list = entry;
    *block = entry->block;
    return 0;
}

void gxm_texture_staging_release(GxmTextureStaging **list)
{
    while (*list) {
        GxmTextureStaging *entry = *list;
        *list = entry->next;
        dkMemBlockDestroy(entry->block);
        free(entry);
    }
}
