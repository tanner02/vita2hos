#include <switch.h>
#include "module.h"
#include "log.h"

/* NP activity bring-up: LittleBigPlanet init/term pairs during boot.
 * Library NID 0xFEF492ED per vitasdk db/360. No activity backend exists. */

EXPORT(SceNpActivity,0xE0FFEE97,int,sceNpActivityInit,void)
{
    LOG("sceNpActivityInit");
    return 0;
}

EXPORT(SceNpActivity,0x9EA4901F,int,sceNpActivityTerm,void)
{
    LOG("sceNpActivityTerm");
    return 0;
}


EXPORT(SceNpActivity,0xBC7FDC77,int,sceNpActivityPostStatus,unsigned int status,unsigned int flags,const void *info)
{
    LOG("sceNpActivityPostStatus status=0x%x (dropped)",(unsigned)status);
    return 0;
}

DECLARE_LIBRARY(SceNpActivity,0xFEF492ED);
