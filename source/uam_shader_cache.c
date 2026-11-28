#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "uam_shader_cache.h"
#include "uam_dksh.h"
#include "vita_sha1.h"

#ifndef UAM_CACHE_MAX_BYTES
#define UAM_CACHE_MAX_BYTES (128U*1024U*1024U)
#endif
#ifndef UAM_CACHE_MAX_ENTRIES
#define UAM_CACHE_MAX_ENTRIES 16384U
#endif
#define SOURCE_LIMIT (2U*1024U*1024U)
#define BINARY_LIMIT (8U*1024U*1024U)
#define HEADER_SIZE 80U
#define PATH_SIZE 768U

static struct { char directory[PATH_SIZE];uint64_t bytes;unsigned entries;bool ready; } budget;
static void digest(const void *data,size_t size,unsigned char out[20])
{ VitaSha1 sha;vita_sha1_init(&sha);vita_sha1_update(&sha,data,size);vita_sha1_finish(&sha,out); }
static void put32(unsigned char *p,uint32_t n)
{ for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(n>>(i*8)); }
static bool cache_key(const char *directory,const char *revision,uint32_t stage,const char *source,
    char path[PATH_SIZE],size_t *length,unsigned char rev[20],unsigned char src[20])
{
    if(!directory||!revision||!source)return false;
    *length=strnlen(source,SOURCE_LIMIT+1U);
    size_t rev_length=strnlen(revision,129);
    if(*length>SOURCE_LIMIT||!rev_length||rev_length>128)return false;
    digest(revision,rev_length,rev);digest(source,*length,src);
    char hex[41];for(unsigned i=0;i<20;i++)snprintf(hex+i*2,3,"%02x",src[i]);
    int n=snprintf(path,PATH_SIZE,"%s/%08x-%s.vsc",directory,stage,hex);
    return n>0&&(unsigned)n<PATH_SIZE;
}

bool uam_cache_load(const char *directory,const char *revision,uint32_t stage,
    const char *source,uint32_t capacity,void **output,uint32_t *size)
{
    if(output)*output=NULL;
    if(size)*size=0;
    if(!output||!size||!capacity)return false;
    char path[PATH_SIZE];size_t length;unsigned char rev[20],src[20],header[HEADER_SIZE];
    if(!cache_key(directory,revision,stage,source,path,&length,rev,src))return false;
    FILE *file=fopen(path,"rb");if(!file)return false;
    void *data=NULL;bool ok=false;uint32_t bytes=0;
    if(fread(header,1,sizeof(header),file)!=sizeof(header)||memcmp(header,"V2SHDC01",8)||
        uam_read_u32(header+8)!=stage||uam_read_u32(header+12)!=length||
        memcmp(header+20,rev,20)||memcmp(header+40,src,20))goto done;
    bytes=uam_read_u32(header+16);
    if(bytes<24||bytes>BINARY_LIMIT||bytes>capacity)goto done;
    struct stat info;
    if(fstat(fileno(file),&info)||!S_ISREG(info.st_mode)||info.st_size<0||
        (uint64_t)info.st_size!=HEADER_SIZE+(uint64_t)length+bytes)goto done;
    unsigned char chunk[4096];
    for(size_t offset=0;offset<length;) {
        size_t n=length-offset;if(n>sizeof(chunk))n=sizeof(chunk);
        if(fread(chunk,1,n,file)!=n||memcmp(chunk,source+offset,n))goto done;
        offset+=n;
    }
    data=malloc(bytes);if(!data)goto done;
    unsigned char actual[20];
    if(fread(data,1,bytes,file)!=bytes||!uam_dksh_valid(data,bytes))goto done;
    digest(data,bytes,actual);if(memcmp(actual,header+60,20))goto done;
    ok=true;
done:
    if(fclose(file))ok=false;
    if(ok){*output=data;*size=bytes;}else free(data);
    return ok;
}

static bool owned_name(const char *name)
{
    if(strlen(name)!=53||name[8]!='-'||strcmp(name+49,".vsc"))return false;
    for(unsigned i=0;i<49;i++) {
        if(i==8)continue;
        if(!((name[i]>='0'&&name[i]<='9')||(name[i]>='a'&&name[i]<='f')))return false;
    }
    return true;
}
static bool owned_temporary(const char *name)
{
    if(strlen(name)!=64||memcmp(name+53,".tmp-",5))return false;
    char prefix[54];memcpy(prefix,name,53);prefix[53]=0;
    if(!owned_name(prefix))return false;
    return true;
}
static bool scan_budget(const char *directory)
{
    if(budget.ready&&!strcmp(budget.directory,directory))return true;
    budget.ready=false;budget.bytes=0;budget.entries=0;
    if(strlen(directory)>=sizeof(budget.directory))return false;
    DIR *dir=opendir(directory);if(!dir)return false;
    struct dirent *entry;
    int read_error=0;
    for(;;) {
        errno=0;entry=readdir(dir);
        if(!entry){read_error=errno;break;}
        /* Calls are serialized and this scan precedes creation of our temp
         * file, so these are abandoned writes from an interrupted process. */
        if(!owned_name(entry->d_name)&&!owned_temporary(entry->d_name))continue;
        char path[PATH_SIZE];int n=snprintf(path,sizeof(path),"%s/%s",directory,entry->d_name);
        struct stat info;
        if(n<=0||(unsigned)n>=sizeof(path))continue;
        if(owned_temporary(entry->d_name)&&!unlink(path))continue;
        if(stat(path,&info)) {
            if(errno==ENOENT)continue;
            closedir(dir);return false;
        }
        if(!S_ISREG(info.st_mode)||info.st_size<0)continue;
        if(budget.bytes>UINT64_MAX-(uint64_t)info.st_size){closedir(dir);return false;}
        budget.bytes+=(uint64_t)info.st_size;
        if(budget.entries==UINT32_MAX){closedir(dir);return false;}
        budget.entries++;
    }
    if(closedir(dir)||read_error)return false;
    strcpy(budget.directory,directory);budget.ready=true;return true;
}

void uam_cache_store(const char *directory,const char *revision,uint32_t stage,
    const char *source,const void *data,uint32_t size)
{
    if(size>BINARY_LIMIT||!uam_dksh_valid(data,size))return;
    char path[PATH_SIZE];size_t length;unsigned char rev[20],src[20],header[HEADER_SIZE]={0};
    if(!cache_key(directory,revision,stage,source,path,&length,rev,src))return;
    if(mkdir(directory,0700)&&errno!=EEXIST)return;
    if(!scan_budget(directory))return;
    struct stat previous;bool exists=!stat(path,&previous)&&S_ISREG(previous.st_mode)&&previous.st_size>=0;
    uint64_t old_size=exists?(uint64_t)previous.st_size:0;
    uint64_t retained=budget.bytes>=old_size?budget.bytes-old_size:budget.bytes;
    uint64_t entry_size=HEADER_SIZE+(uint64_t)length+size;
    if(retained>UAM_CACHE_MAX_BYTES||entry_size>UAM_CACHE_MAX_BYTES-retained||
        (!exists&&budget.entries>=UAM_CACHE_MAX_ENTRIES))return;
    char temporary[PATH_SIZE+16];
    int n=snprintf(temporary,sizeof(temporary),"%s.tmp-XXXXXX",path);
    if(n<=0||(unsigned)n>=sizeof(temporary))return;
    int fd=mkstemp(temporary);if(fd<0)return;
    FILE *file=fdopen(fd,"wb");if(!file){close(fd);unlink(temporary);return;}
    memcpy(header,"V2SHDC01",8);put32(header+8,stage);put32(header+12,(uint32_t)length);put32(header+16,size);
    memcpy(header+20,rev,20);memcpy(header+40,src,20);digest(data,size,header+60);
    bool ok=fwrite(header,1,sizeof(header),file)==sizeof(header)&&
        fwrite(source,1,length,file)==length&&fwrite(data,1,size,file)==size;
    if(fclose(file))ok=false;
    if(ok&&rename(temporary,path)) {
        /* Some SD filesystem backends cannot replace an existing name. */
        if(errno==EEXIST&&exists&&!unlink(path)) {
            budget.bytes=retained;if(budget.entries)budget.entries--;exists=false;
            ok=rename(temporary,path)==0;
        } else ok=false;
    }
    if(ok){budget.bytes=retained+entry_size;if(!exists)budget.entries++;}
    else unlink(temporary);
}
