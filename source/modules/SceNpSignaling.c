#include <switch.h>
#include "module.h"
#include "log.h"

/* NP signaling bring-up: LittleBigPlanet init/term pairs during boot.
 * Library NID 0xEE59FC02 per vitasdk db/360. No signaling backend exists. */

EXPORT(SceNpSignaling,0x4B6ACF47,int,sceNpSignalingInit,void)
{
    LOG("sceNpSignalingInit");
    return 0;
}

EXPORT(SceNpSignaling,0xBC892D18,int,sceNpSignalingTerm,void)
{
    LOG("sceNpSignalingTerm");
    return 0;
}


EXPORT(SceNpSignaling,0xF77EF683,int,sceNpSignalingCreateCtx,void *param,unsigned int *ctxId)
{
    LOG("sceNpSignalingCreateCtx param=%p -> 0",param);
    if(ctxId)*ctxId=0;
    return 0;
}

EXPORT(SceNpSignaling,0xEAA4B1F3,int,sceNpSignalingDestroyCtx,unsigned int ctxId)
{
    LOG("sceNpSignalingDestroyCtx ctxId=%u",ctxId);
    return 0;
}

EXPORT(SceNpSignaling,0x92FFBDE3,int,sceNpSignalingActivateConnection,unsigned int ctxId,const void *npId)
{
    LOG("sceNpSignalingActivateConnection ctxId=%u (offline)",ctxId);
    return 0;
}

EXPORT(SceNpSignaling,0x3B56E6C2,int,sceNpSignalingDeactivateConnection,unsigned int ctxId,const void *npId)
{
    return 0;
}

EXPORT(SceNpSignaling,0x83B3DA0A,int,sceNpSignalingGetConnectionStatus,unsigned int ctxId,const void *npId,unsigned int *status)
{
    if(status)*status=0;
    return 0;
}

EXPORT(SceNpSignaling,0x51883EAE,int,sceNpSignalingGetConnectionInfo,unsigned int ctxId,const void *npId,unsigned int type,void *info)
{
    return 0x80552B07;
}

DECLARE_LIBRARY(SceNpSignaling,0xEE59FC02);
