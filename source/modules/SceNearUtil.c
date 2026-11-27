#include <switch.h>
#include "module.h"
#include "log.h"

/* Near (gift/spot) utility bring-up: LittleBigPlanet init/term pairs during
 * boot. Library NID 0x560D1608 per vitasdk db/360. No near backend exists. */

EXPORT(SceNearUtil,0xBBCA5813,int,sceNearInitialize,void)
{
    LOG("sceNearInitialize");
    return 0;
}

EXPORT(SceNearUtil,0xFF3BC581,int,sceNearFinalize,void)
{
    LOG("sceNearFinalize");
    return 0;
}


/* Gift/near features: no neighbors and no gifts exist offline, so every
 * accessor reports zero gifts or an end-of-collection state. */

EXPORT(SceNearUtil,0xAD264F5F,int,sceNearGetNeighbors,void *neighbors,unsigned int *count)
{
    if(count)*count=0;
    LOG("sceNearGetNeighbors -> 0");
    return 0;
}

EXPORT(SceNearUtil,0xDE6F0859,int,sceNearGetDiscoveredGifts,void *param,unsigned int *count)
{
    if(count)*count=0;
    return 0;
}

EXPORT(SceNearUtil,0x773ABEA9,int,sceNearGetDiscoveredGiftInfo,unsigned int index,void *info)
{
    return -1;
}

EXPORT(SceNearUtil,0xA64D9D38,int,sceNearGetDiscoveredGiftStatus,unsigned int index)
{
    return 0;
}

EXPORT(SceNearUtil,0xEFE6F060,int,sceNearGetDiscoveredGiftSender,unsigned int index,void *sender)
{
    return -1;
}

EXPORT(SceNearUtil,0xF8C15008,int,sceNearOpenDiscoveredGiftImage,unsigned int index,void *param)
{
    return -1;
}

EXPORT(SceNearUtil,0x88A68D20,int,sceNearCloseDiscoveredGiftImage,unsigned int index)
{
    return 0;
}

EXPORT(SceNearUtil,0x2364D6BD,int,sceNearReadDiscoveredGiftImage,unsigned int index,void *buffer,unsigned int size)
{
    return 0;
}

EXPORT(SceNearUtil,0x80D6C552,int,sceNearConvertDiscoveredGiftParam,void *dst,void *src)
{
    return -1;
}

EXPORT(SceNearUtil,0x8CBEB2DA,int,sceNearDeleteDiscoveredGift,unsigned int index)
{
    return 0;
}

EXPORT(SceNearUtil,0x146BE236,int,sceNearSetGift,void *param)
{
    return 0;
}

EXPORT(SceNearUtil,0xDB8BAC35,int,sceNearGetGift,void *param)
{
    return 0;
}

EXPORT(SceNearUtil,0x52021026,int,sceNearDeleteGift,unsigned int type)
{
    return 0;
}

EXPORT(SceNearUtil,0x06807CA3,int,sceNearOpenReceivedGiftData,void *param)
{
    return 0;
}

EXPORT(SceNearUtil,0x2F35C196,int,sceNearCloseReceivedGiftData,void *param)
{
    return 0;
}

EXPORT(SceNearUtil,0xB14145DD,int,sceNearReadReceivedGiftData,void *param,void *buffer,unsigned int *size)
{
    if(size)*size=0;
    return 0;
}

EXPORT(SceNearUtil,0x2ED36EE2,int,sceNearRefresh,unsigned int type,unsigned int mode)
{
    return 0;
}

EXPORT(SceNearUtil,0x3F3F6D92,int,sceNearFinalizeAndLaunchNearApp,void)
{
    LOG("sceNearFinalizeAndLaunchNearApp (no near app)");
    return 0;
}

EXPORT(SceNearUtil,0xF3398774,int,sceNearLaunchNearAppForUpdate,unsigned int type)
{
    return 0;
}

DECLARE_LIBRARY(SceNearUtil,0x560D1608);
