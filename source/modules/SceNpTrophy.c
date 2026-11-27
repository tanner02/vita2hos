#include <stdint.h>
#include <psp2/types.h>
#include <stdlib.h>
#include <switch.h>
#include "module.h"
#include "modules/SceNpTrophy.h"
#include "modules/SceSysmem.h"
/* The pinned SDK lacks trophy declarations. These errors/signatures match the
 * pinned public reference. Only library/operation-handle lifetime is supported. */
#define TROPHY_NOT_INITIALIZED ((int)0x80551601)
#define TROPHY_ALREADY_INITIALIZED ((int)0x80551602)
#define TROPHY_NO_MEMORY ((int)0x80551603)
#define TROPHY_INVALID_ARGUMENT ((int)0x80551604)
#define TROPHY_EXCEEDS_MAX ((int)0x80551606)
#define TROPHY_INVALID_HANDLE ((int)0x80551608)
#define TROPHY_HANDLE_LIMIT 64

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
    mutexLock(&trophy_lock);free(trophy_handles);trophy_handles=NULL;mutexUnlock(&trophy_lock);
}
EXPORT(SceNpTrophy,0x34516838,int,sceNpTrophyInit,const void *options)
{
    mutexLock(&trophy_lock);
    int result=0;
    if(trophy_handles)result=TROPHY_ALREADY_INITIALIZED;
    else if(options)result=TROPHY_INVALID_ARGUMENT;
    else if(!(trophy_handles=calloc(TROPHY_HANDLE_LIMIT,sizeof(*trophy_handles))))result=TROPHY_NO_MEMORY;
    mutexUnlock(&trophy_lock);
    return result;
}
EXPORT(SceNpTrophy,0xBFE0F28F,int,sceNpTrophyTerm,void)
{
    mutexLock(&trophy_lock);
    int result=trophy_handles ? 0 : TROPHY_NOT_INITIALIZED;
    free(trophy_handles);trophy_handles=NULL;
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
DECLARE_LIBRARY(SceNpTrophy,0x4332C10D);
