#define TEST_FIXED_MEMORY
#define TEST_THREADED_RWLOCK
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../source/modules/SceModulemgr.c"
#include "../../source/module.c"
#include "../../source/load_process_run.c"
/* This unit test isolates loader ordering; the threaded integration test uses
 * the production thread runner, TLS and explicit exits. */
int SceKernelThreadMgr_run_module_start(SceKernelThreadEntry entry) { return entry(0,NULL); }

DECLARE_LIBRARY(SceLibKernel,0xcae9ace6);
SceUID SceSysmem_get_next_uid(void) { static SceUID uid=100;return uid++; }
void log_printf(const char *format,...) { (void)format; }
static int dependency(uint32_t args,void *argp)
{
    assert(!args && !argp);SceKernelModuleInfo info={.size=sizeof(info)};
    assert(!sceKernelGetModuleInfoByAddr((void *)0x81010000,&info));
    assert(info.state==SCE_KERNEL_MODULE_STATE_READY);return 0;
}
static int main_entry(uint32_t args,void *argp)
{
    assert(!args && !argp);SceKernelModuleInfo info={.size=sizeof(info)};
    assert(!sceKernelGetModuleInfoByAddr((void *)0x81000000,&info));
    assert(info.state==SCE_KERNEL_MODULE_STATE_STARTED);
    assert(!sceKernelGetModuleInfoByAddr((void *)0x81010000,&info));
    assert(info.state==SCE_KERNEL_MODULE_STATE_STARTED);return 321;
}
int main(void)
{
    SceModulemgr_finish();
    assert((uint32_t)sceKernelGetModuleIdByAddr((void *)0x81000000)==SCE_KERNEL_ERROR_MODULEMGR_NOENT);
    LoadImage images[2]={0};LoadProcess process={.bound=true,.images=images,.count=2};
    for (unsigned i=0;i<2;++i) {
        images[i].prepared=images[i].bound=true;
        SceKernelModuleInfo *m=&images[i].module_info;
        m->size=sizeof(*m);snprintf(m->module_name,sizeof(m->module_name),"image%u",i);
        strcpy(m->path,"/vita2hos/executable");
        m->segments[0]=(SceKernelSegmentInfo){.size=sizeof(SceKernelSegmentInfo),.perms=5,
            .vaddr=(void *)(uintptr_t)(0x81000000+i*0x10000),.memsz=256,.filesz=128};
        m->start_entry=m->segments[0].vaddr;
        m->tlsInit=(char *)m->segments[0].vaddr+64;m->tlsInitSize=4;m->tlsAreaSize=16;
        atomic_store(&images[i].module_state,SCE_KERNEL_MODULE_STATE_READY);
    }
    assert(SceModulemgr_init(NULL)<0);
    images[1].bound=false;assert(SceModulemgr_init(&process)<0 && !loaded_modules);
    images[1].bound=true;assert(!SceModulemgr_init(&process));
    SceUID ids[2];
    for (unsigned i=0;i<2;++i) {
        void *address=images[i].module_info.segments[0].vaddr;
        ids[i]=sceKernelGetModuleIdByAddr(address);assert(ids[i]>0);
        assert(sceKernelGetModuleIdByAddr((char *)address+255)==ids[i]);
        assert((uint32_t)sceKernelGetModuleIdByAddr((char *)address+256)==SCE_KERNEL_ERROR_MODULEMGR_NOENT);
        SceKernelModuleInfo info={.size=sizeof(info)};
        assert(!sceKernelGetModuleInfoByAddr((char *)address+1,&info));
        assert(info.modid==ids[i] && info.state==SCE_KERNEL_MODULE_STATE_READY);
        assert(!strcmp(info.module_name,images[i].module_info.module_name));
        assert(info.segments[0].vaddr==address && info.tlsInit==(char *)address+64 && info.tlsAreaSize==16);
        atomic_store(&images[i].module_state,SCE_KERNEL_MODULE_STATE_STARTED);
        assert(!sceKernelGetModuleInfo(ids[i],&info) && info.state==SCE_KERNEL_MODULE_STATE_STARTED);
        /* Registry metadata is a snapshot; state remains an atomic live value. */
        images[i].module_info.module_name[0]='?';
        assert(!sceKernelGetModuleInfo(ids[i],&info) && info.module_name[0]=='i');
        /* Libc TLS registration passes output-only stack storage: no size
         * word is initialized before the address-based query. */
        memset(&info,0xa5,sizeof(info));
        assert(!sceKernelGetModuleInfoByAddr((char *)address+1,&info));
        assert(info.size==sizeof(info) && info.modid==ids[i]);
        assert(info.tlsInit==(char *)address+64 && info.tlsInitSize==4 && info.tlsAreaSize==16);
        memset(&info,0,sizeof(info));
        assert(!sceKernelGetModuleInfoByAddr((char *)address+1,&info));
        SceKernelModuleInfo unknown;memset(&unknown,0x5a,sizeof(unknown));
        SceKernelModuleInfo unchanged=unknown;
        assert((uint32_t)sceKernelGetModuleInfoByAddr((void *)0x70000000,&unknown)==SCE_KERNEL_ERROR_MODULEMGR_NOENT);
        assert(!memcmp(&unknown,&unchanged,sizeof(unknown)));
        SceKernelModuleInfo saved=info;info.size=0;
        assert((uint32_t)sceKernelGetModuleInfo(ids[i],&info)==SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);
        info=saved;
        assert((uint32_t)sceKernelGetModuleInfo(-1,&info)==SCE_KERNEL_ERROR_LIBRARYDB_NO_MOD);
        assert(!memcmp(&info,&saved,sizeof(info)));
    }
    assert(ids[0]!=ids[1]);assert(SceModulemgr_init(&process)<0);
    assert(sceKernelGetModuleIdByAddr((void *)0x81000000)==ids[0]);
    atomic_store(&images[0].module_state,SCE_KERNEL_MODULE_STATE_READY);
    atomic_store(&images[1].module_state,SCE_KERNEL_MODULE_STATE_READY);
    images[1].lifecycle.start=dependency;process.entry=main_entry;
    assert(load_process_run(&process,0,NULL)==321 && process.modules_started==1);
    SceKernelModuleInfo info={.size=sizeof(info)};
    assert(!sceKernelGetModuleInfo(ids[0],&info) && info.state==SCE_KERNEL_MODULE_STATE_ENDED);
    assert((uint32_t)sceKernelGetModuleInfoByAddr(NULL,&info)==SCE_KERNEL_ERROR_MODULEMGR_NOENT);
    assert((uint32_t)sceKernelGetModuleInfoByAddr((void *)0x81000000,NULL)==SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    assert(module_get_func_export(0xcae9ace6,0xD11A5103));
    assert(module_get_func_export(0xEAED1616,0xF5798C7C) && module_get_func_export(0xEAED1616,0x36585DAF));
    SceModulemgr_finish();assert(!loaded_modules && !loaded_count);
    assert((uint32_t)sceKernelGetModuleInfo(ids[0],&info)==SCE_KERNEL_ERROR_LIBRARYDB_NO_MOD);
    assert(!SceModulemgr_init(&process));
    assert(sceKernelGetModuleIdByAddr((void *)0x81000000)!=ids[0]);SceModulemgr_finish();
    pthread_rwlock_destroy(&module_lock);
    puts("Module queries: guest ranges, IDs, atomic state, metadata snapshots and registry teardown passed.");
}
