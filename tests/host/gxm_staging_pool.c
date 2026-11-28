#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include "gxm/staging_pool.h"

typedef struct { DkDevice device; uint32_t size,flags; } FakeBlock;
static unsigned created,destroyed,live;
static bool fail_create;
DkMemBlock dkMemBlockCreate(const DkMemBlockMaker *maker)
{
    ++created;
    if(fail_create)return NULL;
    FakeBlock *block=malloc(sizeof(*block));assert(block);
    *block=(FakeBlock){maker->device,maker->size,maker->flags};++live;
    return block;
}
void dkMemBlockDestroy(DkMemBlock block)
{ assert(block&&live);free(block);--live;++destroyed; }
static void publish(GxmStagingBlock **active,GxmStagingBlock *entry)
{ assert(entry);entry->next=*active;*active=entry; }
int main(void)
{
    const uint32_t n=DK_MEMBLOCK_ALIGNMENT;
    const uint32_t flags=DkMemBlockFlags_CpuUncached|DkMemBlockFlags_GpuCached;
    DkDevice device=(DkDevice)1;
    GxmStagingPool pool={.max_bytes=4*n,.max_count=4};
    GxmStagingBlock *active=NULL;
    GxmStagingBlock *a=gxm_staging_acquire(&pool,device,n,flags);
    GxmStagingBlock *b=gxm_staging_acquire(&pool,device,n,flags);
    assert(a&&b&&a!=b&&a->block!=b->block&&created==2&&live==2);
    publish(&active,a);publish(&active,b);
    assert(!pool.count&&!pool.bytes&&!pool.available);
    gxm_staging_retire(&pool,&active);
    assert(!active&&pool.count==2&&pool.bytes==2*n&&!destroyed);
    fail_create=true;
    GxmStagingBlock *reused=gxm_staging_acquire(&pool,device,n,flags);
    assert((reused==a||reused==b)&&created==2&&pool.count==1&&pool.bytes==n);
    fail_create=false;publish(&active,reused);
    /* A free block with the wrong device, byte size or memory flags cannot
     * satisfy a request, even though an allocation is available. */
    const struct { DkDevice device;uint32_t size,flags; } misses[]={
        {(DkDevice)2,n,flags},{device,2*n,flags},{device,n,flags|DkMemBlockFlags_Image}
    };
    for(unsigned i=0;i<3;i++) {
        unsigned before=created;
        GxmStagingBlock *miss=gxm_staging_acquire(&pool,misses[i].device,misses[i].size,misses[i].flags);
        assert(miss&&created==before+1&&pool.count==1&&pool.bytes==n);
        FakeBlock *memory=miss->block;
        assert(memory->device==misses[i].device&&memory->size==misses[i].size&&memory->flags==misses[i].flags);
        gxm_staging_retire(NULL,&miss);
        assert(!miss&&live==2);
    }
    fail_create=true;
    assert(!gxm_staging_acquire(&pool,device,3*n,flags)&&pool.count==1&&live==2);
    fail_create=false;
    /* Pool destruction never touches an active snapshot. */
    gxm_staging_pool_release(&pool);
    assert(!pool.available&&!pool.bytes&&!pool.count&&live==1&&active==reused);
    assert(pool.max_bytes==4*n&&pool.max_count==4);
    gxm_staging_retire(NULL,&active);assert(!active&&!live);

    pool.max_bytes=n;pool.max_count=4;
    publish(&active,gxm_staging_acquire(&pool,device,2*n,flags));
    gxm_staging_retire(&pool,&active);
    assert(!active&&!pool.count&&!live); /* Single block exceeds byte cap. */
    for(unsigned i=0;i<3;i++)publish(&active,gxm_staging_acquire(&pool,device,n,flags));
    gxm_staging_retire(&pool,&active);
    assert(!active&&pool.count==1&&pool.bytes==n&&live==1);
    gxm_staging_pool_release(&pool);assert(!live);
    pool.max_bytes=4*n;pool.max_count=1;
    for(unsigned i=0;i<3;i++)publish(&active,gxm_staging_acquire(&pool,device,n,flags));
    gxm_staging_retire(&pool,&active);
    assert(!active&&pool.count==1&&pool.bytes==n&&live==1);
    gxm_staging_pool_release(&pool);assert(!live);
    pool.max_count=0;
    publish(&active,gxm_staging_acquire(&pool,device,n,flags));
    gxm_staging_retire(&pool,&active);assert(!active&&!live&&!pool.count);
    gxm_staging_retire(NULL,NULL);gxm_staging_pool_release(NULL);
    assert(created==destroyed+1); /* One intentional failed native create. */
    puts("Staging pool: retirement ownership, exact compatibility, allocation failure, byte/count bounds and cleanup passed.");
}
