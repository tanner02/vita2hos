#ifndef GXM_TEXTURE_STAGING_H
#define GXM_TEXTURE_STAGING_H

#include "gxm/texture_layout.h"

typedef struct GxmTextureStaging GxmTextureStaging;
int gxm_texture_stage(DkDevice device, GxmTextureStaging **list, const GxmTextureLayout *layout,
                      const void *source, size_t source_size, DkMemBlock *block);
/* Caller must finish all GPU commands referencing the list before releasing it. */
void gxm_texture_staging_release(GxmTextureStaging **list);

#endif
