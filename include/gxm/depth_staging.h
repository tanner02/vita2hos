#ifndef GXM_DEPTH_STAGING_H
#define GXM_DEPTH_STAGING_H
#include <stdbool.h>
#include <deko3d.h>
#include "gxm/depth_transfer.h"
typedef struct GxmDepthStaging GxmDepthStaging;
/* Caller keeps guest planes mapped through finish_store. Output is NULL on
 * failure. All allocation and guest span checks precede GPU command recording.
 * Combined native ZF32_X24S8 transfers use depth then low-byte stencil words. */
int gxm_depth_staging_prepare(DkDevice device,const GxmDepthTransfer *plan,
    void *depth,size_t depth_size,void *stencil,size_t stencil_size,
    uint32_t background_depth_bits,uint8_t background_stencil,bool load,bool store,
    GxmDepthStaging **out);
/* View must explicitly select ZF32_X24S8 and cover the planned dimensions.
 * The render target is single-sampled and uncompressed. Copy strides are bytes. */
int gxm_depth_staging_upload(GxmDepthStaging *stage,DkCmdBuf commands,const DkImageView *view);
int gxm_depth_staging_download(GxmDepthStaging *stage,DkCmdBuf commands,const DkImageView *view);
/* Submit the recorded download first. This waits for GPU completion, then
 * exports guest planes. Signal guest completion only after successful return. */
int gxm_depth_staging_finish_store(GxmDepthStaging *stage,DkQueue queue);
/* Waits for recorded GPU references before freeing. Discard any unsubmitted
 * command lists before releasing; queue waits cannot retire unsubmitted work. */
void gxm_depth_staging_release(GxmDepthStaging **stage,DkQueue queue);
#endif
