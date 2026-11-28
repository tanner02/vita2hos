#ifndef GXM_TEXTURE_STAGING_H
#define GXM_TEXTURE_STAGING_H

#include "gxm/texture_layout.h"
#include "gxm/staging_pool.h"

typedef GxmStagingBlock GxmTextureStaging;
int gxm_texture_allocate_image(DkDevice device, GxmTextureStaging **list,
                                uint64_t size, DkMemBlock *block);
int gxm_texture_stage_data(DkDevice device, GxmTextureStaging **list, const void *data,
                           uint32_t size, DkMemBlock *block);
int gxm_texture_stage(DkDevice device, GxmTextureStaging **list, const GxmTextureLayout *layout,
                      const void *source, size_t source_size, DkMemBlock *block);
int gxm_texture_allocate_image_pooled(DkDevice device, GxmTextureStaging **list,
    GxmStagingPool *pool, uint64_t size, DkMemBlock *block);
int gxm_texture_stage_data_pooled(DkDevice device, GxmTextureStaging **list,
    GxmStagingPool *pool, const void *data, uint32_t size, DkMemBlock *block);
int gxm_texture_stage_pooled(DkDevice device, GxmTextureStaging **list,
    GxmStagingPool *pool, const GxmTextureLayout *layout,
    const void *source, size_t source_size, DkMemBlock *block);
/* Caller must finish all GPU commands referencing the list before releasing it. */
void gxm_texture_staging_release(GxmTextureStaging **list);

#endif
