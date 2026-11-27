#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../source/modules/SceSysmem.c"
#include "../../source/gxm_driver_memory.c"
static unsigned live;
static bool fail_map;
void log_printf(const char *format,...) { (void)format; }
DkMemBlock dk_map_memblock(DkDevice device,void *base,uint32_t size,uint32_t flags)
{
    (void)device;(void)base;assert(size%4096==0);
    if (fail_map) return NULL;
    assert(flags&DkMemBlockFlags_CpuUncached);
    ++live;return malloc(1);
}
void dkMemBlockDestroy(DkMemBlock block) { assert(block && live);--live;free(block); }
int main(void)
{
    assert(!SceSysmem_init(NULL));
    uint32_t size=123;
    assert(!gxm_driver_memory_size(256,&size) && size==4096);
    assert(!gxm_driver_memory_size(4097,&size) && size==8192);
    assert((uint32_t)gxm_driver_memory_size(UINT32_MAX,&size)==SCE_GXM_ERROR_INVALID_VALUE && size==8192);
    GxmDriverMemory memory={0};
    assert(!gxm_driver_memory_acquire(-1,256,&memory) && memory.owned && memory.uid>0 && live==1);
    void *base;assert(!sceKernelGetMemBlockBase(memory.uid,&base) && base==memory.base);
    memset(memory.base,0xa5,256);
    assert(!gxm_driver_memory_release(&memory) && !live && !memory.uid && !memory.base);
    assert(!gxm_driver_memory_acquire(-1,256,&memory));
    GxmDriverMemory *embedded=memory.base;*embedded=memory;
    assert(!gxm_driver_memory_release(embedded) && !live); /* No post-free owner write. */
    memory=(GxmDriverMemory){0};
    SceUID supplied=sceKernelAllocMemBlock("supplied",SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,4096,NULL);
    assert(supplied>0 && !sceKernelGetMemBlockBase(supplied,&base));
    assert(!gxm_driver_memory_acquire(supplied,256,&memory) && !memory.owned && memory.base==base);
    assert(!gxm_driver_memory_release(&memory) && live==1);
    assert(!sceKernelGetMemBlockBase(supplied,&base));
    GxmDriverMemory saved=memory;
    assert(gxm_driver_memory_acquire(supplied,8192,&memory)<0 && !memcmp(&memory,&saved,sizeof(memory)));
    assert(gxm_driver_memory_acquire(0,256,&memory)<0 && !memcmp(&memory,&saved,sizeof(memory)));
    assert(!sceKernelFreeMemBlock(supplied) && !live);
    fail_map=true;
    assert((uint32_t)gxm_driver_memory_acquire(-1,256,&memory)==SCE_GXM_ERROR_OUT_OF_MEMORY && !live);
    fail_map=false;
    assert(!gxm_driver_memory_acquire(-1,256,&memory));
    saved=memory;assert(!sceKernelFreeMemBlock(memory.uid));
    assert(gxm_driver_memory_release(&memory)<0 && !memcmp(&memory,&saved,sizeof(memory)));
    assert((uint32_t)gxm_driver_memory_acquire(-1,256,NULL)==SCE_GXM_ERROR_INVALID_POINTER);
    SceSysmem_finish();assert(!live);
    puts("Driver memory: real Sysmem IDs, page sizing, owned/borrowed lifetime, failure preservation and cleanup passed.");
}
