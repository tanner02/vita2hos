#include <assert.h>

#include "../../source/modules/SceSysmem.c"

static bool fail_mapping;
static unsigned int live_mappings;
static uint32_t mapped_size;

void log_printf(const char *format, ...) { (void)format; }
DkMemBlock dk_map_memblock(DkDevice device, void *addr, uint32_t size, uint32_t flags)
{
    (void)device;
    (void)addr;
    (void)flags;
    mapped_size = size;
    if (fail_mapping)
        return NULL;
    live_mappings++;
    return malloc(1);
}
void dkMemBlockDestroy(DkMemBlock block)
{
    assert(block != NULL && live_mappings > 0);
    live_mappings--;
    free(block);
}

int main(void)
{
    void *base;
    assert(SceSysmem_init(NULL) == 0);
    assert((uint32_t)sceKernelFreeMemBlock(-1) == SCE_KERNEL_ERROR_INVALID_UID);
    assert((uint32_t)sceKernelGetMemBlockBase(-1, &base) == SCE_KERNEL_ERROR_INVALID_UID);
    assert((uint32_t)sceKernelAllocMemBlock("zero", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, 0, NULL) ==
           SCE_KERNEL_ERROR_ILLEGAL_SIZE);
    assert((uint32_t)sceKernelAllocMemBlock("odd", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, 4097, NULL) ==
           SCE_KERNEL_ERROR_ILLEGAL_SIZE);

    SceKernelAllocMemBlockOpt opt = {
        .size = sizeof(opt),
        .attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT,
        .alignment = 65536,
    };
    SceUID uid = sceKernelAllocMemBlock("aligned", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, 4096, &opt);
    assert(uid > 0);
    assert(sceKernelGetMemBlockBase(uid, &base) == 0);
    assert((uintptr_t)base % opt.alignment == 0);
    assert(mapped_size == 4096 && live_mappings == 1);
    for (unsigned int i = 0; i < 4096; i++)
        assert(((unsigned char *)base)[i] == 0);
    VitaMemBlockInfo *block = SceSysmem_get_vita_memblock_info_for_addr(base);
    assert(block && block->uid == uid);
    assert(SceSysmem_get_vita_memblock_info_for_addr((char *)base + 4095) == block);
    assert(SceSysmem_get_vita_memblock_info_for_addr((char *)base + 4096) == NULL);
    assert((uint32_t)sceKernelGetMemBlockBase(uid, NULL) == SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    assert(sceKernelFreeMemBlock(uid) == 0);
    assert(live_mappings == 0);
    assert((uint32_t)sceKernelFreeMemBlock(uid) == SCE_KERNEL_ERROR_INVALID_UID);
    assert((uint32_t)sceKernelGetMemBlockBase(uid, &base) == SCE_KERNEL_ERROR_INVALID_UID);

    opt.alignment = 6000;
    assert((uint32_t)sceKernelAllocMemBlock("bad", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, 4096, &opt) ==
           SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    opt.size = 4;
    assert((uint32_t)sceKernelAllocMemBlock("short", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, 4096, &opt) ==
           SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);

    uid = sceKernelAllocMemBlock("cdram", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, 256 * 1024, NULL);
    assert(uid > 0 && sceKernelGetMemBlockBase(uid, &base) == 0);
    assert((uintptr_t)base % (256 * 1024) == 0);
    assert(sceKernelFreeMemBlock(uid) == 0);

    fail_mapping = true;
    assert((uint32_t)sceKernelAllocMemBlock("map fail", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, 4096, NULL) ==
           SCE_KERNEL_ERROR_NO_MEMORY);
    assert(vita_memblock_info_dict_size(g_vita_memblock_infos) == 0 && live_mappings == 0);
    fail_mapping = false;
    for (unsigned int i = 0; i < 1000; i++) {
        uid = sceKernelAllocMemBlock("cycle", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, 4096, NULL);
        assert(uid > 0 && sceKernelFreeMemBlock(uid) == 0);
    }
    assert(vita_memblock_info_dict_size(g_vita_memblock_infos) == 0 && live_mappings == 0);
    vita_memblock_info_dict_clear(g_vita_memblock_infos);
    puts("SceSysmem regression checks passed.");
    return 0;
}
