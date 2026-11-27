#include <switch.h>
#include "module.h"
#include "log.h"

/* NP utility bring-up: LittleBigPlanet uses NpLookup for profile lookups and
 * NpWordFilter for comment sanitization. Library NID 0x7B47D9CF per vitasdk
 * db/360. No NP service exists: contexts/requests get local ids, lookups
 * report "not signed in", filters complete with no changes. */

#define SCE_NP_ERROR_NOT_SIGNED_IN 0x80552B07

static Mutex nputil_lock;
static int nputil_next_id;

static int nputil_new_id(void)
{
    mutexLock(&nputil_lock);
    int id=++nputil_next_id;
    mutexUnlock(&nputil_lock);
    return id;
}

EXPORT(SceNpUtility,0x9246A673,int,sceNpLookupInit,void)
{
    LOG("sceNpLookupInit");
    return 0;
}

EXPORT(SceNpUtility,0x0158B61B,int,sceNpLookupTerm,void)
{
    LOG("sceNpLookupTerm");
    return 0;
}

EXPORT(SceNpUtility,0x5110E17E,int,sceNpLookupCreateTitleCtx,int commId,const void *commPassphrase)
{
    return nputil_new_id();
}

EXPORT(SceNpUtility,0x33B64699,int,sceNpLookupDeleteTitleCtx,int ctxId)
{
    return 0;
}

EXPORT(SceNpUtility,0x9E42E922,int,sceNpLookupCreateRequest,int ctxId)
{
    return nputil_new_id();
}

EXPORT(SceNpUtility,0x8B608BF6,int,sceNpLookupDeleteRequest,int reqId)
{
    return 0;
}

/* Offline: profile lookups fail so LBP falls back to local profiles. */
EXPORT(SceNpUtility,0x6A1BF429,int,sceNpLookupUserProfile,int reqId,const void *npId,void *option)
{
    LOG("sceNpLookupUserProfile reqId=%d -> not signed in",reqId);
    return (int)SCE_NP_ERROR_NOT_SIGNED_IN;
}

EXPORT(SceNpUtility,0xE5285E0F,int,sceNpLookupUserProfileAsync,int reqId,const void *npId,void *option,int *reqSlot)
{
    LOG("sceNpLookupUserProfileAsync reqId=%d -> not signed in",reqId);
    return (int)SCE_NP_ERROR_NOT_SIGNED_IN;
}

EXPORT(SceNpUtility,0xB1A14879,int,sceNpLookupNpId,int reqId,const char *onlineId,void *option)
{
    LOG("sceNpLookupNpId reqId=%d -> not signed in",reqId);
    return (int)SCE_NP_ERROR_NOT_SIGNED_IN;
}

EXPORT(SceNpUtility,0xCF956F23,int,sceNpLookupWaitAsync,int reqId,unsigned int timeoutUSec)
{
    return (int)SCE_NP_ERROR_NOT_SIGNED_IN;
}

EXPORT(SceNpUtility,0xFCDBA234,int,sceNpLookupPollAsync,int reqId,unsigned int *result)
{
    if(result)*result=0;
    return 0;
}

EXPORT(SceNpUtility,0x01284C6E,int,sceNpWordFilterInit,void)
{
    LOG("sceNpWordFilterInit");
    return 0;
}

EXPORT(SceNpUtility,0x7D140F70,int,sceNpWordFilterTerm,void)
{
    LOG("sceNpWordFilterTerm");
    return 0;
}

EXPORT(SceNpUtility,0x1075589C,int,sceNpWordFilterCreateTitleCtx,int commId,const void *commPassphrase)
{
    return nputil_new_id();
}

EXPORT(SceNpUtility,0xB4E5A189,int,sceNpWordFilterDeleteTitleCtx,int ctxId)
{
    return 0;
}

EXPORT(SceNpUtility,0x7183047C,int,sceNpWordFilterCreateRequest,int ctxId)
{
    return nputil_new_id();
}

EXPORT(SceNpUtility,0x6690D8E2,int,sceNpWordFilterDeleteRequest,int reqId)
{
    return 0;
}

/* Sanitization completes immediately with no filtered output. */
EXPORT(SceNpUtility,0x40EC7271,int,sceNpWordFilterSanitizeCommentAsync,int reqId,void *option,const char *comment)
{
    LOG("sceNpWordFilterSanitizeCommentAsync reqId=%d comment=%s",reqId,comment?comment:"(null)");
    return 0;
}

EXPORT(SceNpUtility,0xBAE4CD1F,int,sceNpWordFilterPollAsync,int reqId,unsigned int *result)
{
    if(result)*result=0;
    return 1;
}

EXPORT(SceNpUtility,0x36DA662E,int,sceNpWordFilterAbortRequest,int reqId)
{
    return 0;
}

DECLARE_LIBRARY(SceNpUtility,0x7B47D9CF);
