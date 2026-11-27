#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../source/gxm_depth_transfer.c"
#include "../../source/gxm_depth_staging.c"
typedef struct { unsigned char *data;uint32_t size; } Block;
static Block *live;
static unsigned allocations,destroys,waits,flushes;
static bool fail_create,fail_cpu,fail_gpu,submitted;
static GxmDepthSample image[6];
typedef struct { int kind;DkCopyBuf buffer; } Job;
static Job jobs[8];static unsigned job_count;
DkMemBlock dkMemBlockCreate(const DkMemBlockMaker *maker)
{
    assert(!live && maker->size%DK_MEMBLOCK_ALIGNMENT==0);
    assert(maker->flags==(DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached));
    if (fail_create) return NULL;
    live=malloc(sizeof(*live));assert(live);live->data=malloc(maker->size);assert(live->data);
    live->size=maker->size;++allocations;return live;
}
void *dkMemBlockGetCpuAddr(DkMemBlock block) { assert(block==live);return fail_cpu ? NULL : live->data; }
DkGpuAddr dkMemBlockGetGpuAddr(DkMemBlock block)
{ assert(block==live);return fail_gpu ? DK_GPU_ADDR_INVALID : UINT64_C(0x123450000); }
void dkMemBlockDestroy(DkMemBlock block)
{ assert(block==live && !job_count);free(live->data);free(live);live=NULL;++destroys; }
void dkCmdBufBarrier(DkCmdBuf commands,DkBarrier mode,uint32_t flags)
{ assert(commands && mode==DkBarrier_Full);(void)flags; }
static void record(int kind,const DkCopyBuf *buffer,const DkImageView *view,const DkImageRect *rect,uint32_t flags)
{
    assert(live && view && view->pImage && view->format==DkImageFormat_ZF32_X24S8);
    assert(!flags && !rect->x && !rect->y && !rect->z && rect->width==3 && rect->height==2 && rect->depth==1);
    assert(buffer->addr==UINT64_C(0x123450000) && buffer->rowLength==64 && buffer->imageHeight==128);
    jobs[job_count++]=(Job){.kind=kind,.buffer=*buffer};
}
void dkCmdBufCopyBufferToImage(DkCmdBuf commands,const DkCopyBuf *buffer,const DkImageView *view,const DkImageRect *rect,uint32_t flags)
{ assert(commands);record(1,buffer,view,rect,flags); }
void dkCmdBufCopyImageToBuffer(DkCmdBuf commands,const DkImageView *view,const DkImageRect *rect,const DkCopyBuf *buffer,uint32_t flags)
{ assert(commands);record(2,buffer,view,rect,flags); }
void dkQueueFlush(DkQueue queue) { assert(queue);++flushes; }
void dkQueueWaitIdle(DkQueue queue)
{
    assert(queue && flushes);++waits;
    if (job_count) assert(submitted);
    for (unsigned j=0;j<job_count;++j) {
        assert(live);
        if (jobs[j].kind==3) {
            for (unsigned i=0;i<6;++i) image[i]=(GxmDepthSample){0x3e800000+i,0xabcdef00u+i};
        } else for (unsigned y=0;y<2;++y) {
            void *row=live->data+y*jobs[j].buffer.rowLength;
            if (jobs[j].kind==1) memcpy(image+y*3,row,3*sizeof(GxmDepthSample));
            else memcpy(row,image+y*3,3*sizeof(GxmDepthSample));
        }
    }
    job_count=0;submitted=false;
}
int main(void)
{
    GxmDepthTransfer p;assert(!gxm_depth_transfer_plan(&p,SCE_GXM_DEPTH_STENCIL_FORMAT_DF32_S8,0,3,2,32,8));
    uint32_t depth[64];unsigned char stencil[64];
    for (unsigned i=0;i<64;++i) { depth[i]=0x3f000000+i;stencil[i]=(unsigned char)i; }
    uint32_t old_depth[64];unsigned char old_stencil[64];memcpy(old_depth,depth,sizeof(depth));memcpy(old_stencil,stencil,64);
    GxmDepthStaging *stage=NULL;DkImage native={0};DkImageView view={.pImage=&native,.format=DkImageFormat_ZF32_X24S8};
    void *commands=(void *)1,*queue=(void *)2;
    assert(!gxm_depth_staging_prepare(NULL,&p,depth,sizeof(depth),stencil,64,0x3f800000,0,true,true,&stage));
    assert(stage && live && !memcmp(depth,old_depth,sizeof(depth)));
    assert(gxm_depth_staging_download(stage,commands,&view)<0); /* Upload must precede download. */
    assert(gxm_depth_staging_upload(stage,NULL,&view)<0);
    DkImageView wrong=view;wrong.format=DkImageFormat_RGBA8_Unorm;
    assert(gxm_depth_staging_upload(stage,commands,&wrong)<0 && !job_count);
    assert(!gxm_depth_staging_upload(stage,commands,&view));
    assert(gxm_depth_staging_upload(stage,commands,&view)<0);
    jobs[job_count++]=(Job){.kind=3}; /* Draw between upload and download. */
    assert(!gxm_depth_staging_download(stage,commands,&view));
    assert(!memcmp(depth,old_depth,sizeof(depth)) && !memcmp(stencil,old_stencil,64));
    assert(gxm_depth_staging_finish_store(stage,NULL)<0);
    submitted=true;assert(!gxm_depth_staging_finish_store(stage,queue));
    for (unsigned y=0;y<2;++y) for (unsigned x=0;x<32;++x) {
        unsigned at=y*32+x;
        assert(depth[at]==(x<3 ? 0x3e800000+y*3+x : old_depth[at]));
        assert(stencil[at]==(x<3 ? y*3+x : old_stencil[at]));
    }
    assert(gxm_depth_staging_finish_store(stage,queue)<0);
    unsigned waited=waits;gxm_depth_staging_release(&stage,queue);
    assert(!stage && !live && waits==waited);
    /* Upload-only retirement waits before destroying GPU-referenced storage. */
    assert(!gxm_depth_staging_prepare(NULL,&p,depth,sizeof(depth),NULL,0,0,77,true,false,&stage));
    assert(!gxm_depth_staging_upload(stage,commands,&view));
    gxm_depth_staging_release(&stage,NULL);assert(stage && live);
    submitted=true;gxm_depth_staging_release(&stage,queue);assert(!stage && waits==waited+1);
    assert(image[0].stencil==77 && image[5].stencil==77);
    /* Store-only staging preserves guest output until the recorded copy completes. */
    assert(!gxm_depth_staging_prepare(NULL,&p,depth,sizeof(depth),stencil,64,0,0,false,true,&stage));
    assert(gxm_depth_staging_upload(stage,commands,&view)<0);
    assert(!gxm_depth_staging_download(stage,commands,&view));submitted=true;
    assert(!gxm_depth_staging_finish_store(stage,queue));gxm_depth_staging_release(&stage,queue);
    for (int failure=0;failure<4;++failure) {
        fail_create=failure==0;fail_cpu=failure==1;fail_gpu=failure==2;
        assert(gxm_depth_staging_prepare(NULL,&p,depth,failure==3 ? 1 : sizeof(depth),stencil,64,0,0,true,true,&stage)<0);
        assert(!stage && !live && !job_count);
    }
    fail_create=fail_cpu=fail_gpu=false;
    assert(gxm_depth_staging_prepare(NULL,&p,NULL,0,NULL,0,0,0,false,false,&stage)<0 && !stage);
    assert(allocations==destroys);
    puts("Depth GPU staging model: byte strides, upload/draw/readback order, CPU export timing, ownership and allocation failures passed.");
}
