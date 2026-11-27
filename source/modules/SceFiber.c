#include <switch.h>
#include <string.h>
#include "module.h"
#include "log.h"

/* Fiber (coroutine) bring-up: LittleBigPlanet runs its job engine through
 * fibers. Library NID 0xF88DAA70 per vitasdk db/360. Real stack switching is
 * not implemented; fibers execute INLINE on the calling thread: Run invokes
 * the stored entry with (argOnInitialize, argOnRun) and ReturnToThread simply
 * returns, so a yielding fiber runs to completion. The guest ABI stays
 * intact because everything happens on the same guest thread and stack. */

typedef void (*SceFiberEntryFn)(unsigned int argOnInitialize,unsigned int argOnRun);

/* SceFiber is 128 opaque bytes; keep our dispatch record in its first words
 * and leave the rest untouched for the guest. */
typedef struct FiberRecord {
    SceFiberEntryFn entry;
    unsigned int argOnInitialize;
} FiberRecord;

_Static_assert(sizeof(FiberRecord)<=128,"FiberRecord must fit SceFiber");

EXPORT(SceFiber,0xF24A298C,int,_sceFiberInitializeImpl,void *fiber,char *name,SceFiberEntryFn entry,unsigned int argOnInitialize,void *addrContext,unsigned int sizeContext,void *params)
{
    LOG("sceFiberInitialize name=%s entry=%p arg=0x%x context=%p size=0x%x",
        name?name:"(null)",(void*)entry,argOnInitialize,addrContext,(unsigned)sizeContext);
    if(!fiber||!entry)return (int)0x80590001; /* SCE_FIBER_ERROR_NULL */
    FiberRecord record={entry,argOnInitialize};
    memcpy(fiber,&record,sizeof(record));
    return 0;
}

EXPORT(SceFiber,0x7DF23243,int,sceFiberRun,void *fiber,unsigned int argOnRunTo,unsigned int *argOnRun)
{
    if(!fiber)return (int)0x80590001;
    FiberRecord record;
    memcpy(&record,fiber,sizeof(record));
    LOG("sceFiberRun fiber=%p entry=%p argOnRunTo=0x%x (inline)",
        fiber,(void*)record.entry,(unsigned)argOnRunTo);
    if(!record.entry)return (int)0x80590004; /* SCE_FIBER_ERROR_INVALID */
    record.entry(record.argOnInitialize,argOnRunTo);
    if(argOnRun)*argOnRun=0;
    return 0;
}

EXPORT(SceFiber,0x3B42921F,int,sceFiberReturnToThread,unsigned int argOnRun,void *option)
{
    /* Inline execution: a yield falls through and the entry runs to
     * completion on the same stack. */
    return 0;
}

EXPORT(SceFiber,0xE160F844,int,sceFiberFinalize,void *fiber)
{
    LOG("sceFiberFinalize fiber=%p",fiber);
    return 0;
}

DECLARE_LIBRARY(SceFiber,0xF88DAA70);
