#ifndef GXM_UNIFORM_STAGING_H
#define GXM_UNIFORM_STAGING_H
#include <deko3d.h>
#include "gxm/uniform_layout.h"
#include "gxm/staging_pool.h"

typedef GxmStagingBlock GxmUniformStaging;
typedef struct { const void *data; uint32_t size; } GxmUniformSource;
int gxm_uniform_stage(DkDevice device, GxmUniformStaging **list,
                      const GxmUniformLayout *layout,
                      const GxmUniformSource sources[GXM_UNIFORM_LAYOUT_SLOTS], DkMemBlock *block);
int gxm_uniform_stage_pooled(DkDevice device, GxmUniformStaging **list, GxmStagingPool *pool,
    const GxmUniformLayout *layout,
    const GxmUniformSource sources[GXM_UNIFORM_LAYOUT_SLOTS], DkMemBlock *block);
void gxm_uniform_staging_release(GxmUniformStaging **list);
#endif
