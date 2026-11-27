#include <psp2/apputil.h>
#include <psp2/kernel/error.h>
#include <switch.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "config.h"
#include "module.h"
#include "savedata_identity.h"
#include "savedata_slots.h"
#include "modules/SceAppUtil.h"

_Static_assert(sizeof(SceAppUtilSaveDataSlotParam)==SAVEDATA_SLOT_BYTES,"slot ABI");
static Mutex app_lock;
static bool app_ready;
static char save_root[1024];
static int app_error(int result)
{
    switch(-result) {
    case 0:return 0;
    case ENOENT:return (int)SCE_APPUTIL_ERROR_SAVEDATA_SLOT_NOT_FOUND;
    case EEXIST:return (int)SCE_APPUTIL_ERROR_SAVEDATA_SLOT_EXISTS;
    case ENOSPC:return (int)SCE_APPUTIL_ERROR_SAVEDATA_NO_SPACE_FS;
    case EACCES:case EPERM:return (int)SCE_APPUTIL_ERROR_NO_PERMISSION;
    case ENOMEM:return (int)SCE_APPUTIL_ERROR_NO_MEMORY;
    case EINVAL:case ENAMETOOLONG:return (int)SCE_APPUTIL_ERROR_PARAMETER;
    default:return (int)0x80010005 /* Vita errno EIO, as used by file I/O. */;
    }
}
static int ensure_directory(const char *path)
{
    if(!mkdir(path,0700))return 0;
    if(errno!=EEXIST)return -errno;
    struct stat info;
    if(stat(path,&info))return -errno;
    return S_ISDIR(info.st_mode) ? 0 : -ENOTDIR;
}
static int prepare_storage(void)
{
    char path[1024];
    int length=snprintf(path,sizeof(path),"%s/sce_sys/param.sfo",VITA2HOS_APP0_PATH);
    if(length<0 || (size_t)length>=sizeof(path))return -ENAMETOOLONG;
    FILE *file=fopen(path,"rb");if(!file)return -errno;
    int result=0;unsigned char *bytes=NULL;SavedataIdentity identity;
    struct stat info;
    if(fstat(fileno(file),&info))result=-errno;
    else if(info.st_size<20 || info.st_size>1024*1024)result=-EINVAL;
    else {
        bytes=malloc((size_t)info.st_size);
        if(!bytes)result=-ENOMEM;
        else if(fread(bytes,1,(size_t)info.st_size,file)!=(size_t)info.st_size || ferror(file))result=-EIO;
        else result=savedata_identity_parse(bytes,(size_t)info.st_size,&identity);
    }
    free(bytes);if(fclose(file) && !result)result=-EIO;
    if(result)return result;
    length=snprintf(path,sizeof(path),"%s/savedata",VITA2HOS_ROOT_PATH);
    if(length<0 || (size_t)length>=sizeof(path))return -ENAMETOOLONG;
    result=ensure_directory(path);if(result)return result;
    length=snprintf(save_root,sizeof(save_root),"%s/%s",path,identity.directory);
    if(length<0 || (size_t)length>=sizeof(save_root))return -ENAMETOOLONG;
    return ensure_directory(save_root);
}
void SceAppUtil_finish(void)
{mutexLock(&app_lock);app_ready=false;save_root[0]=0;mutexUnlock(&app_lock);}
EXPORT(SceAppUtil,0xDAFFE671,int,sceAppUtilInit,SceAppUtilInitParam *init,SceAppUtilBootParam *boot)
{
    if(!init || !boot)return (int)SCE_APPUTIL_ERROR_PARAMETER;
    mutexLock(&app_lock);
    int result=app_ready ? (int)SCE_APPUTIL_ERROR_BUSY : 0;
    if(!result) {
        int prepared=prepare_storage();
        result=prepared==-ENOENT || prepared==-ENOTDIR ? (int)SCE_APPUTIL_ERROR_NOT_MOUNTED : app_error(prepared);
        if(!result)app_ready=true;
        else save_root[0]=0;
    }
    mutexUnlock(&app_lock);return result;
}
EXPORT(SceAppUtil,0xB220B00B,int,sceAppUtilShutdown,void)
{
    mutexLock(&app_lock);int result=app_ready ? 0 : (int)SCE_APPUTIL_ERROR_NOT_INITIALIZED;
    app_ready=false;save_root[0]=0;mutexUnlock(&app_lock);return result;
}
static int slot_operation(unsigned id,SceAppUtilSaveDataSlotParam *param,
                          SceAppUtilSaveDataMountPoint *mount,unsigned operation)
{
    mutexLock(&app_lock);int result=0;
    if(!app_ready)result=(int)SCE_APPUTIL_ERROR_NOT_INITIALIZED;
    else if(mount || (operation!=3 && !param))result=(int)SCE_APPUTIL_ERROR_PARAMETER;
    else {
        int native=operation==0 ? savedata_slot_get(save_root,id,param) :
            operation==3 ? savedata_slot_delete(save_root,id) :
            savedata_slot_put(save_root,id,param,operation==1);
        result=app_error(native);
    }
    mutexUnlock(&app_lock);return result;
}
EXPORT(SceAppUtil,0x93F0D89F,int,sceAppUtilSaveDataSlotGetParam,unsigned id,SceAppUtilSaveDataSlotParam *param,SceAppUtilSaveDataMountPoint *mount)
{return slot_operation(id,param,mount,0);}
EXPORT(SceAppUtil,0x7E8FE96A,int,sceAppUtilSaveDataSlotCreate,unsigned id,SceAppUtilSaveDataSlotParam *param,SceAppUtilSaveDataMountPoint *mount)
{return slot_operation(id,param,mount,1);}
EXPORT(SceAppUtil,0x98630136,int,sceAppUtilSaveDataSlotSetParam,unsigned id,SceAppUtilSaveDataSlotParam *param,SceAppUtilSaveDataMountPoint *mount)
{return slot_operation(id,param,mount,2);}
EXPORT(SceAppUtil,0x266A7646,int,sceAppUtilSaveDataSlotDelete,unsigned id,SceAppUtilSaveDataMountPoint *mount)
{return slot_operation(id,NULL,mount,3);}
DECLARE_LIBRARY(SceAppUtil,0x48E01D74);
