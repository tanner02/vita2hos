#include <switch.h>
#include "module.h"
#include "log.h"

/* NP messaging bring-up: LittleBigPlanet polls the message box during boot.
 * Library NID 0x8134120B per vitasdk db/360. Offline: zero messages. */

static bool npmessage_inited;

EXPORT(SceNpMessage,0x258DA4AC,int,sceNpMessageInit,void)
{
    LOG("sceNpMessageInit");
    npmessage_inited=true;
    return 0;
}

EXPORT(SceNpMessage,0x380230A1,int,sceNpMessageTerm,void)
{
    LOG("sceNpMessageTerm");
    npmessage_inited=false;
    return 0;
}

EXPORT(SceNpMessage,0x1DBD26D2,int,sceNpMessageGetMessageEntryCount,unsigned int *count)
{
    if(count)*count=0;
    LOG("sceNpMessageGetMessageEntryCount -> 0");
    return 0;
}

EXPORT(SceNpMessage,0xC86B6DA7,int,sceNpMessageGetMessageEntries,unsigned int startIndex,void *entries,unsigned int bufferSize,unsigned int *count)
{
    if(count)*count=0;
    LOG("sceNpMessageGetMessageEntries startIndex=%u -> 0",(unsigned)startIndex);
    return 0;
}

EXPORT(SceNpMessage,0x7E697F98,int,sceNpMessageGetAttachedData,const void *entry,void *buffer,unsigned int *size)
{
    return -1;
}

DECLARE_LIBRARY(SceNpMessage,0x8134120B);
