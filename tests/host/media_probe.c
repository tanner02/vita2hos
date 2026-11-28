#define _POSIX_C_SOURCE 200809L
#define TEST_THREADED_MUTEX
#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
static char flag_path[512],report_path[512];
#define MEDIA_PROBE_FLAG flag_path
#define MEDIA_PROBE_REPORT report_path
#include "../../source/media_probe.c"
#include "../../source/modules/SceSysmodule.c"
#include "../../source/modules/SceMp4.c"
#include "../../source/missing_imports.c"
#include "../../source/module.c"
bool SceNet_host_ready(void){return false;}
bool SceNet_is_initialized(void){return false;}
bool SceNpTrophy_is_initialized(void){return false;}
bool SceAppUtil_is_initialized(void){return false;}
void log_printf(const char *format,...){(void)format;}
uintptr_t missing_import_stub_address(unsigned index){return UINT32_C(0x70000000)+index*4;}
static size_t read_report(char *out,size_t capacity)
{
    FILE *file=fopen(report_path,"rb");assert(file);
    size_t n=fread(out,1,capacity-1,file);assert(!ferror(file));assert(!fclose(file));out[n]=0;return n;
}
static void *writer(void *opaque)
{
    unsigned id=(unsigned)(uintptr_t)opaque;
    for(unsigned i=0;i<300;i++)media_probe_record("ROW thread=%u value=%u",id,i);
    return NULL;
}
int main(void)
{
    char dir[]="/tmp/codex-media-probe-XXXXXX";assert(mkdtemp(dir));
    assert(snprintf(flag_path,sizeof(flag_path),"%s/enabled",dir)>0);
    assert(snprintf(report_path,sizeof(report_path),"%s/report.txt",dir)>0);
    assert(!media_probe_active());
    assert(sceSysmoduleLoadModule(SCE_SYSMODULE_MP4)==(int)SCE_SYSMODULE_ERROR_FATAL);
    assert(sceSysmoduleIsLoaded(SCE_SYSMODULE_MP4)==(int)SCE_SYSMODULE_ERROR_UNLOADED);
    media_probe_record("disabled");assert(access(report_path,F_OK));
    FILE *flag=fopen(flag_path,"wb");assert(flag && !fclose(flag));
    assert(!media_probe_active()); /* sampled once per lifecycle */
    SceSysmodule_finish();assert(media_probe_active());
    assert(!sceSysmoduleLoadModule(SCE_SYSMODULE_MP4));
    assert(!sceSysmoduleIsLoaded(SCE_SYSMODULE_MP4));
    assert(!sceSysmoduleUnloadModule(SCE_SYSMODULE_MP4));
    assert(sceSysmoduleIsLoaded(SCE_SYSMODULE_MP4)==(int)SCE_SYSMODULE_ERROR_UNLOADED);
    uint32_t (*open_export)(uint32_t,uint32_t,uint32_t,uint32_t)=
        module_get_func_export(0x8feacabd,0x05474af0);
    assert(open_export==(void *)sceMp4OpenFile);
    uint32_t guard[4]={0x13579bdf,0x2468ace0,0x12345678,0xa5a55a5a},before_guard[4];
    memcpy(before_guard,guard,sizeof(guard));
    assert(open_export((uint32_t)(uintptr_t)guard,0,UINT32_MAX,0x8002d00b)==0);
    assert(!memcmp(guard,before_guard,sizeof(guard)));
    assert(open_export(0,1,2,3)==0);
    unsigned owner=0;
    assert(missing_import_bind(&owner,0x8feacabd,0x05474af0,0x8177bad0)==missing_import_stub_address(0));
    assert(missing_import_report(0,0x8153470f,0x11,0x22,0x33,0x44)==(int)SCE_KERNEL_ERROR_MODULEMGR_NO_FUNC_NID);
    unsigned before=records;
    assert(missing_import_report(0,0x8153470f,0x11,0x22,0x33,0x44)==(int)SCE_KERNEL_ERROR_MODULEMGR_NO_FUNC_NID);
    assert(records==before); /* first-use only, not one record per invocation */
    char output[65536];read_report(output,sizeof(output));
    assert(strstr(output,"SYSMODULE_LOAD id=0x002d result=0x00000000"));
    assert(strstr(output,"module_presence_only=1 decoder_implemented=0"));
    assert(strstr(output,"MP4_OPEN_UNAVAILABLE result=0x00000000"));
    assert(strstr(output,"library=0x8feacabd nid=0x05474af0 stub=0x8177bad0"));
    assert(strstr(output,"return_address=0x8153470f r0=0x00000011 r1=0x00000022 r2=0x00000033 r3=0x00000044"));
    assert(strstr(output,"result=0x8002d00b"));
    missing_import_release(&owner);assert(!unlink(report_path));media_probe_finish();
    pthread_t threads[4];for(unsigned i=0;i<4;i++)assert(!pthread_create(&threads[i],NULL,writer,(void*)(uintptr_t)i));
    for(unsigned i=0;i<4;i++)assert(!pthread_join(threads[i],NULL));
    size_t n=read_report(output,sizeof(output));unsigned lines=0;
    for(size_t i=0;i<n;i++)if(output[i]=='\n')lines++;
    assert(lines==MEDIA_PROBE_LIMIT && records==MEDIA_PROBE_LIMIT);
    assert(!unlink(report_path));media_probe_finish();assert(!mkdir(report_path,0700));
    assert(!sceSysmoduleLoadModule(SCE_SYSMODULE_MP4)); /* report IO fails; result unchanged */
    assert(!sceSysmoduleIsLoaded(SCE_SYSMODULE_MP4));
    assert(!rmdir(report_path));assert(!unlink(flag_path));SceSysmodule_finish();
    assert(!media_probe_active());assert(sceSysmoduleLoadModule(SCE_SYSMODULE_MP4)==(int)SCE_SYSMODULE_ERROR_FATAL);
    assert(!rmdir(dir));
    puts("Media probe: opt-in module gate, first-use unsupported error/arguments, bounded concurrent records, IO failure and lifecycle reset passed.");
}
