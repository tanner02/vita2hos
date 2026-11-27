#define _GNU_SOURCE
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
static int fail_write,fail_sync,fail_rename;
static ssize_t checked_write(int fd,const void *data,size_t size);
static int checked_sync(int fd);
static int checked_rename(const char *from,const char *to);
#define write checked_write
#define fsync checked_sync
#define rename checked_rename
#include "../../source/savedata_slots.c"
#undef write
#undef fsync
#undef rename
static ssize_t checked_write(int fd,const void *data,size_t size)
{if(fail_write){errno=ENOSPC;return -1;}return write(fd,data,size);}
static int checked_sync(int fd)
{if(fail_sync){errno=EIO;return -1;}return fsync(fd);}
static int checked_rename(const char *from,const char *to)
{if(fail_rename){errno=EACCES;return -1;}return rename(from,to);}
int main(void)
{
    char root[]="/tmp/vita2hos-save-XXXXXX";assert(mkdtemp(root));
    unsigned char value[SAVEDATA_SLOT_BYTES],output[SAVEDATA_SLOT_BYTES];
    memset(value,0x58,sizeof(value));memset(output,0x73,sizeof(output));
    assert(savedata_slot_get(root,4,output)==-ENOENT && output[0]==0x73);
    assert(savedata_slot_put(root,4,value,false)==-ENOENT);
    assert(!savedata_slot_put(root,4,value,true));
    assert(savedata_slot_put(root,4,value,true)==-EEXIST);
    assert(!savedata_slot_get(root,4,output) && !memcmp(value,output,sizeof(value)));
    memset(value,0x92,sizeof(value));assert(!savedata_slot_put(root,4,value,false));
    assert(!savedata_slot_get(root,4,output) && !memcmp(value,output,sizeof(value)));
    for(unsigned failure=0;failure<3;failure++) {
        fail_write=failure==0;fail_sync=failure==1;fail_rename=failure==2;
        int expected=failure==0 ? -ENOSPC : failure==1 ? -EIO : -EACCES;
        memset(value,0x33,sizeof(value));assert(savedata_slot_put(root,4,value,false)==expected);
        fail_write=fail_sync=fail_rename=0;
        assert(!savedata_slot_get(root,4,output) && output[0]==0x92);
        char temporary[1024];assert(!slot_path(temporary,root,4,true));
        assert(access(temporary,F_OK)<0 && errno==ENOENT);
    }
    char path[1024];assert(!slot_path(path,root,4,true));
    FILE *file=fopen(path,"wb");assert(file && !fclose(file));
    assert(savedata_slot_put(root,4,output,false)==-EEXIST);
    assert(!savedata_slot_get(root,4,output) && output[0]==0x92);assert(!unlink(path));
    assert(!slot_path(path,root,4,false));file=fopen(path,"r+b");assert(file);
    assert(!fseek(file,20,SEEK_SET) && fputc(0,file)!=EOF && !fclose(file));
    memset(output,0x73,sizeof(output));assert(savedata_slot_get(root,4,output)==-EIO);
    for(unsigned i=0;i<sizeof(output);i++)assert(output[i]==0x73);
    assert(!savedata_slot_put(root,UINT32_MAX,value,true));
    assert(!savedata_slot_delete(root,UINT32_MAX));assert(!savedata_slot_delete(root,4));
    assert(savedata_slot_delete(root,4)==-ENOENT);assert(!rmdir(root));
    puts("Savedata slot backend: persistent reopen, create/update, stale staging, corruption preservation and full-width IDs passed.");
}
