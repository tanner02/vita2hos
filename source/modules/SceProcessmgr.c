#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/error.h>
#include "modules/SceProcessmgr.h"
#include "module.h"

static atomic_uint process_parameter_address;
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
    atomic_store(&process_parameter_address,process->images[0].parameters.guest_address);
    process_registered=true;
    return 0;
}

void SceProcessmgr_finish(void)
{
    atomic_store(&process_parameter_address,0);
    process_registered=false;
}

EXPORT(SceProcessmgr,0x2BE3E066,const void *,sceKernelGetProcessParam,void)
{
    return (const void *)(uintptr_t)atomic_load(&process_parameter_address);
}

DECLARE_LIBRARY(SceProcessmgr,0x2DD91812);
