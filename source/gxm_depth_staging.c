#include <stdlib.h>
#include <string.h>
#include "gxm/depth_staging.h"
struct GxmDepthStaging {
    GxmDepthTransfer plan;
    DkMemBlock block;
    GxmDepthSample *samples;
    DkCopyBuf buffer;
    void *depth,*stencil;
    size_t depth_size,stencil_size;
    bool load,store,uploaded,downloaded,pending,exported;
};
int gxm_depth_staging_prepare(DkDevice device,const GxmDepthTransfer *plan,
    void *depth,size_t depth_size,void *stencil,size_t stencil_size,
    uint32_t background_depth_bits,uint8_t background_stencil,bool load,bool store,
    GxmDepthStaging **out)
{
    if (!out) return SCE_GXM_ERROR_INVALID_POINTER;
    *out=NULL;
    int result=gxm_depth_transfer_check_plan(plan);
    if (result) return result;
    if (!load && !store) return SCE_GXM_ERROR_INVALID_VALUE;
    uint64_t pitch=(uint64_t)plan->sample_stride*sizeof(GxmDepthSample);
    uint64_t bytes=pitch*plan->height;
    if (pitch%64) return SCE_GXM_ERROR_INVALID_ALIGNMENT;
    if (bytes>UINT32_MAX-(DK_MEMBLOCK_ALIGNMENT-1)) return SCE_GXM_ERROR_OUT_OF_MEMORY;
    GxmDepthStaging *stage=calloc(1,sizeof(*stage));
    if (!stage) return SCE_GXM_ERROR_OUT_OF_MEMORY;
    uint32_t size=((uint32_t)bytes+DK_MEMBLOCK_ALIGNMENT-1)&~(DK_MEMBLOCK_ALIGNMENT-1);
    DkMemBlockMaker maker;
    dkMemBlockMakerDefaults(&maker,device,size);
    maker.flags=DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached;
    stage->block=dkMemBlockCreate(&maker);
    if (!stage->block) { free(stage);return SCE_GXM_ERROR_OUT_OF_MEMORY; }
    stage->samples=dkMemBlockGetCpuAddr(stage->block);
    stage->buffer=(DkCopyBuf){.addr=dkMemBlockGetGpuAddr(stage->block),
                             .rowLength=(uint32_t)pitch,.imageHeight=(uint32_t)bytes};
    result=0;
    if (!stage->samples || stage->buffer.addr==DK_GPU_ADDR_INVALID) result=SCE_GXM_ERROR_INVALID_POINTER;
    if (!result && store) result=gxm_depth_transfer_check_write(plan,stage->samples,
        plan->sample_count,depth,depth_size,stencil,stencil_size);
    if (!result) memset(stage->samples,0,size);
    if (!result && load) result=gxm_depth_transfer_read(plan,depth,depth_size,stencil,stencil_size,
        background_depth_bits,background_stencil,stage->samples,plan->sample_count);
    if (result) { dkMemBlockDestroy(stage->block);free(stage);return result; }
    stage->plan=*plan;stage->depth=depth;stage->stencil=stencil;
    stage->depth_size=depth_size;stage->stencil_size=stencil_size;
    stage->load=load;stage->store=store;*out=stage;
    return 0;
}
static int check_view(const GxmDepthStaging *stage,const DkImageView *view)
{
    if (!stage || !view || !view->pImage) return SCE_GXM_ERROR_INVALID_POINTER;
    if (view->format!=DkImageFormat_ZF32_X24S8) return SCE_GXM_ERROR_UNSUPPORTED;
    return 0;
}
int gxm_depth_staging_upload(GxmDepthStaging *stage,DkCmdBuf commands,const DkImageView *view)
{
    if (!commands) return SCE_GXM_ERROR_INVALID_POINTER;
    int result=check_view(stage,view);if (result) return result;
    if (!stage->load || stage->uploaded || stage->downloaded) return SCE_GXM_ERROR_INVALID_VALUE;
    DkImageRect rect={.width=stage->plan.width,.height=stage->plan.height,.depth=1};
    dkCmdBufBarrier(commands,DkBarrier_Full,DkInvalidateFlags_L2Cache);
    dkCmdBufCopyBufferToImage(commands,&stage->buffer,view,&rect,0);
    dkCmdBufBarrier(commands,DkBarrier_Full,DkInvalidateFlags_Image|DkInvalidateFlags_Zcull);
    stage->uploaded=stage->pending=true;
    return 0;
}
int gxm_depth_staging_download(GxmDepthStaging *stage,DkCmdBuf commands,const DkImageView *view)
{
    if (!commands) return SCE_GXM_ERROR_INVALID_POINTER;
    int result=check_view(stage,view);if (result) return result;
    if (!stage->store || stage->downloaded || (stage->load && !stage->uploaded))
        return SCE_GXM_ERROR_INVALID_VALUE;
    DkImageRect rect={.width=stage->plan.width,.height=stage->plan.height,.depth=1};
    dkCmdBufBarrier(commands,DkBarrier_Full,0);
    dkCmdBufCopyImageToBuffer(commands,view,&rect,&stage->buffer,0);
    dkCmdBufBarrier(commands,DkBarrier_Full,DkInvalidateFlags_L2Cache);
    stage->downloaded=stage->pending=true;
    return 0;
}
int gxm_depth_staging_finish_store(GxmDepthStaging *stage,DkQueue queue)
{
    if (!stage || !queue) return SCE_GXM_ERROR_INVALID_POINTER;
    if (!stage->downloaded || stage->exported) return SCE_GXM_ERROR_INVALID_VALUE;
    dkQueueFlush(queue);dkQueueWaitIdle(queue);stage->pending=false;
    int result=gxm_depth_transfer_write(&stage->plan,stage->samples,stage->plan.sample_count,
        stage->depth,stage->depth_size,stage->stencil,stage->stencil_size);
    if (!result) stage->exported=true;
    return result;
}
void gxm_depth_staging_release(GxmDepthStaging **owner,DkQueue queue)
{
    if (!owner || !*owner) return;
    GxmDepthStaging *stage=*owner;
    if (stage->pending && !queue) return;
    if (stage->pending) { dkQueueFlush(queue);dkQueueWaitIdle(queue); }
    dkMemBlockDestroy(stage->block);free(stage);*owner=NULL;
}
