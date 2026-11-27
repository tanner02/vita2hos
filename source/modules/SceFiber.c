#include <switch.h>
#include "module.h"
#include "log.h"

/* Fiber (coroutine) bring-up: LittleBigPlanet creates fibers for its job
 * engine. Library NID 0xF88DAA70 per vitasdk db/360. Real context switching
 * is NOT implemented: initialization and finalize succeed, but Run returns
 * immediately without executing the fiber entry point, and ReturnToThread
 * returns as if the fiber completed. If the game hangs here, fibers need
 * real coroutine support. */

EXPORT(SceFiber,0xF24A298C,int,_sceFiberInitializeImpl,void *fiber,const char *name,void *entry,unsigned int entryParam,void *context,unsigned int contextSize,void *option)
{
    LOG("sceFiberInitialize name=%s entry=%p contextSize=0x%x",name?name:"(null)",entry,(unsigned)contextSize);
    return 0;
}

EXPORT(SceFiber,0x7DF23243,int,sceFiberRun,void *fiber,unsigned int argOnRun,unsigned int *argOnReturn)
{
    LOG("sceFiberRun fiber=%p arg=0x%x (not executed -> immediate return)",fiber,(unsigned)argOnRun);
    if(argOnReturn)*argOnReturn=0;
    return 0;
}

EXPORT(SceFiber,0x3B42921F,int,sceFiberReturnToThread,unsigned int argOnReturn,void *option)
{
    LOG("sceFiberReturnToThread arg=0x%x (immediate return)",(unsigned)argOnReturn);
    return 0;
}

EXPORT(SceFiber,0xE160F844,int,sceFiberFinalize,void *fiber)
{
    LOG("sceFiberFinalize fiber=%p",fiber);
    return 0;
}

DECLARE_LIBRARY(SceFiber,0xF88DAA70);
