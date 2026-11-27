#include <stdint.h>
#include <psp2/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "config.h"
#include "log.h"
#include "trophy_archive.h"
#include "trophy_metadata.h"
#include <switch.h>
#include "module.h"
#include "modules/SceNpTrophy.h"
#include "modules/SceSysmem.h"
/* The pinned SDK lacks trophy declarations. These errors/signatures match the
 * pinned public reference. Contexts own verified archive/configuration data;
 * trophy progress, unlocks and online services remain unsupported. */
#define TROPHY_NOT_INITIALIZED ((int)0x80551601)
#define TROPHY_ALREADY_INITIALIZED ((int)0x80551602)
#define TROPHY_NO_MEMORY ((int)0x80551603)
#define TROPHY_INVALID_ARGUMENT ((int)0x80551604)
#define TROPHY_EXCEEDS_MAX ((int)0x80551606)
#define TROPHY_INVALID_HANDLE ((int)0x80551608)
#define TROPHY_HANDLE_LIMIT 64
#define TROPHY_CONTEXT_LIMIT 8
#define TROPHY_CONTEXT_BYTES (128u*1024u*1024u)
#define TROPHY_INVALID_CONTEXT ((int)0x80551609)
#define TROPHY_INVALID_COMM_ID ((int)0x8055160a)
#define TROPHY_INVALID_COMM_SIGN ((int)0x8055160b)
#define TROPHY_BROKEN_DATA ((int)0x80551614)
#define TROPHY_CONTEXT_EXISTS ((int)0x80551616)
#define TROPHY_TRP_VERIFICATION ((int)0x80551617)
#define TROPHY_ICON_NOT_FOUND ((int)0x80551618)
#define TROPHY_TRP_NOT_FOUND ((int)0x80551619)
#define TROPHY_INVALID_TRP ((int)0x8055161a)
#define TROPHY_UNSUPPORTED_TRP ((int)0x8055161b)
#define TROPHY_INVALID_CONF ((int)0x8055161c)
#define TROPHY_INSUFFICIENT_BUFFER ((int)0x80551605)
#define TROPHY_ABORT ((int)0x80551607)
typedef struct {char data[9],term;uint8_t num;char reserved;} TrophyCommunicationId;
_Static_assert(sizeof(TrophyCommunicationId)==12,"Trophy communication ID ABI");
typedef struct {
    int32_t uid;
    TrophyCommunicationId communication;
    uint8_t *bytes;
    TrophyArchive archive;
    TrophyMetadata metadata;
} TrophyContext;
static TrophyContext *trophy_contexts;
static size_t context_bytes;
static void clear_context(TrophyContext *context)
{
    context_bytes-=context->archive.size;
    trophy_archive_close(&context->archive);free(context->bytes);*context=(TrophyContext){0};
}
static void clear_library(void)
{
    if(trophy_contexts)for(unsigned i=0;i<TROPHY_CONTEXT_LIMIT;i++)if(trophy_contexts[i].uid)clear_context(&trophy_contexts[i]);
    free(trophy_contexts);trophy_contexts=NULL;context_bytes=0;
}
static TrophyContext *find_context(int32_t uid)
{
    if(trophy_contexts && uid>0)for(unsigned i=0;i<TROPHY_CONTEXT_LIMIT;i++)if(trophy_contexts[i].uid==uid)return &trophy_contexts[i];
    return NULL;
}

typedef struct {int32_t uid;bool aborted;} TrophyHandle;
static TrophyHandle *trophy_handles;
static Mutex trophy_lock;
static TrophyHandle *find_handle(int32_t uid)
{
    if(!trophy_handles || uid<=0)return NULL;
    for(unsigned i=0;i<TROPHY_HANDLE_LIMIT;i++)if(trophy_handles[i].uid==uid)return &trophy_handles[i];
    return NULL;
}
bool SceNpTrophy_is_initialized(void)
{
    mutexLock(&trophy_lock);bool ready=trophy_handles!=NULL;mutexUnlock(&trophy_lock);return ready;
}
void SceNpTrophy_finish(void)
{
    mutexLock(&trophy_lock);clear_library();free(trophy_handles);trophy_handles=NULL;mutexUnlock(&trophy_lock);
}
EXPORT(SceNpTrophy,0x34516838,int,sceNpTrophyInit,const void *options)
{
    mutexLock(&trophy_lock);
    int result=0;
    if(trophy_handles)result=TROPHY_ALREADY_INITIALIZED;
    else if(options)result=TROPHY_INVALID_ARGUMENT;
    else {
        TrophyHandle *handles=calloc(TROPHY_HANDLE_LIMIT,sizeof(*handles));
        TrophyContext *contexts=calloc(TROPHY_CONTEXT_LIMIT,sizeof(*contexts));
        if(!handles || !contexts){free(handles);free(contexts);result=TROPHY_NO_MEMORY;}
        else {trophy_handles=handles;trophy_contexts=contexts;}
    }
    mutexUnlock(&trophy_lock);
    return result;
}
EXPORT(SceNpTrophy,0xBFE0F28F,int,sceNpTrophyTerm,void)
{
    mutexLock(&trophy_lock);
    int result=trophy_handles ? 0 : TROPHY_NOT_INITIALIZED;
    clear_library();free(trophy_handles);trophy_handles=NULL;
    mutexUnlock(&trophy_lock);
    return result;
}
EXPORT(SceNpTrophy,0x4EBC6977,int,sceNpTrophyCreateHandle,int32_t *output)
{
    mutexLock(&trophy_lock);
    int result=TROPHY_EXCEEDS_MAX;
    if(!trophy_handles)result=TROPHY_NOT_INITIALIZED;
    else if(!output)result=TROPHY_INVALID_ARGUMENT;
    else for(unsigned i=0;i<TROPHY_HANDLE_LIMIT;i++)if(!trophy_handles[i].uid) {
        int32_t uid=SceSysmem_get_next_uid();
        if(uid<=0){result=TROPHY_NO_MEMORY;break;}
        trophy_handles[i]=(TrophyHandle){.uid=uid};*output=uid;result=0;break;
    }
    mutexUnlock(&trophy_lock);
    return result;
}
EXPORT(SceNpTrophy,0xFF142071,int,sceNpTrophyDestroyHandle,int32_t uid)
{
    mutexLock(&trophy_lock);
    TrophyHandle *handle=find_handle(uid);
    int result=!trophy_handles ? TROPHY_NOT_INITIALIZED : !handle ? TROPHY_INVALID_HANDLE : 0;
    if(handle)*handle=(TrophyHandle){0};
    mutexUnlock(&trophy_lock);
    return result;
}
EXPORT(SceNpTrophy,0xD55C6F4C,int,sceNpTrophyAbortHandle,int32_t uid)
{
    mutexLock(&trophy_lock);
    TrophyHandle *handle=find_handle(uid);
    int result=!trophy_handles ? TROPHY_NOT_INITIALIZED : !handle ? TROPHY_INVALID_HANDLE : 0;
    if(handle)handle->aborted=true;
    mutexUnlock(&trophy_lock);
    return result;
}
static bool valid_communication(const TrophyCommunicationId *id)
{
    if(!id || id->term || id->num>99 || memcmp(id->data,"NPWR",4))return false;
    for(unsigned i=4;i<9;i++)if(id->data[i]<'0' || id->data[i]>'9')return false;
    return true;
}
static int read_archive(const TrophyCommunicationId *id,uint8_t **output,size_t *size)
{
    char path[1024];
    int length=snprintf(path,sizeof(path),"%s/sce_sys/trophy/%.9s_%02u/TROPHY.TRP",VITA2HOS_APP0_PATH,id->data,id->num);
    if(length<0 || (size_t)length>=sizeof(path))return TROPHY_INVALID_ARGUMENT;
    FILE *file=fopen(path,"rb");
    if(!file)return errno==ENOENT ? TROPHY_TRP_NOT_FOUND : TROPHY_BROKEN_DATA;
    if(fseek(file,0,SEEK_END)){fclose(file);return TROPHY_BROKEN_DATA;}
    long bytes=ftell(file);
    if(bytes<64 || (unsigned long)bytes>TROPHY_ARCHIVE_MAX_BYTES || fseek(file,0,SEEK_SET)) {
        fclose(file);return TROPHY_INVALID_TRP;
    }
    if((size_t)bytes>TROPHY_CONTEXT_BYTES-context_bytes){fclose(file);return TROPHY_NO_MEMORY;}
    uint8_t *data=malloc((size_t)bytes);
    if(!data){fclose(file);return TROPHY_NO_MEMORY;}
    bool read_ok=fread(data,1,(size_t)bytes,file)==(size_t)bytes && fgetc(file)==EOF && !ferror(file);
    int close_result=fclose(file);
    if(!read_ok || close_result){free(data);return TROPHY_BROKEN_DATA;}
    *output=data;*size=(size_t)bytes;return 0;
}
EXPORT(SceNpTrophy,0xC49FD33F,int,sceNpTrophyCreateContext,int32_t *output,
       const TrophyCommunicationId *communication,const void *signature,uint64_t options)
{
    mutexLock(&trophy_lock);
    int result=0;TrophyContext *slot=NULL;
    uint8_t *bytes=NULL;size_t size=0;TrophyArchive archive={0};TrophyMetadata metadata={0};
    if(!trophy_handles){result=TROPHY_NOT_INITIALIZED;goto done;}
    if(!output || options){result=TROPHY_INVALID_ARGUMENT;goto done;}
    if(!valid_communication(communication)){result=TROPHY_INVALID_COMM_ID;goto done;}
    if(!signature){result=TROPHY_INVALID_COMM_SIGN;goto done;}
    /* The offline resource context does not authenticate publisher signatures
     * or establish a PSN identity. Never treat it as an entitlement check. */
    TrophyCommunicationId id=*communication;
    for(unsigned i=0;i<TROPHY_CONTEXT_LIMIT;i++) {
        TrophyContext *context=&trophy_contexts[i];
        if(!context->uid){if(!slot)slot=context;continue;}
        if(context->communication.num==id.num && !memcmp(context->communication.data,id.data,9)) {
            result=TROPHY_CONTEXT_EXISTS;goto done;
        }
    }
    if(!slot){result=TROPHY_EXCEEDS_MAX;goto done;}
    result=read_archive(&id,&bytes,&size);if(result)goto done;
    int parsed=trophy_archive_open(bytes,size,&archive);
    if(parsed) {
        result=parsed==TROPHY_ARCHIVE_NO_MEMORY ? TROPHY_NO_MEMORY :
            parsed==TROPHY_ARCHIVE_CHECKSUM ? TROPHY_TRP_VERIFICATION :
            parsed==TROPHY_ARCHIVE_UNSUPPORTED ? TROPHY_UNSUPPORTED_TRP : TROPHY_INVALID_TRP;
        goto done;
    }
    const TrophyArchiveEntry *conf=trophy_archive_find(&archive,"TROPCONF.SFM");
    if(!conf){result=TROPHY_INVALID_CONF;goto done;}
    parsed=trophy_metadata_parse(bytes+(size_t)conf->offset,(size_t)conf->size,&metadata);
    if(parsed){result=parsed==TROPHY_METADATA_NO_MEMORY ? TROPHY_NO_MEMORY : TROPHY_INVALID_CONF;goto done;}
    char expected[13];snprintf(expected,sizeof(expected),"%.9s_%02u",id.data,id.num);
    if(strcmp(expected,metadata.communication_id)){result=TROPHY_INVALID_COMM_ID;goto done;}
    int32_t uid=SceSysmem_get_next_uid();
    if(uid<=0){result=TROPHY_NO_MEMORY;goto done;}
    *slot=(TrophyContext){.uid=uid,.communication=id,.bytes=bytes,.archive=archive,.metadata=metadata};
    context_bytes+=size;*output=uid;bytes=NULL;archive=(TrophyArchive){0};
done:
    trophy_archive_close(&archive);free(bytes);
    mutexUnlock(&trophy_lock);
    LOG("sceNpTrophyCreateContext result=0x%x",result);
    return result;
}
EXPORT(SceNpTrophy,0x56F5CBA5,int,sceNpTrophyDestroyContext,int32_t uid)
{
    mutexLock(&trophy_lock);
    TrophyContext *context=find_context(uid);
    int result=!trophy_handles ? TROPHY_NOT_INITIALIZED : !context ? TROPHY_INVALID_CONTEXT : 0;
    if(context)clear_context(context);
    mutexUnlock(&trophy_lock);return result;
}
EXPORT(SceNpTrophy,0xFE382529,int,sceNpTrophyGetGameIcon,int32_t context_uid,int32_t handle_uid,
       void *output,uint32_t *size)
{
    mutexLock(&trophy_lock);
    TrophyContext *context=find_context(context_uid);TrophyHandle *handle=find_handle(handle_uid);
    int result=0;
    if(!trophy_handles)result=TROPHY_NOT_INITIALIZED;
    else if(!context)result=TROPHY_INVALID_CONTEXT;
    else if(!handle)result=TROPHY_INVALID_HANDLE;
    else if(handle->aborted)result=TROPHY_ABORT;
    else if(!size)result=TROPHY_INVALID_ARGUMENT;
    else {
        const TrophyArchiveEntry *icon=trophy_archive_find(&context->archive,"ICON0.PNG");
        if(!icon)result=TROPHY_ICON_NOT_FOUND;
        else if(!output)*size=(uint32_t)icon->size;
        else if(*size<icon->size){*size=(uint32_t)icon->size;result=TROPHY_INSUFFICIENT_BUFFER;}
        else {
            result=trophy_archive_read(&context->archive,"ICON0.PNG",0,output,(size_t)icon->size);
            if(result)result=TROPHY_BROKEN_DATA;else *size=(uint32_t)icon->size;
        }
    }
    mutexUnlock(&trophy_lock);return result;
}
DECLARE_LIBRARY(SceNpTrophy,0x4332C10D);
