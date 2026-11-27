#include <switch.h>
#include <string.h>
#include "module.h"
#include "log.h"
#include "modules/SceNpManager.h"

/* NP manager bring-up: LittleBigPlanet queries the local PlayStation Network
 * identity during boot. Library NID 0xD8835093 per vitasdk db/360. */

#define SCE_NP_ONLINEID_MAX_LENGTH 16
#define SCE_NP_MANAGER_ERROR_ID_NOT_AVAIL 0x80552b03

/* Layout per Vita3K np/common.h: 20-byte online ID (16 chars, NUL term and
 * padding), 4 unknown bytes, "psp2" platform tag, validity flag and 7
 * reserved bytes. */
typedef struct SceNpOnlineId {
    char data[SCE_NP_ONLINEID_MAX_LENGTH];
    char term;
    char dummy[3];
} SceNpOnlineId;

typedef struct SceNpIdOptParam {
    uint8_t unknown[4];
    char platformType[4];
} SceNpIdOptParam;

typedef struct SceNpId {
    SceNpOnlineId handle;
    SceNpIdOptParam opt;
    int8_t isIdValid;
    uint8_t reserved[7];
} SceNpId;

_Static_assert(sizeof(SceNpId)==0x24,"SceNpId must be 0x24 bytes");

static const char np_online_id[]="vita2hos";

static bool np_initialized;
static void *service_state_callback,*service_state_data;

void SceNpManager_finish(void)
{
    np_initialized=false;
    service_state_callback=NULL;
    service_state_data=NULL;
}

EXPORT(SceNpManager,0x3C94B4B4,int,sceNpManagerGetNpId,SceNpId *id)
{
    if(!id)return (int)SCE_NP_MANAGER_ERROR_ID_NOT_AVAIL;
    memset(id,0,sizeof(*id));
    strncpy(id->handle.data,np_online_id,SCE_NP_ONLINEID_MAX_LENGTH);
    memcpy(id->opt.platformType,"psp2",4);
    id->isIdValid=1;
    LOG("sceNpManagerGetNpId handle=%s",(const char*)id->handle.data);
    return 0;
}

EXPORT(SceNpManager,0x04D9F484,int,sceNpInit,const void *commConfig,void *dontcare)
{
    if(np_initialized)return 0;
    LOG("sceNpInit commConfig=%p",(void*)commConfig);
    np_initialized=true;
    return 0;
}

EXPORT(SceNpManager,0x19E40AE1,int,sceNpTerm,void)
{
    LOG("sceNpTerm");
    np_initialized=false;
    return 0;
}

/* The service-state callback is recorded but never fired: no NP backend
 * exists, so the guest observes a silently signed-out session. */
EXPORT(SceNpManager,0x44239C35,int,sceNpRegisterServiceStateCallback,void *callback,void *data)
{
    LOG("sceNpRegisterServiceStateCallback callback=%p data=%p",callback,data);
    service_state_callback=callback;
    service_state_data=data;
    return 0;
}

EXPORT(SceNpManager,0xD9E6E56C,int,sceNpUnregisterServiceStateCallback,void *callback)
{
    LOG("sceNpUnregisterServiceStateCallback callback=%p",callback);
    if(service_state_callback==callback)service_state_callback=NULL;
    return 0;
}


EXPORT(SceNpManager,0x3B0AE9A9,int,sceNpCheckCallback,void)
{
    return 0;
}

EXPORT(SceNpManager,0x54060DF6,int,sceNpGetServiceState,unsigned int *state)
{
    /* 0 = signed out. */
    if(state)*state=0;
    return 0;
}

EXPORT(SceNpManager,0x60C575B1,int,sceNpManagerGetChatRestrictionFlag,unsigned char *flag)
{
    if(flag)*flag=0;
    return 0;
}

EXPORT(SceNpManager,0xAF0073B2,int,sceNpManagerGetContentRatingFlag,unsigned char *flag)
{
    if(flag)*flag=0;
    return 0;
}

EXPORT(SceNpManager,0xFE835967,int,sceNpManagerGetAccountRegion,void *region)
{
    LOG("sceNpManagerGetAccountRegion -> zeros");
    if(region)memset(region,0,0x14);
    return 0;
}

DECLARE_LIBRARY(SceNpManager,0xD8835093);
