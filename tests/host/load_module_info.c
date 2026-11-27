#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../source/load_module_info.c"
static unsigned char bytes[512];
static const void *read_guest(void *context,uint32_t address,size_t size,bool executable)
{
    assert(context==bytes);(void)executable;
    if (address<0x81000000 || address-0x81000000>sizeof(bytes) ||
        size>sizeof(bytes)-(address-0x81000000)) return NULL;
    return bytes+(address-0x81000000);
}
int main(void)
{
    segment_info_t segments[5]={
        {.p_type=PT_LOAD,.rx_addr=0x81000000,.rw_addr=0x10000000,.p_filesz=512,.p_memsz=768,.p_flags=PF_R|PF_X},
        {.p_type=PT_SCE_RELA},
        {.p_type=PT_LOAD,.rx_addr=0x82000000,.rw_addr=0x82000000,.p_filesz=16,.p_memsz=64,.p_flags=PF_R|PF_W},
    };
    sce_module_info_raw raw={.attributes=0x1234,.version=0x0201,.tls_start=256,.tls_filesz=4,.tls_memsz=16,
        .exidx_top=128,.exidx_end=144,.extab_top=144,.extab_end=160};
    memset(raw.name,'X',sizeof(raw.name));
    LoadLifecycle life={.start=(void *)0x81000001,.stop=(void *)0x81000011,.exit=(void *)0x81000021};
    SceKernelModuleInfo info;
    assert(!load_module_info_build(&info,&raw,segments,3,0,&life,read_guest,bytes));
    assert(info.size==sizeof(info) && info.state==SCE_KERNEL_MODULE_STATE_READY);
    assert(info.modattr==0x1234 && info.modver[0]==1 && info.modver[1]==2);
    assert(info.module_name[26]=='X' && !info.module_name[27] && !info.path[0]);
    assert(info.start_entry==life.start && info.stop_entry==life.stop && info.exit_entry==life.exit);
    assert(info.segments[0].vaddr==(void *)0x81000000 && info.segments[0].memsz==768 && info.segments[0].filesz==512);
    assert(info.segments[1].vaddr==(void *)0x82000000 && info.segments[1].memsz==64 && !info.segments[2].memsz);
    assert(info.segments[0].perms==5 && info.segments[1].perms==6);
    assert(info.tlsInit==(void *)0x81000100 && info.tlsInitSize==4 && info.tlsAreaSize==16);
    assert(info.exidx_top==(void *)0x81000080 && info.extab_btm==(void *)0x810000a0);
    SceKernelModuleInfo saved=info;
    raw.exidx_end=124;
    assert(load_module_info_build(&info,&raw,segments,3,0,&life,read_guest,bytes)<0);
    assert(!memcmp(&info,&saved,sizeof(info)));
    raw.exidx_end=513;
    assert(load_module_info_build(&info,&raw,segments,3,0,&life,read_guest,bytes)<0);
    raw.exidx_end=144;raw.exidx_top=0x81000080; /* Absolute addresses are not offsets. */
    assert(load_module_info_build(&info,&raw,segments,3,0,&life,read_guest,bytes)<0);
    raw.exidx_top=128;raw.tls_start=510;
    assert(load_module_info_build(&info,&raw,segments,3,0,&life,read_guest,bytes)<0);
    raw.tls_start=256;raw.tls_memsz=3;
    assert(load_module_info_build(&info,&raw,segments,3,0,&life,read_guest,bytes)<0);
    raw.tls_memsz=16;
    assert(load_module_info_build(&info,&raw,segments,3,1,&life,read_guest,bytes)<0);
    segments[2].p_filesz=65;
    assert(load_module_info_build(&info,&raw,segments,3,0,&life,read_guest,bytes)<0);
    segments[2].p_filesz=16;segments[2].rw_addr=0xfffffff0;
    assert(load_module_info_build(&info,&raw,segments,3,0,&life,read_guest,bytes)<0);
    segments[2].rw_addr=0x82000000;
    segments[1]=segments[3]=segments[4]=segments[2];
    assert(load_module_info_build(&info,&raw,segments,5,0,&life,read_guest,bytes)<0);
    raw=(sce_module_info_raw){.tls_memsz=16};
    assert(!load_module_info_build(&info,&raw,segments,1,0,&life,read_guest,bytes));
    assert(!info.tlsInit && !info.tlsInitSize && info.tlsAreaSize==16 && !info.exidx_top && !info.extab_top);
    puts("Loaded module metadata: guest aliases, TLS, relative exception tables and bounded ABI fields passed.");
}
