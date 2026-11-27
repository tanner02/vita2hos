#include <psp2/gxm.h>
#include <psp2/kernel/sysmem.h>
#include "gxm/driver_memory.h"
#include "modules/SceSysmem.h"
int gxm_driver_memory_size(uint32_t required,uint32_t *size)
{
    if (!size) return SCE_GXM_ERROR_INVALID_POINTER;
    if (!required || required>UINT32_MAX-4095) return SCE_GXM_ERROR_INVALID_VALUE;
    *size=(required+4095)&~4095u;
    return 0;
}
int gxm_driver_memory_acquire(SceUID requested,uint32_t required,GxmDriverMemory *out)
{
    if (!out) return SCE_GXM_ERROR_INVALID_POINTER;
    uint32_t size;
    int result=gxm_driver_memory_size(required,&size);
    if (result) return result;
    bool owned=requested==-1;
    SceUID uid=owned ? sceKernelAllocMemBlock("gxm_render_target",
        SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,size,NULL) : requested;
    if (uid<=0) return owned ? SCE_GXM_ERROR_OUT_OF_MEMORY : SCE_GXM_ERROR_INVALID_VALUE;
    void *base=NULL;
    result=sceKernelGetMemBlockBase(uid,&base);
    if (!result) {
        VitaMemBlockInfo *block=SceSysmem_get_vita_memblock_info_for_addr(base);
        if (!block || block->uid!=uid || block->base!=base || block->size<size)
            result=SCE_GXM_ERROR_INVALID_VALUE;
    }
    if (result) {
        if (owned) sceKernelFreeMemBlock(uid);
        return result;
    }
    *out=(GxmDriverMemory){.uid=uid,.base=base,.owned=owned};
    return 0;
}
int gxm_driver_memory_release(GxmDriverMemory *memory)
{
    if (!memory) return SCE_GXM_ERROR_INVALID_POINTER;
    GxmDriverMemory saved=*memory;
    /* The ownership record may itself live inside the block being freed. */
    *memory=(GxmDriverMemory){0};
    if (saved.owned) {
        int result=sceKernelFreeMemBlock(saved.uid);
        if (result) { *memory=saved;return result; }
    }
    return 0;
}
