#include <switch.h>
#include <string.h>
#include "module.h"
#include "log.h"

/* NP common/auth bring-up: LittleBigPlanet init/term pairs during boot.
 * Library NID 0xECB62224 per vitasdk db/360. No auth backend exists. */

EXPORT(SceNpCommon,0x441D8B4E,int,sceNpAuthInit,void)
{
    LOG("sceNpAuthInit");
    return 0;
}

EXPORT(SceNpCommon,0x6093B689,int,sceNpAuthTerm,void)
{
    LOG("sceNpAuthTerm");
    return 0;
}


/* Auth ticket flow: offline, no ticket is ever issued. */
EXPORT(SceNpCommon,0x59608D1C,int,sceNpAuthGetTicket,int reqId,void *ticket,unsigned int *size)
{
    LOG("sceNpAuthGetTicket reqId=%d -> not signed in",reqId);
    return 0x80552B07;
}

EXPORT(SceNpCommon,0xC1E23E01,int,sceNpAuthGetTicketParam,int reqId,void *param)
{
    return 0x80552B07;
}

EXPORT(SceNpCommon,0xED42079F,int,sceNpAuthCreateStartRequest,int commId,void *commPassphrase)
{
    return 0x80552B07;
}

EXPORT(SceNpCommon,0xE807D0BC,int,sceNpSetPlatformType,unsigned int type)
{
    LOG("sceNpSetPlatformType type=0x%x",(unsigned)type);
    return 0;
}

EXPORT(SceNpCommon,0xFB8D82E5,int,sceNpCmpNpId,const void *npIdA,const void *npIdB)
{
    /* 0 when equal, 1 when different (per SDK docs). */
    return npIdA&&npIdB&&memcmp(npIdA,npIdB,0x24)==0 ? 0 : 1;
}

DECLARE_LIBRARY(SceNpCommon,0xECB62224);
