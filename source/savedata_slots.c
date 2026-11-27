#include "savedata_slots.h"
#include "vita_sha1.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { RECORD_BYTES=8+SAVEDATA_SLOT_BYTES+20 };
static int slot_path(char path[1024],const char *root,uint32_t id,bool temporary)
{
    if(!root || !*root)return -EINVAL;
    int length=snprintf(path,1024,"%s/.vita2hos-slot-%08lx%s",root,(unsigned long)id,
                        temporary ? ".tmp" : ".bin");
    return length<0 || length>=1024 ? -ENAMETOOLONG : 0;
}
static int backup_path(char path[1024],const char *root,uint32_t id)
{
    int result=slot_path(path,root,id,false);
    if(!result)memcpy(path+strlen(path)-4,".bak",5);
    return result;
}
static int backup_absent(const char *root,uint32_t id)
{
    char path[1024];int result=backup_path(path,root,id);if(result)return result;
    struct stat info;if(!stat(path,&info))return -EBUSY;
    return errno==ENOENT ? 0 : -errno;
}
static void digest(const unsigned char *record,unsigned char output[20])
{
    VitaSha1 hash;vita_sha1_init(&hash);
    vita_sha1_update(&hash,record,8+SAVEDATA_SLOT_BYTES);vita_sha1_finish(&hash,output);
}
int savedata_slot_get(const char *root,uint32_t id,void *data)
{
    if(!data)return -EINVAL;
    char path[1024];int result=slot_path(path,root,id,false);if(result)return result;
    FILE *file=fopen(path,"rb");
    if(!file && errno==ENOENT) {
        result=backup_path(path,root,id);if(result)return result;
        file=fopen(path,"rb");
    }
    if(!file)return -errno;
    unsigned char record[RECORD_BYTES],hash[20];
    size_t count=fread(record,1,sizeof(record),file);
    bool valid=count==sizeof(record) && fgetc(file)==EOF && !ferror(file);
    if(fclose(file))valid=false;
    if(!valid || memcmp(record,"V2HSS01",8))return -EIO;
    digest(record,hash);
    if(memcmp(hash,record+8+SAVEDATA_SLOT_BYTES,20))return -EIO;
    memcpy(data,record+8,SAVEDATA_SLOT_BYTES);return 0;
}
int savedata_slot_put(const char *root,uint32_t id,const void *data,bool create)
{
    if(!data)return -EINVAL;
    char path[1024],temporary[1024];int result=slot_path(path,root,id,false);
    if(result)return result;
    result=slot_path(temporary,root,id,true);if(result)return result;
    result=backup_absent(root,id);if(result)return result;
    struct stat info;
    if(!stat(path,&info)) {if(create)return -EEXIST;}
    else {if(errno!=ENOENT)return -errno;if(!create)return -ENOENT;}
    unsigned char record[RECORD_BYTES];memcpy(record,"V2HSS01",8);
    memcpy(record+8,data,SAVEDATA_SLOT_BYTES);digest(record,record+8+SAVEDATA_SLOT_BYTES);
    int fd=open(temporary,O_WRONLY|O_CREAT|O_EXCL,0600);
    if(fd<0)return errno==EEXIST ? -EBUSY : -errno;
    size_t written=0;
    while(written<sizeof(record)) {
        ssize_t count=write(fd,record+written,sizeof(record)-written);
        if(count<0 && errno==EINTR)continue;
        if(count<=0){result=count<0 ? -errno : -EIO;break;}
        written+=(size_t)count;
    }
    if(!result && fsync(fd))result=-errno;
    if(close(fd) && !result)result=-errno;
    if(!result && rename(temporary,path)) {
        result=-errno;
        if(!create && result==-EEXIST) {
            char backup[1024];int prepared=backup_path(backup,root,id);
            if(prepared)result=prepared;
            else if(rename(path,backup))result=-errno;
            else if(!rename(temporary,path)) {
                result=unlink(backup) ? -errno : 0;
            } else {
                result=-errno;
                /* Never delete the old record to make room. Failed rollback
                 * leaves .bak readable and blocks further mutation. */
                if(rename(backup,path))result=-EIO;
            }
        }
    }
    if(result)unlink(temporary); /* Only the temporary file this call created. */
    return result;
}
int savedata_slot_delete(const char *root,uint32_t id)
{
    char path[1024];int result=slot_path(path,root,id,false);
    if(result)return result;
    result=backup_absent(root,id);if(result)return result;
    return unlink(path) ? -errno : 0;
}
