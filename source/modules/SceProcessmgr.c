#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/error.h>
#include "modules/SceProcessmgr.h"
#include "modules/SceSysmem.h"
#include "module.h"
#include "log.h"

static atomic_uint process_parameter_address;
static atomic_int guest_process_uid;
static bool process_registered; /* Host lifecycle operations are serialized. */

static bool main_contains_parameters(const LoadImage *image)
{
    const LoadProcessParameters *p=&image->parameters;
    if (!p->guest_address) return !p->structure_size;
    if ((p->guest_address & 3) || p->structure_size<12 || !image->segments) return false;
    for (int i=0;i<image->segment_count;++i) {
        const segment_info_t *s=&image->segments[i];
        if (s->p_type!=PT_LOAD) continue;
        uintptr_t address=(s->p_flags & PF_X) ? s->rx_addr : s->rw_addr;
        if (p->guest_address>=address && p->guest_address-address<s->p_memsz &&
            p->structure_size<=s->p_memsz-(p->guest_address-address)) return true;
    }
    return false;
}

int SceProcessmgr_init(const LoadProcess *process)
{
    if (process_registered || !process || !process->bound || !process->images || !process->count ||
        process->count>LOAD_PROCESS_MAX_MODULES+1 || !process->images[0].prepared ||
        !process->images[0].bound || !main_contains_parameters(&process->images[0]))
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    SceUID uid=SceSysmem_get_next_uid();
    if (uid<=0) return SCE_KERNEL_ERROR_NO_MEMORY;
    atomic_store(&process_parameter_address,process->images[0].parameters.guest_address);
    atomic_store(&guest_process_uid,uid);
    process_registered=true;
    return 0;
}

void SceProcessmgr_finish(void)
{
    atomic_store(&guest_process_uid,0);
    atomic_store(&process_parameter_address,0);
    process_registered=false;
}

EXPORT(SceProcessmgr,0x2BE3E066,const void *,sceKernelGetProcessParam,void)
{
    return (const void *)(uintptr_t)atomic_load(&process_parameter_address);
}

/* SceThreadmgr's descriptor is declared in SceKernelThreadMgr.c. All guest
 * threads belong to this loaded process; Horizon IDs are not Vita UIDs. */
EXPORT(SceThreadmgr,0x9DCB4B7A,SceUID,sceKernelGetProcessId,void)
{
    SceUID uid=atomic_load(&guest_process_uid);
    return uid>0 ? uid : (SceUID)SCE_KERNEL_ERROR_UNKNOWN_UID;
}

/* LittleBigPlanet invokes this during boot; no host abort handler is wired up
 * so a no-op success keeps the guest going. */
EXPORT(SceProcessmgr,0xEB6E50BB,int,sceKernelCallAbortHandler,uint32_t param1,uint32_t param2)
{
    LOG("sceKernelCallAbortHandler param1=0x%x param2=0x%x",param1,param2);
    return 0;
}

DECLARE_LIBRARY(SceProcessmgr,0x2DD91812);
