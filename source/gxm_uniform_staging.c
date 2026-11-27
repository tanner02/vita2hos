#include <stdlib.h>
#include <string.h>
#include <psp2/gxm.h>
#include "gxm/uniform_staging.h"

struct GxmUniformStaging { DkMemBlock block; GxmUniformStaging *next; };

int gxm_uniform_stage(DkDevice device, GxmUniformStaging **list,
                      const GxmUniformLayout *layout,
                      const GxmUniformSource sources[GXM_UNIFORM_LAYOUT_SLOTS], DkMemBlock *block)
{
    if (!block) return SCE_GXM_ERROR_INVALID_POINTER;
    *block = NULL;
    if (!device || !list || !layout || !sources) return SCE_GXM_ERROR_INVALID_POINTER;
    uint64_t end = 0;
    for (unsigned i = 0; i < GXM_UNIFORM_LAYOUT_SLOTS; ++i) {
        if (!layout->bytes[i]) continue;
        if (!sources[i].data) return SCE_GXM_ERROR_INVALID_POINTER;
        if (sources[i].size < layout->bytes[i] || layout->offsets[i] % 16 ||
            layout->offsets[i] < end) return SCE_GXM_ERROR_INVALID_VALUE;
        end = (uint64_t)layout->offsets[i] + ((uint64_t)layout->bytes[i] + 15) / 16 * 16;
        if (end > layout->size) return SCE_GXM_ERROR_INVALID_VALUE;
    }
    if (!layout->size) return 0;
    if (layout->size > UINT32_MAX - (DK_MEMBLOCK_ALIGNMENT - 1U))
        return SCE_GXM_ERROR_OUT_OF_MEMORY;
    uint32_t size = (layout->size + DK_MEMBLOCK_ALIGNMENT - 1U) & ~(DK_MEMBLOCK_ALIGNMENT - 1U);
    GxmUniformStaging *entry = malloc(sizeof(*entry));
    if (!entry) return SCE_GXM_ERROR_OUT_OF_MEMORY;
    DkMemBlockMaker maker;
    dkMemBlockMakerDefaults(&maker, device, size);
    maker.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
    entry->block = dkMemBlockCreate(&maker);
    if (!entry->block) { free(entry); return SCE_GXM_ERROR_OUT_OF_MEMORY; }
    void *destination = dkMemBlockGetCpuAddr(entry->block);
    if (!destination) {
        dkMemBlockDestroy(entry->block); free(entry);
        return SCE_GXM_ERROR_INVALID_POINTER;
    }
    memset(destination, 0, size);
    for (unsigned i = 0; i < GXM_UNIFORM_LAYOUT_SLOTS; ++i)
        if (layout->bytes[i])
            memcpy((char *)destination + layout->offsets[i], sources[i].data, layout->bytes[i]);
    entry->next = *list;
    *list = entry;
    *block = entry->block;
    return 0;
}

void gxm_uniform_staging_release(GxmUniformStaging **list)
{
    if (!list) return;
    while (*list) {
        GxmUniformStaging *entry = *list;
        *list = entry->next;
        dkMemBlockDestroy(entry->block);
        free(entry);
    }
}
