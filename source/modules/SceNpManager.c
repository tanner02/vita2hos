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

void SceNpManager_finish(void)
{
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

DECLARE_LIBRARY(SceNpManager,0xD8835093);
