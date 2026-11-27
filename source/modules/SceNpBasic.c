#include <switch.h>
#include "module.h"
#include "log.h"

/* NP basic bring-up: LittleBigPlanet registers presence/friend handlers.
 * Library NID 0x605BA760 per vitasdk db/360. Handlers are recorded but
 * never fired; friend/presence queries report "not signed in". */

#define SCE_NP_ERROR_NOT_SIGNED_IN 0x80552B07

static Mutex npbasic_lock;
static void *npbasic_handler,*npbasic_handler_arg;
static bool npbasic_inited;

EXPORT(SceNpBasic,0xEFB91A99,int,sceNpBasicInit,void)
{
    mutexLock(&npbasic_lock);
    LOG("sceNpBasicInit");
    npbasic_inited=true;
    mutexUnlock(&npbasic_lock);
    return 0;
}

EXPORT(SceNpBasic,0x389BCB3B,int,sceNpBasicTerm,void)
{
    mutexLock(&npbasic_lock);
    LOG("sceNpBasicTerm");
    npbasic_inited=false;
    npbasic_handler=NULL;
    npbasic_handler_arg=NULL;
    mutexUnlock(&npbasic_lock);
    return 0;
}

EXPORT(SceNpBasic,0x26E6E048,int,sceNpBasicRegisterHandler,unsigned int messageMode,void *callback,void *userArg)
{
    mutexLock(&npbasic_lock);
    npbasic_handler=callback;
    npbasic_handler_arg=userArg;
    mutexUnlock(&npbasic_lock);
    LOG("sceNpBasicRegisterHandler mode=0x%x callback=%p",(unsigned)messageMode,callback);
    return 0;
}

EXPORT(SceNpBasic,0x050AE072,int,sceNpBasicUnregisterHandler,unsigned int messageMode)
{
    mutexLock(&npbasic_lock);
    npbasic_handler=NULL;
    npbasic_handler_arg=NULL;
    mutexUnlock(&npbasic_lock);
    LOG("sceNpBasicUnregisterHandler mode=0x%x",(unsigned)messageMode);
    return 0;
}

EXPORT(SceNpBasic,0x20146AEC,int,sceNpBasicCheckCallback,void)
{
    return 0;
}

EXPORT(SceNpBasic,0x51D75562,int,sceNpBasicSetInGamePresence,unsigned int state)
{
    LOG("sceNpBasicSetInGamePresence state=0x%x",(unsigned)state);
    return 0;
}

EXPORT(SceNpBasic,0x5183A4B5,int,sceNpBasicGetFriendOnlineStatus,void *friendDetails,unsigned int *status)
{
    return (int)SCE_NP_ERROR_NOT_SIGNED_IN;
}

EXPORT(SceNpBasic,0xEF8A91BC,int,sceNpBasicGetGamePresenceOfFriend,const void *npId,void *presence)
{
    return (int)SCE_NP_ERROR_NOT_SIGNED_IN;
}

EXPORT(SceNpBasic,0x407E1E6F,int,sceNpBasicGetBlockListEntryCount,void)
{
    return 0;
}

EXPORT(SceNpBasic,0x1211AE8E,int,sceNpBasicGetBlockListEntries,void *entries,unsigned int size,unsigned int *count)
{
    if(count)*count=0;
    return 0;
}

EXPORT(SceNpBasic,0x7A5020A5,int,sceNpBasicSendInGameDataMessage,const void *npId,const void *data,unsigned int size)
{
    LOG("sceNpBasicSendInGameDataMessage size=%u (dropped)",(unsigned)size);
    return 0;
}

EXPORT(SceNpBasic,0x3B0A7F47,int,sceNpBasicRecordPlaySessionLog,const void *npId,unsigned int state,unsigned int playSessionLogParam)
{
    LOG("sceNpBasicRecordPlaySessionLog state=%u (dropped)",(unsigned)state);
    return 0;
}

DECLARE_LIBRARY(SceNpBasic,0x605BA760);
