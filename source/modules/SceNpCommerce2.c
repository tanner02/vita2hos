#include <switch.h>
#include "module.h"
#include "log.h"

/* NP commerce bring-up: LittleBigPlanet init/term pairs during boot.
 * Library NID 0x40120DC3 per vitasdk db/360. No store backend exists. */

EXPORT(SceNpCommerce2,0xC73F209A,int,sceNpCommerce2Init,void)
{
    LOG("sceNpCommerce2Init");
    return 0;
}

EXPORT(SceNpCommerce2,0xB99958AE,int,sceNpCommerce2Term,void)
{
    LOG("sceNpCommerce2Term");
    return 0;
}

DECLARE_LIBRARY(SceNpCommerce2,0x40120DC3);
