#include <psp2/apputil.h>
#include <psp2/kernel/error.h>
#include <switch.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
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
    case EBUSY:return (int)SCE_APPUTIL_ERROR_BUSY;
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
        /* A successfully loaded slot is available, matching the pinned reference. */
        if(!native && operation==0)param->status=0;
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

static int savedata_path(const char *input,char path[1024])
{
    if(!input)return -EINVAL;
    if(!strncmp(input,"savedata0:/",11))input+=11;
    if(!*input || *input=='/')return -EINVAL;
    size_t used=strlen(save_root);memcpy(path,save_root,used);
    while(*input) {
        const char *start=input;
        while(*input && *input!='/') {
            unsigned char c=(unsigned char)*input++;
            if(c<32 || c==127 || c==':' || c=='\\')return -EINVAL;
        }
        size_t length=(size_t)(input-start);
        if(!length || (length==1 && start[0]=='.') ||
           (length==2 && !memcmp(start,"..",2)) ||
           start[length-1]==' ' || start[length-1]=='.' ||
           (length>=10 && !strncasecmp(start,".vita2hos-",10)))return -EINVAL;
        if(used+1+length>=1024)return -ENAMETOOLONG;
        path[used++]='/';memcpy(path+used,start,length);used+=length;path[used]=0;
        struct stat info;
        if(!lstat(path,&info)) {
            if(S_ISLNK(info.st_mode))return -EACCES;
            if(*input && !S_ISDIR(info.st_mode))return -ENOTDIR;
        } else if(errno!=ENOENT)return -errno;
        if(*input && !*++input)return -EINVAL;
    }
    return 0;
}
static int validate_save_file(const SceAppUtilSaveDataFile *file,char path[1024])
{
    if(file->mode>2 || file->offset<0 || (uint64_t)file->offset>INT64_MAX-(uint64_t)file->bufSize ||
       (file->mode==0 && file->bufSize && !file->buf) ||
       (file->mode==2 && (file->bufSize || file->offset)))return -EINVAL;
    return savedata_path(file->filePath,path);
}
static int write_save_file(const SceAppUtilSaveDataFile *file)
{
    char path[1024];int result=validate_save_file(file,path);if(result)return result;
    if(file->mode==2)return ensure_directory(path);
    int fd=open(path,O_WRONLY|O_CREAT,0600);if(fd<0)return -errno;
    if(lseek(fd,(off_t)file->offset,SEEK_SET)<0)result=-errno;
    size_t done=0;
    while(!result && file->buf && done<file->bufSize) {
        ssize_t count=write(fd,(const unsigned char *)file->buf+done,file->bufSize-done);
        if(count<0 && errno==EINTR)continue;
        if(count<=0)result=count<0 ? -errno : -EIO;
        else done+=(size_t)count;
    }
    if(!result && file->mode==1 && ftruncate(fd,(off_t)(file->offset+file->bufSize)))result=-errno;
    if(!result && fsync(fd))result=-errno;
    if(close(fd) && !result)result=-errno;
    return result;
}
EXPORT(SceAppUtil,0x607647BA,int,sceAppUtilSaveDataDataSave,SceAppUtilSaveDataFileSlot *slot,
       SceAppUtilSaveDataFile *files,unsigned file_count,SceAppUtilSaveDataMountPoint *mount,
       SceSize *required_kb)
{
    mutexLock(&app_lock);int result=0;
    if(!app_ready)result=(int)SCE_APPUTIL_ERROR_NOT_INITIALIZED;
    else if(mount || (file_count && !files) || file_count>1024)result=(int)SCE_APPUTIL_ERROR_PARAMETER;
    else {
        char path[1024];int native=0;
        /* Validate the entire batch before creating anything. Execution is
         * sequential, not a cross-file transaction; errors are never success. */
        for(unsigned i=0;i<file_count && !native;i++)native=validate_save_file(&files[i],path);
        for(unsigned i=0;i<file_count && !native;i++)native=write_save_file(&files[i]);
        if(!native && slot && slot->slotParam) {
            native=savedata_slot_put(save_root,slot->id,slot->slotParam,false);
            if(native==-ENOENT)native=savedata_slot_put(save_root,slot->id,slot->slotParam,true);
        }
        result=native==-ENOENT ? (int)0x80010002 :
            native==-ENOTDIR ? (int)0x80010014 : app_error(native);
        if(!result && required_kb)*required_kb=0;
    }
    mutexUnlock(&app_lock);return result;
}
DECLARE_LIBRARY(SceAppUtil,0x48E01D74);
