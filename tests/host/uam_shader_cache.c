#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdbool.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

static unsigned write_calls,fail_write;
static bool fail_allocation,fail_rename,replace_fallback;
static void *test_malloc(size_t n){return fail_allocation?NULL:malloc(n);}
static size_t test_fwrite(const void *p,size_t size,size_t count,FILE *file)
{ return ++write_calls==fail_write?0:fwrite(p,size,count,file); }
static int test_rename(const char *a,const char *b)
{
    if(fail_rename){errno=EIO;return -1;}
    if(replace_fallback){replace_fallback=false;errno=EEXIST;return -1;}
    return rename(a,b);
}
#define UAM_CACHE_MAX_BYTES 512U
#define UAM_CACHE_MAX_ENTRIES 2U
#define malloc test_malloc
#define fwrite test_fwrite
#define rename test_rename
#include "../../source/uam_shader_cache.c"
#undef malloc
#undef fwrite
#undef rename

static void write_file(const char *path,const void *data,size_t size)
{ FILE *f=fopen(path,"wb");assert(f);assert(fwrite(data,1,size,f)==size);assert(!fclose(f)); }
static unsigned entries(const char *directory,char path[768])
{
    DIR *dir=opendir(directory);assert(dir);unsigned count=0;struct dirent *item;
    while((item=readdir(dir)))if(owned_name(item->d_name)) {
        if(path)snprintf(path,768,"%s/%s",directory,item->d_name);
        count++;
    }
    assert(!closedir(dir));return count;
}
static void clear_directory(const char *path)
{
    DIR *dir=opendir(path);assert(dir);struct dirent *entry;
    while((entry=readdir(dir)))if(strcmp(entry->d_name,".")&&strcmp(entry->d_name,"..")) {
        char file[768];snprintf(file,sizeof(file),"%s/%s",path,entry->d_name);
        assert(!unlink(file)||!rmdir(file));
    }
    assert(!closedir(dir));assert(!rmdir(path));
}
static void miss(const char *directory,const char *revision,unsigned stage,const char *source,unsigned capacity)
{
    void *data=(void *)1;uint32_t size=99;
    assert(!uam_cache_load(directory,revision,stage,source,capacity,&data,&size));
    assert(!data&&!size);
}
int main(void)
{
    uint32_t shader[8]={0x48534b44,24,24,8,0,0,0x38383838,0x38383838};
    char directory[]="/tmp/vita2hos-cache-data-XXXXXX";assert(mkdtemp(directory));
    miss(directory,"backend-A",0,"valid",32);
    uam_cache_store(directory,"backend-A",0,"valid",shader,sizeof(shader));
    char path[768];assert(entries(directory,path)==1);
    void *data=NULL;uint32_t size=0;
    assert(uam_cache_load(directory,"backend-A",0,"valid",32,&data,&size));
    assert(size==32&&!memcmp(data,shader,32));free(data);
    miss(directory,"backend-A",0,"valid",31);
    miss(directory,"backend-B",0,"valid",32);
    miss(directory,"backend-A",4,"valid",32);
    miss(directory,"backend-A",0,"other",32);
    fail_allocation=true;miss(directory,"backend-A",0,"valid",32);fail_allocation=false;

    unsigned char original[117];FILE *file=fopen(path,"rb");assert(file);
    assert(fread(original,1,sizeof(original),file)==sizeof(original));assert(fgetc(file)==EOF);assert(!fclose(file));
    for(unsigned i=0;i<sizeof(original);i++) {
        unsigned char changed[sizeof(original)];memcpy(changed,original,sizeof(changed));changed[i]^=0x80;
        write_file(path,changed,sizeof(changed));miss(directory,"backend-A",0,"valid",32);
    }
    for(unsigned n=0;n<sizeof(original);n++) {
        write_file(path,original,n);miss(directory,"backend-A",0,"valid",32);
    }
    unsigned char trailing[118];memcpy(trailing,original,sizeof(original));trailing[117]=0;
    write_file(path,trailing,sizeof(trailing));miss(directory,"backend-A",0,"valid",32);
    write_file(path,original,sizeof(original));
    unsigned char bad_dksh[117];memcpy(bad_dksh,original,sizeof(bad_dksh));
    memset(bad_dksh+80+5+4,0,4);
    digest(bad_dksh+85,32,bad_dksh+60);
    write_file(path,bad_dksh,sizeof(bad_dksh));miss(directory,"backend-A",0,"valid",32);
    write_file(path,original,sizeof(original));
    for(fail_write=1;fail_write<=3;fail_write++) {
        write_calls=0;uam_cache_store(directory,"backend-B",0,"valid",shader,32);
        assert(uam_cache_load(directory,"backend-A",0,"valid",32,&data,&size));free(data);
    }
    fail_write=0;fail_rename=true;
    uam_cache_store(directory,"backend-B",0,"valid",shader,32);
    assert(uam_cache_load(directory,"backend-A",0,"valid",32,&data,&size));free(data);
    fail_rename=false;replace_fallback=true;
    uam_cache_store(directory,"backend-B",0,"valid",shader,32);
    miss(directory,"backend-A",0,"valid",32);
    assert(uam_cache_load(directory,"backend-B",0,"valid",32,&data,&size));free(data);
    uam_cache_store(directory,"backend-B",4,"valid",shader,32);assert(entries(directory,NULL)==2);
    uam_cache_store(directory,"backend-B",5,"valid",shader,32);assert(entries(directory,NULL)==2);
    miss(directory,"backend-B",5,"valid",32);

    char bytes_dir[]="/tmp/vita2hos-cache-budget-XXXXXX";assert(mkdtemp(bytes_dir));
    char large[401];memset(large,'x',400);large[400]=0;
    uam_cache_store(bytes_dir,"backend-A",0,large,shader,32);assert(entries(bytes_dir,NULL)==1);
    uam_cache_store(bytes_dir,"backend-A",4,large,shader,32);assert(entries(bytes_dir,NULL)==1);
    miss(bytes_dir,"backend-A",4,large,32);

    char temp_dir[]="/tmp/vita2hos-cache-temporary-XXXXXX";assert(mkdtemp(temp_dir));
    const char *basename=strrchr(path,'/');assert(basename);
    char abandoned[768];snprintf(abandoned,sizeof(abandoned),"%s/%s.tmp-ABC123",temp_dir,basename+1);
    write_file(abandoned,trailing,sizeof(trailing));
    char foreign[768];snprintf(foreign,sizeof(foreign),"%s/notes.txt",temp_dir);write_file(foreign,"keep",4);
    uam_cache_store(temp_dir,"backend-A",0,"valid",shader,32);
    assert(access(abandoned,F_OK)&&!access(foreign,F_OK)&&entries(temp_dir,NULL)==1);
    char *oversized=malloc(SOURCE_LIMIT+2);assert(oversized);memset(oversized,'x',SOURCE_LIMIT+1);oversized[SOURCE_LIMIT+1]=0;
    uam_cache_store(temp_dir,"backend-A",1,oversized,shader,32);free(oversized);
    uam_cache_store(temp_dir,"backend-A",1,"valid",shader,BINARY_LIMIT+1);
    assert(entries(temp_dir,NULL)==1);
    miss("/nonexistent/vita2hos-cache","backend-A",0,"valid",32);
    uam_cache_store(foreign,"backend-A",0,"valid",shader,32);
    assert(!access(foreign,F_OK));
    clear_directory(directory);clear_directory(bytes_dir);clear_directory(temp_dir);
    puts("Shader cache: exact source/stage/revision, all-byte corruption, truncation, capacity, atomic failures, budgets and abandoned-temp cleanup passed.");
}
