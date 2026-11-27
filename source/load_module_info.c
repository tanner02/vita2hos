#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "load_module_info.h"

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(SceKernelModuleInfo)==0x1b8,"Vita module information ABI");
_Static_assert(sizeof(SceKernelSegmentInfo)==0x18,"Vita segment information ABI");
_Static_assert(offsetof(SceKernelModuleInfo,segments)==0x154,"Vita segment array offset");
#endif

static int exception_range(uint32_t first,uint32_t end,const segment_info_t *segment,
                            LoadExportRead read,void *context,void **top,void **bottom)
{
    if (!first && !end) return 0;
    uint64_t address=(uint64_t)segment->rx_addr+first;
    uint64_t limit=(uint64_t)segment->rx_addr+end;
    if (end<first || end>segment->p_filesz || (first & 3) || (end & 3) ||
        address>UINT32_MAX || limit>UINT32_MAX ||
        !read(context,(uint32_t)address,end-first,false)) return -1;
    *top=(void *)(uintptr_t)address;*bottom=(void *)(uintptr_t)limit;
    return 0;
}

int load_module_info_build(SceKernelModuleInfo *info,const sce_module_info_raw *raw,
                           const segment_info_t *segments,int count,int info_segment,
                           const LoadLifecycle *lifecycle,LoadExportRead read,void *context)
{
    if (!info || !raw || !segments || !lifecycle || !read || count<=0 ||
        info_segment<0 || info_segment>=count || segments[info_segment].p_type!=PT_LOAD)
        return -1;
    SceKernelModuleInfo result={.size=sizeof(result),.state=SCE_KERNEL_MODULE_STATE_READY};
    result.modattr=raw->attributes;
    result.modver[0]=raw->version & 0xff;result.modver[1]=raw->version >> 8;
    memcpy(result.module_name,raw->name,sizeof(raw->name));
    unsigned loaded=0;
    for (int i=0;i<count;++i) {
        const segment_info_t *s=&segments[i];
        if (s->p_type!=PT_LOAD || !s->p_memsz) continue;
        uintptr_t address=(s->p_flags & PF_X) ? s->rx_addr : s->rw_addr;
        if (loaded==4 || !address || address>UINT32_MAX || s->p_filesz>s->p_memsz ||
            (uint64_t)address+s->p_memsz>UINT64_C(0x100000000)) return -1;
        result.segments[loaded++]=(SceKernelSegmentInfo){.size=sizeof(SceKernelSegmentInfo),
            .perms=s->p_flags & (PF_R|PF_W|PF_X),.vaddr=(void *)address,
            .filesz=s->p_filesz,.memsz=s->p_memsz};
    }
    if (!loaded || (uintptr_t)lifecycle->start>UINT32_MAX ||
        (uintptr_t)lifecycle->stop>UINT32_MAX || (uintptr_t)lifecycle->exit>UINT32_MAX)
        return -1;
    result.start_entry=(void *)lifecycle->start;
    result.stop_entry=(void *)lifecycle->stop;
    result.exit_entry=(void *)lifecycle->exit;
    if (raw->tls_filesz>raw->tls_memsz) return -1;
    result.tlsInitSize=raw->tls_filesz;result.tlsAreaSize=raw->tls_memsz;
    if (raw->tls_filesz) {
        const segment_info_t *s=&segments[info_segment];
        uint64_t address=(uint64_t)s->rx_addr+raw->tls_start;
        if (raw->tls_start>s->p_filesz || raw->tls_filesz>s->p_filesz-raw->tls_start ||
            address>UINT32_MAX || !read(context,(uint32_t)address,raw->tls_filesz,false)) return -1;
        result.tlsInit=(void *)(uintptr_t)address;
    }
    const segment_info_t *s=&segments[info_segment];
    if (exception_range(raw->exidx_top,raw->exidx_end,s,read,context,&result.exidx_top,&result.exidx_btm) ||
        exception_range(raw->extab_top,raw->extab_end,s,read,context,&result.extab_top,&result.extab_btm))
        return -1;
    *info=result;
    return 0;
}
