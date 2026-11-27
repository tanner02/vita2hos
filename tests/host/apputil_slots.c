#define _GNU_SOURCE
#define TEST_THREADED_MUTEX
#define CONFIG_H
static char test_root[]="/tmp/vita2hos-apputil-XXXXXX";
#define VITA2HOS_ROOT_PATH test_root
#define VITA2HOS_APP0_PATH test_root
#include <assert.h>
#include <unistd.h>
#include "../../source/savedata_identity.c"
#include "../../source/savedata_slots.c"
static int fail_payload_write,fail_clock;
#define sceRtcGetCurrentClock app_clock
static ssize_t payload_write(int fd,const void *data,size_t size);
#define write payload_write
#include "../../source/modules/SceAppUtil.c"
#undef write
#undef sceRtcGetCurrentClock
int app_clock(SceDateTime *date,int zone)
{
    assert(zone==0);if(fail_clock)return -1;
    *date=(SceDateTime){.year=2026,.month=11,.day=27,.hour=12,.minute=34,.second=56,.microsecond=123456};return 0;
}
static ssize_t payload_write(int fd,const void *data,size_t size)
{if(fail_payload_write){errno=ENOSPC;return -1;}return write(fd,data,size);}
#include "../../source/modules/SceLibKernel.c"
#include "../../source/modules/SceSysmodule.c"
#include "../../source/module.c"
bool SceNet_host_ready(void){return false;}
bool SceNet_is_initialized(void){return false;}
bool SceNpTrophy_is_initialized(void){return false;}
void log_printf(const char *format,...) {(void)format;}
void SceKernelThreadMgr_prepare_current_exit(int status) {(void)status;}
VitaThreadInfo *SceKernelThreadMgr_get_thread_info(void) {return NULL;}
SceUID SceSysmem_get_next_uid(void) {static SceUID next=100;return next++;}
_Static_assert(sizeof(SceAppUtilSaveDataSlotParam)==SAVEDATA_SLOT_BYTES,"slot ABI");
static void put32(unsigned char *p,unsigned value)
{for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(value>>(8*i));}
int main(void)
{
    assert(mkdtemp(test_root));assert(!SceLibKernel_init());
    assert(sceSysmoduleIsLoaded(SCE_SYSMODULE_APPUTIL)==(int)SCE_SYSMODULE_ERROR_UNLOADED);
    assert(!sceSysmoduleLoadModule(SCE_SYSMODULE_APPUTIL));
    assert(!sceSysmoduleLoadModule(SCE_SYSMODULE_APPUTIL));
    assert(!SceAppUtil_is_initialized());
    SceAppUtilInitParam init={0};SceAppUtilBootParam boot={0};
    SceAppUtilSaveDataSlotParam param={0},output={0};param.userParam=42;param.status=17;
    assert(sceAppUtilSaveDataSlotGetParam(1,&output,NULL)==(int)SCE_APPUTIL_ERROR_NOT_INITIALIZED);
    assert(sceAppUtilInit(&init,&boot)==(int)SCE_APPUTIL_ERROR_NOT_MOUNTED);
    char sys[1024],path[1024];snprintf(sys,sizeof(sys),"%s/sce_sys",test_root);assert(!mkdir(sys,0700));
    snprintf(path,sizeof(path),"%s/sce_sys/param.sfo",test_root);
    unsigned char sfo[64]={0};memcpy(sfo,"\0PSF",4);put32(sfo+4,0x101);
    put32(sfo+8,36);put32(sfo+12,48);put32(sfo+16,1);sfo[22]=4;sfo[23]=2;
    put32(sfo+24,10);put32(sfo+28,16);memcpy(sfo+36,"TITLE_ID",9);memcpy(sfo+48,"TEST00001",10);
    FILE *file=fopen(path,"wb");assert(file && fwrite(sfo,1,64,file)==64 && !fclose(file));
    assert(!sceAppUtilInit(&init,&boot));assert(sceAppUtilInit(&init,&boot)==(int)SCE_APPUTIL_ERROR_BUSY);
    assert(SceAppUtil_is_initialized());
    assert(sceSysmoduleUnloadModule(SCE_SYSMODULE_APPUTIL)==(int)SCE_SYSMODULE_ERROR_FATAL);
    assert(!sceSysmoduleIsLoaded(SCE_SYSMODULE_APPUTIL));
    assert(sceAppUtilSaveDataSlotGetParam(1,&output,NULL)==(int)SCE_APPUTIL_ERROR_SAVEDATA_SLOT_NOT_FOUND);
    char staged[1024];assert(!slot_path(staged,save_root,1,true));
    file=fopen(staged,"wb");assert(file && !fclose(file));
    assert(sceAppUtilSaveDataSlotCreate(1,&param,NULL)==(int)SCE_APPUTIL_ERROR_BUSY);
    assert(sceAppUtilSaveDataSlotGetParam(1,&output,NULL)==(int)SCE_APPUTIL_ERROR_SAVEDATA_SLOT_NOT_FOUND);
    assert(!unlink(staged));
    assert(!sceAppUtilSaveDataSlotCreate(1,&param,NULL));
    assert(sceAppUtilSaveDataSlotCreate(1,&param,NULL)==(int)SCE_APPUTIL_ERROR_SAVEDATA_SLOT_EXISTS);
    assert(!sceAppUtilShutdown());assert(!sceAppUtilInit(&init,&boot));
    assert(!sceAppUtilSaveDataSlotGetParam(1,&output,NULL) && output.userParam==42 && output.status==0);
    param.userParam=7;assert(!sceAppUtilSaveDataSlotSetParam(1,&param,NULL));
    assert(!sceAppUtilSaveDataSlotGetParam(1,&output,NULL) && output.userParam==7);
    SceAppUtilSaveDataMountPoint mount={0};
    assert(sceAppUtilSaveDataSlotDelete(1,&mount)==(int)SCE_APPUTIL_ERROR_PARAMETER);
    SceSize required=99;
    SceAppUtilSaveDataFile files[2]={{.filePath="data",.mode=2},
        {.filePath="data/save.bin",.buf="abcdef",.bufSize=6}};
    SceAppUtilSaveDataFileSlot slot={.id=2,.slotParam=&param};
    SceDateTime before_time=param.modifiedTime;
    fail_clock=1;
    assert(sceAppUtilSaveDataDataSave(&slot,files,2,NULL,&required)==(int)0x80010005 && required==99);
    assert(!memcmp(&param.modifiedTime,&before_time,sizeof(before_time)));fail_clock=0;
    char not_created[1024];assert(!savedata_path("data",not_created));assert(access(not_created,F_OK)<0);
    assert(!sceAppUtilSaveDataDataSave(&slot,files,2,NULL,&required) && required==0);
    assert(param.modifiedTime.year==2026 && param.modifiedTime.microsecond==123456);
    assert(!sceAppUtilSaveDataSlotGetParam(2,&output,NULL) && output.userParam==7 && !memcmp(&output.modifiedTime,&param.modifiedTime,sizeof(param.modifiedTime)));
    SceAppUtilSaveDataSlotParam failed_param=param;failed_param.modifiedTime.year=1999;
    SceAppUtilSaveDataFileSlot failed_slot={.id=3,.slotParam=&failed_param};
    char occupied[1024];assert(!slot_path(occupied,save_root,3,true));
    file=fopen(occupied,"wb");assert(file && !fclose(file));required=99;
    assert(sceAppUtilSaveDataDataSave(&failed_slot,NULL,0,NULL,&required)==(int)SCE_APPUTIL_ERROR_BUSY);
    assert(failed_param.modifiedTime.year==1999 && required==99);assert(!unlink(occupied));
    SceUID guest_file=sceIoOpen("savedata0:/data/save.bin",SCE_O_RDONLY,0);assert(guest_file>0);
    assert(!sceAppUtilShutdown());
    assert((uint32_t)sceIoOpen("savedata0:/data/save.bin",SCE_O_RDONLY,0)==SCE_ERROR_ERRNO_ENODEV);
    char reopened[6];assert(sceIoRead(guest_file,reopened,6)==6 && !memcmp(reopened,"abcdef",6));
    assert(!sceIoClose(guest_file));assert(!sceAppUtilInit(&init,&boot));
    assert((uint32_t)sceIoOpen("savedata0:/../escape",SCE_O_RDONLY,0)==SCE_ERROR_ERRNO_EINVAL);
    assert((uint32_t)sceIoOpen("savedata0:/.VITA2HOS-slot-00000002.bin",SCE_O_RDONLY,0)==SCE_ERROR_ERRNO_EINVAL);
    char payload[1024];assert(!savedata_path("data/save.bin",payload));
    files[0]=(SceAppUtilSaveDataFile){.filePath="savedata0:/data/save.bin",.buf="XY",.bufSize=2,.offset=2};
    assert(!sceAppUtilSaveDataDataSave(NULL,files,1,NULL,NULL));
    required=99;fail_payload_write=1;
    assert(sceAppUtilSaveDataDataSave(NULL,files,1,NULL,&required)==(int)SCE_APPUTIL_ERROR_SAVEDATA_NO_SPACE_FS && required==99);
    fail_payload_write=0;
    char contents[7]={0};file=fopen(payload,"rb");assert(file && fread(contents,1,6,file)==6 && !fclose(file));
    assert(!memcmp(contents,"abXYef",6));
    files[1]=(SceAppUtilSaveDataFile){.filePath="../escape",.mode=2};required=99;
    files[0].buf="ZZ";
    assert(sceAppUtilSaveDataDataSave(NULL,files,2,NULL,&required)==(int)SCE_APPUTIL_ERROR_PARAMETER && required==99);
    file=fopen(payload,"rb");assert(file && fread(contents,1,6,file)==6 && !fclose(file));assert(!memcmp(contents,"abXYef",6));
    files[0].mode=1;files[0].buf="Z";files[0].bufSize=1;
    assert(!sceAppUtilSaveDataDataSave(NULL,files,1,NULL,NULL));
    file=fopen(payload,"rb");assert(file && fread(contents,1,7,file)==3 && !fclose(file));assert(!memcmp(contents,"abZ",3));
    files[0].filePath=".vita2hos-slot-00000001.bin";
    assert(sceAppUtilSaveDataDataSave(NULL,files,1,NULL,NULL)==(int)SCE_APPUTIL_ERROR_PARAMETER);
    files[0].filePath=".VITA2HOS-slot-00000001.bin";
    assert(sceAppUtilSaveDataDataSave(NULL,files,1,NULL,NULL)==(int)SCE_APPUTIL_ERROR_PARAMETER);
    char alias[1024];assert(!savedata_path("alias",alias));assert(!symlink(sys,alias));
    files[0].filePath="alias/param.sfo";
    assert(sceAppUtilSaveDataDataSave(NULL,files,1,NULL,NULL)==(int)SCE_APPUTIL_ERROR_NO_PERMISSION);
    assert(!unlink(alias));
    files[0].filePath="data/save.bin";files[0].offset=-1;
    assert(sceAppUtilSaveDataDataSave(NULL,files,1,NULL,NULL)==(int)SCE_APPUTIL_ERROR_PARAMETER);
    assert(!unlink(payload));assert(!savedata_path("data",payload));assert(!rmdir(payload));
    assert(!sceAppUtilSaveDataSlotDelete(2,NULL));
    assert(!sceAppUtilSaveDataSlotDelete(1,NULL));
    assert(module_get_func_export(0x48e01d74,0x93f0d89f)==(void *)sceAppUtilSaveDataSlotGetParam);
    char root[1024];memcpy(root,save_root,sizeof(root));SceAppUtil_finish();
    assert(sceAppUtilShutdown()==(int)SCE_APPUTIL_ERROR_NOT_INITIALIZED);
    assert(!sceSysmoduleUnloadModule(SCE_SYSMODULE_APPUTIL));
    assert(sceSysmoduleIsLoaded(SCE_SYSMODULE_APPUTIL)==(int)SCE_SYSMODULE_ERROR_UNLOADED);
    assert(!rmdir(root));snprintf(root,sizeof(root),"%s/savedata",test_root);assert(!rmdir(root));
    assert(!unlink(path) && !rmdir(sys) && !rmdir(test_root));
    puts("AppUtil slots: missing mount, initialization retry, persistent reopen, SDK errors, NID and cleanup passed.");
}
