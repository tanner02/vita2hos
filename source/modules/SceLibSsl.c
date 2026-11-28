#include <switch.h>
#include "module.h"
#include "log.h"

/* SSL library bring-up: LittleBigPlanet pairs SSL init with HTTP init during
 * boot. Library NID 0xCAF5F2E9 per vitasdk db/360. No TLS backend is wired
 * to the host yet; init/term succeed so the guest continues. */

static bool ssl_initialized;

EXPORT(SceSsl,0x3C733316,int,sceSslInit,unsigned int poolSize)
{
    if(ssl_initialized)return 0;
    LOG("sceSslInit poolSize=0x%x",(unsigned)poolSize);
    ssl_initialized=true;
    return 0;
}

EXPORT(SceSsl,0x03CE6E3A,int,sceSslTerm,void)
{
    LOG("sceSslTerm");
    ssl_initialized=false;
    return 0;
}

DECLARE_LIBRARY(SceSsl,0xCAF5F2E9);
