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
#include "../../source/modules/SceAppUtil.c"
#include "../../source/module.c"
_Static_assert(sizeof(SceAppUtilSaveDataSlotParam)==SAVEDATA_SLOT_BYTES,"slot ABI");
static void put32(unsigned char *p,unsigned value)
{for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(value>>(8*i));}
int main(void)
{
    assert(mkdtemp(test_root));
    SceAppUtilInitParam init={0};SceAppUtilBootParam boot={0};
    SceAppUtilSaveDataSlotParam param={0},output={0};param.userParam=42;
    assert(sceAppUtilSaveDataSlotGetParam(1,&output,NULL)==(int)SCE_APPUTIL_ERROR_NOT_INITIALIZED);
    assert(sceAppUtilInit(&init,&boot)==(int)SCE_APPUTIL_ERROR_NOT_MOUNTED);
    char sys[1024],path[1024];snprintf(sys,sizeof(sys),"%s/sce_sys",test_root);assert(!mkdir(sys,0700));
    snprintf(path,sizeof(path),"%s/sce_sys/param.sfo",test_root);
    unsigned char sfo[64]={0};memcpy(sfo,"\0PSF",4);put32(sfo+4,0x101);
    put32(sfo+8,36);put32(sfo+12,48);put32(sfo+16,1);sfo[22]=4;sfo[23]=2;
    put32(sfo+24,10);put32(sfo+28,16);memcpy(sfo+36,"TITLE_ID",9);memcpy(sfo+48,"TEST00001",10);
    FILE *file=fopen(path,"wb");assert(file && fwrite(sfo,1,64,file)==64 && !fclose(file));
    assert(!sceAppUtilInit(&init,&boot));assert(sceAppUtilInit(&init,&boot)==(int)SCE_APPUTIL_ERROR_BUSY);
    assert(sceAppUtilSaveDataSlotGetParam(1,&output,NULL)==(int)SCE_APPUTIL_ERROR_SAVEDATA_SLOT_NOT_FOUND);
    assert(!sceAppUtilSaveDataSlotCreate(1,&param,NULL));
    assert(sceAppUtilSaveDataSlotCreate(1,&param,NULL)==(int)SCE_APPUTIL_ERROR_SAVEDATA_SLOT_EXISTS);
    assert(!sceAppUtilShutdown());assert(!sceAppUtilInit(&init,&boot));
    assert(!sceAppUtilSaveDataSlotGetParam(1,&output,NULL) && output.userParam==42);
    param.userParam=7;assert(!sceAppUtilSaveDataSlotSetParam(1,&param,NULL));
    assert(!sceAppUtilSaveDataSlotGetParam(1,&output,NULL) && output.userParam==7);
    SceAppUtilSaveDataMountPoint mount={0};
    assert(sceAppUtilSaveDataSlotDelete(1,&mount)==(int)SCE_APPUTIL_ERROR_PARAMETER);
    assert(!sceAppUtilSaveDataSlotDelete(1,NULL));
    assert(module_get_func_export(0x48e01d74,0x93f0d89f)==(void *)sceAppUtilSaveDataSlotGetParam);
    char root[1024];memcpy(root,save_root,sizeof(root));SceAppUtil_finish();
    assert(sceAppUtilShutdown()==(int)SCE_APPUTIL_ERROR_NOT_INITIALIZED);
    assert(!rmdir(root));snprintf(root,sizeof(root),"%s/savedata",test_root);assert(!rmdir(root));
    assert(!unlink(path) && !rmdir(sys) && !rmdir(test_root));
    puts("AppUtil slots: missing mount, initialization retry, persistent reopen, SDK errors, NID and cleanup passed.");
}
