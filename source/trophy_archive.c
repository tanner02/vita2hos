#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include "trophy_archive.h"
static uint32_t read_be32(const uint8_t *p)
{return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static uint64_t read_be64(const uint8_t *p)
{return (uint64_t)read_be32(p)<<32|read_be32(p+4);}
static bool valid_name(const uint8_t *name)
{
    size_t length=0;
    while(length<32 && name[length]) {
        uint8_t c=name[length++];
        if(c<32 || c>=127 || c=='/' || c=='\\' || c==':')return false;
    }
    return length>0 && length<32 &&
        !(length==1 && name[0]=='.') && !(length==2 && name[0]=='.' && name[1]=='.');
}
int trophy_archive_open(const void *input,size_t size,TrophyArchive *output)
{
    if(!input || !output || output->data || output->entries || output->size || output->count ||
       size<64 || size>TROPHY_ARCHIVE_MAX_BYTES)return TROPHY_ARCHIVE_INVALID;
    const uint8_t *data=input;
    if(read_be32(data)!=0xdca24d00 || read_be64(data+8)!=size)return TROPHY_ARCHIVE_INVALID;
    uint32_t version=read_be32(data+4),count=read_be32(data+16);
    if((version!=2 && version!=3) || read_be32(data+20)!=64)return TROPHY_ARCHIVE_UNSUPPORTED;
    if(!count || count>TROPHY_ARCHIVE_MAX_ENTRIES || count>(size-64)/64)return TROPHY_ARCHIVE_INVALID;
    uint8_t digest[20],zero[20]={0};Sha1Context hash;
    sha1ContextCreate(&hash);sha1ContextUpdate(&hash,data,28);
    sha1ContextUpdate(&hash,zero,sizeof(zero));sha1ContextUpdate(&hash,data+48,size-48);
    sha1ContextGetHash(&hash,digest);
    if(memcmp(digest,data+28,sizeof(digest)))return TROPHY_ARCHIVE_CHECKSUM;
    TrophyArchiveEntry *entries=calloc(count,sizeof(*entries));
    if(!entries)return TROPHY_ARCHIVE_NO_MEMORY;
    uint64_t table_end=64+(uint64_t)count*64;
    for(uint32_t i=0;i<count;i++) {
        const uint8_t *record=data+64+(size_t)i*64;
        if(!valid_name(record))goto invalid;
        memcpy(entries[i].name,record,32);
        entries[i].offset=read_be64(record+32);entries[i].size=read_be64(record+40);
        if(entries[i].offset<table_end || entries[i].offset>size ||
           entries[i].size>size-entries[i].offset)goto invalid;
        for(uint32_t j=0;j<i;j++)if(!strcmp(entries[i].name,entries[j].name))goto invalid;
    }
    *output=(TrophyArchive){.data=data,.size=size,.count=count,.entries=entries};
    return TROPHY_ARCHIVE_OK;
invalid:
    free(entries);return TROPHY_ARCHIVE_INVALID;
}
void trophy_archive_close(TrophyArchive *archive)
{if(archive){free(archive->entries);*archive=(TrophyArchive){0};}}
const TrophyArchiveEntry *trophy_archive_find(const TrophyArchive *archive,const char *name)
{
    if(!archive || !archive->data || !archive->entries || !name || strnlen(name,32)>=32)return NULL;
    for(size_t i=0;i<archive->count;i++)if(!strcmp(name,archive->entries[i].name))return &archive->entries[i];
    return NULL;
}
int trophy_archive_read(const TrophyArchive *archive,const char *name,uint64_t offset,void *output,size_t size)
{
    const TrophyArchiveEntry *entry=trophy_archive_find(archive,name);
    if(!entry)return TROPHY_ARCHIVE_NOT_FOUND;
    if((size && !output) || offset>entry->size || size>entry->size-offset)return TROPHY_ARCHIVE_RANGE;
    if(entry->offset>archive->size || offset>archive->size-entry->offset ||
       size>archive->size-entry->offset-offset)return TROPHY_ARCHIVE_RANGE;
    if(size)memmove(output,archive->data+(size_t)(entry->offset+offset),size);
    return TROPHY_ARCHIVE_OK;
}
