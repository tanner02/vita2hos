#include <deko3d.h>
#include <m-dict.h>
#include <psp2/kernel/error.h>
#include <psp2/kernel/sysmem.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include <switch.h>

#include "modules/SceSysmem.h"
#include "deko_utils.h"
#include "log.h"
#include "module.h"
#include "util.h"

static _Atomic SceUID g_last_uid = 1;
static DkDevice g_dk_device;

DICT_DEF2(vita_memblock_info_dict, SceUID, M_DEFAULT_OPLIST, VitaMemBlockInfo *, M_POD_OPLIST)
static vita_memblock_info_dict_t g_vita_memblock_infos;
static RwLock g_vita_memblock_infos_lock;

/* The caller holds the table lock while using the returned record. */
static VitaMemBlockInfo *get_memblock_info_for_uid_locked(SceUID uid)
{
    VitaMemBlockInfo *block;
    VitaMemBlockInfo **entry;

    entry = vita_memblock_info_dict_get(g_vita_memblock_infos, uid);
    block = entry ? *entry : NULL;

    return block;
}

static VitaMemBlockInfo *get_memblock_info_for_range_locked(const void *addr, SceSize size)
{
    uintptr_t address=(uintptr_t)addr;
    vita_memblock_info_dict_it_t it;
    for (vita_memblock_info_dict_it(it,g_vita_memblock_infos);
         !vita_memblock_info_dict_end_p(it);vita_memblock_info_dict_next(it)) {
        VitaMemBlockInfo *block=vita_memblock_info_dict_ref(it)->value;
        uintptr_t base=(uintptr_t)block->base;
        /* A zero length still requires an address inside the allocation.
         * Subtraction avoids overflow at either end of the address space. */
        if (address>=base && address-base<block->size && size<=block->size-(address-base))
            return block;
    }
    return NULL;
}

EXPORT(SceSysmem, 0xA33B99D1, SceUID, sceKernelFindMemBlockByAddr, const void *addr,
       SceSize size)
{
    rwlockReadLock(&g_vita_memblock_infos_lock);
    VitaMemBlockInfo *block=get_memblock_info_for_range_locked(addr,size);
    SceUID result=block ? block->uid : (SceUID)SCE_KERNEL_ERROR_BLOCK_ERROR;
    rwlockReadUnlock(&g_vita_memblock_infos_lock);
    return result;
}

EXPORT(SceSysmem, 0xB9D5EBDE, SceUID, sceKernelAllocMemBlock, const char *name,
       SceKernelMemBlockType type, SceSize size, SceKernelAllocMemBlockOpt *opt)
{
    VitaMemBlockInfo *block;
    uint32_t alignment;
    void *base;
    uint32_t memblock_flags;
    size_t allocation_size;

    LOG("sceKernelAllocMemBlock: name: %s, size: 0x%x", name, size);

    if (type == SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW)
        alignment = 256 * 1024;
    else
        alignment = 4 * 1024;

    if (size == 0 || size % alignment != 0)
        return SCE_KERNEL_ERROR_ILLEGAL_SIZE;

    if (opt) {
        if (opt->size < offsetof(SceKernelAllocMemBlockOpt, alignment) + sizeof(opt->alignment))
            return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
        if (opt->attr & SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT) {
            if (opt->alignment == 0 || (opt->alignment & (opt->alignment - 1)) != 0)
                return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
            alignment = MAX2(alignment, opt->alignment);
        }
    }

    const size_t padding = (size_t)alignment - 1;
    if (SIZE_MAX - (size_t)size < padding)
        return SCE_KERNEL_ERROR_NO_MEMORY;
    allocation_size = ((size_t)size + alignment - 1) & ~((size_t)alignment - 1);

    block = malloc(sizeof(*block));
    if (!block)
        return SCE_KERNEL_ERROR_NO_MEMORY;

    base = aligned_alloc(alignment, allocation_size);
    if (!base) {
        free(block);
        return SCE_KERNEL_ERROR_NO_MEMORY;
    }

    memset(block, 0, sizeof(*block));
    block->uid = SceSysmem_get_next_uid();
    block->base = base;
    block->size = size;
    memset(base, 0, size);

    switch (type) {
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RW:
        memblock_flags = DkMemBlockFlags_CpuCached | DkMemBlockFlags_GpuCached;
        break;
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_RW:
    case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW:
    default:
        memblock_flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
        break;
    }

    /* We also map all the allocated memory blocks to the GPU */
    block->dk_memblock = dk_map_memblock(g_dk_device, block->base, block->size,
                                         memblock_flags | DkMemBlockFlags_Image);
    if (!block->dk_memblock) {
        free(block->base);
        free(block);
        return SCE_KERNEL_ERROR_NO_MEMORY;
    }

    const SceUID uid=block->uid;
    rwlockWriteLock(&g_vita_memblock_infos_lock);
    vita_memblock_info_dict_set_at(g_vita_memblock_infos, block->uid, block);
    rwlockWriteUnlock(&g_vita_memblock_infos_lock);

    return uid;
}

EXPORT(SceSysmem, 0xA91E15EE, int, sceKernelFreeMemBlock, SceUID uid)
{
    rwlockWriteLock(&g_vita_memblock_infos_lock);
    VitaMemBlockInfo *block = get_memblock_info_for_uid_locked(uid);
    if (!block) {
        rwlockWriteUnlock(&g_vita_memblock_infos_lock);
        return SCE_KERNEL_ERROR_INVALID_UID;
    }
    vita_memblock_info_dict_erase(g_vita_memblock_infos, uid);
    rwlockWriteUnlock(&g_vita_memblock_infos_lock);
    dkMemBlockDestroy(block->dk_memblock);
    free(block->base);
    free(block);

    return 0;
}

EXPORT(SceSysmem, 0xB8EF5818, int, sceKernelGetMemBlockBase, SceUID uid, void **base)
{
    rwlockReadLock(&g_vita_memblock_infos_lock);
    VitaMemBlockInfo *block = get_memblock_info_for_uid_locked(uid);
    int result=0;
    if (!block) result=SCE_KERNEL_ERROR_INVALID_UID;
    else if (!base) result=SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    else *base=block->base;
    rwlockReadUnlock(&g_vita_memblock_infos_lock);
    return result;
}

DECLARE_LIBRARY(SceSysmem, 0x37fe725a);

int SceSysmem_init(DkDevice dk_device)
{
    g_dk_device = dk_device;
    vita_memblock_info_dict_init(g_vita_memblock_infos);
    rwlockInit(&g_vita_memblock_infos_lock);

    return 0;
}

/* Called after guest execution and the render/presenter queues have stopped.
 * Guest-owned blocks may intentionally outlive module_start (for example a
 * displayed framebuffer). Reclaim their GPU mappings before device destruction. */
void SceSysmem_finish(void)
{
    rwlockWriteLock(&g_vita_memblock_infos_lock);
    vita_memblock_info_dict_it_t it;
    for (vita_memblock_info_dict_it(it, g_vita_memblock_infos);
         !vita_memblock_info_dict_end_p(it); vita_memblock_info_dict_next(it)) {
        VitaMemBlockInfo *block = vita_memblock_info_dict_ref(it)->value;
        dkMemBlockDestroy(block->dk_memblock);
        free(block->base);
        free(block);
    }
    vita_memblock_info_dict_clear(g_vita_memblock_infos);
    g_dk_device = NULL;
    rwlockWriteUnlock(&g_vita_memblock_infos_lock);
}

SceUID SceSysmem_get_next_uid(void)
{
    /* Keep generated object IDs clear of the SDK pseudo-handle range, including
     * the process-wide thread-event selector 0x10027. */
    for (;;) {
        SceUID uid=atomic_fetch_add(&g_last_uid,1);
        if ((uint32_t)uid<0x10000 || (uint32_t)uid>0x100ff) return uid;
    }
}

VitaMemBlockInfo *SceSysmem_get_vita_memblock_info_for_addr(const void *addr)
{
    rwlockReadLock(&g_vita_memblock_infos_lock);
    VitaMemBlockInfo *block=get_memblock_info_for_range_locked(addr,0);
    rwlockReadUnlock(&g_vita_memblock_infos_lock);

    return block;
}

DkMemBlock SceSysmem_get_dk_memblock_for_addr(const void *addr)
{
    rwlockReadLock(&g_vita_memblock_infos_lock);
    VitaMemBlockInfo *info=get_memblock_info_for_range_locked(addr,0);
    DkMemBlock result=info ? info->dk_memblock : NULL;
    rwlockReadUnlock(&g_vita_memblock_infos_lock);
    return result;
}
