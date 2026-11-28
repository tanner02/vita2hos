#ifndef GXM_STAGING_POOL_H
#define GXM_STAGING_POOL_H

#include <deko3d.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct GxmStagingBlock {
    DkMemBlock block;
    struct GxmStagingBlock *next;
    DkDevice device;
    uint32_t size, flags;
} GxmStagingBlock;

typedef struct {
    GxmStagingBlock *available;
    uint32_t bytes, max_bytes;
    unsigned count, max_count;
} GxmStagingPool;

static inline void gxm_staging_block_destroy(GxmStagingBlock *entry)
{
    dkMemBlockDestroy(entry->block);
    free(entry);
}

static inline GxmStagingBlock *gxm_staging_acquire(GxmStagingPool *pool,
    DkDevice device, uint32_t size, uint32_t flags)
{
    if (pool) {
        for (GxmStagingBlock **slot=&pool->available; *slot; slot=&(*slot)->next) {
            GxmStagingBlock *entry=*slot;
            if (entry->device!=device || entry->size!=size || entry->flags!=flags)
                continue;
            *slot=entry->next;
            pool->bytes-=size;
            pool->count--;
            entry->next=NULL;
            return entry;
        }
    }
    GxmStagingBlock *entry=malloc(sizeof(*entry));
    if (!entry) return NULL;
    DkMemBlockMaker maker;
    dkMemBlockMakerDefaults(&maker,device,size);
    maker.flags=flags;
    entry->block=dkMemBlockCreate(&maker);
    if (!entry->block) { free(entry);return NULL; }
    entry->device=device;entry->size=size;entry->flags=flags;entry->next=NULL;
    return entry;
}

/* The caller must finish every GPU command referencing the active list before
 * retiring it. Available entries are never handed out twice in one scene. */
static inline void gxm_staging_retire(GxmStagingPool *pool,GxmStagingBlock **active)
{
    if (!active) return;
    while (*active) {
        GxmStagingBlock *entry=*active;
        *active=entry->next;
        if (!pool || pool->count>=pool->max_count || pool->bytes>pool->max_bytes ||
            entry->size>pool->max_bytes-pool->bytes) {
            gxm_staging_block_destroy(entry);
            continue;
        }
        entry->next=pool->available;pool->available=entry;
        pool->bytes+=entry->size;pool->count++;
    }
}

static inline void gxm_staging_pool_release(GxmStagingPool *pool)
{
    if (!pool) return;
    gxm_staging_retire(NULL,&pool->available);
    pool->bytes=0;pool->count=0;
}

#endif
