#include <stdlib.h>
#include <string.h>
#include <psp2/kernel/error.h>
#include <psp2/kernel/modulemgr.h>
#include "modules/SceModulemgr.h"
#include "modules/SceSysmem.h"
#include "module.h"

typedef struct { SceKernelModuleInfo info; const LoadImage *image; } LoadedModule;
static LoadedModule *loaded_modules;
static size_t loaded_count;
static RwLock module_lock;
static bool module_lock_initialized;

int SceModulemgr_init(const LoadProcess *process)
{
    if (!process || !process->bound || !process->images || !process->count ||
        process->count>LOAD_PROCESS_MAX_MODULES+1) return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    if (!module_lock_initialized) {
        rwlockInit(&module_lock);module_lock_initialized=true;
    }
    LoadedModule *pending=calloc(process->count,sizeof(*pending));
    if (!pending) return SCE_KERNEL_ERROR_NO_MEMORY;
    for (size_t i=0;i<process->count;++i) {
        const LoadImage *image=&process->images[i];
        if (!image->prepared || !image->bound || image->module_info.size!=sizeof(image->module_info)) {
            free(pending);return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
        }
        pending[i].info=image->module_info;
        pending[i].info.modid=SceSysmem_get_next_uid();
        pending[i].image=image;
    }
    rwlockWriteLock(&module_lock);
    if (loaded_modules) {
        rwlockWriteUnlock(&module_lock);free(pending);return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }
    loaded_modules=pending;loaded_count=process->count;
    rwlockWriteUnlock(&module_lock);
    return 0;
}

void SceModulemgr_finish(void)
{
    if (!module_lock_initialized) return;
    rwlockWriteLock(&module_lock);
    free(loaded_modules);loaded_modules=NULL;loaded_count=0;
    rwlockWriteUnlock(&module_lock);
}

static const LoadedModule *find_module_address(const void *address)
{
    uintptr_t addr=(uintptr_t)address;
    for (size_t i=0;i<loaded_count;++i) for (unsigned j=0;j<4;++j) {
        const SceKernelSegmentInfo *s=&loaded_modules[i].info.segments[j];
        uintptr_t base=(uintptr_t)s->vaddr;
        if (addr>=base && addr-base<s->memsz) return &loaded_modules[i];
    }
    return NULL;
}

static int copy_module_info(const LoadedModule *module,SceKernelModuleInfo *info)
{
    *info=module->info;
    info->state=atomic_load(&module->image->module_state);
    return 0;
}

EXPORT(SceLibKernel, 0xD11A5103, int, sceKernelGetModuleInfoByAddr, void *address,
       SceKernelModuleInfo *info)
{
    if (!info) return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    /* This address-based query writes the complete output structure. Libc's
     * TLS registrar passes uninitialized storage here, unlike the UID-based
     * sceKernelGetModuleInfo query which has an input size contract. */
    if (!module_lock_initialized) return SCE_KERNEL_ERROR_MODULEMGR_NOENT;
    rwlockReadLock(&module_lock);
    const LoadedModule *module=find_module_address(address);
    int result;
    if (module) result=copy_module_info(module,info);
    else result=SCE_KERNEL_ERROR_MODULEMGR_NOENT;
    rwlockReadUnlock(&module_lock);
    return result;
}

EXPORT(SceModulemgr, 0xF5798C7C, SceUID, sceKernelGetModuleIdByAddr, void *address)
{
    if (!module_lock_initialized) return SCE_KERNEL_ERROR_MODULEMGR_NOENT;
    rwlockReadLock(&module_lock);
    const LoadedModule *module=find_module_address(address);
    SceUID result=module ? module->info.modid : (SceUID)SCE_KERNEL_ERROR_MODULEMGR_NOENT;
    rwlockReadUnlock(&module_lock);
    return result;
}

EXPORT(SceModulemgr, 0x36585DAF, int, sceKernelGetModuleInfo, SceUID uid,SceKernelModuleInfo *info)
{
    if (!info) return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    if (info->size!=sizeof(*info)) return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
    if (!module_lock_initialized) return SCE_KERNEL_ERROR_LIBRARYDB_NO_MOD;
    rwlockReadLock(&module_lock);
    int result=SCE_KERNEL_ERROR_LIBRARYDB_NO_MOD;
    for (size_t i=0;i<loaded_count;++i) if (loaded_modules[i].info.modid==uid) {
        result=copy_module_info(&loaded_modules[i],info);break;
    }
    rwlockReadUnlock(&module_lock);
    return result;
}

DECLARE_LIBRARY(SceModulemgr,0xEAED1616);
