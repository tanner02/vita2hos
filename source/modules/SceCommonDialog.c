#include <switch.h>
#include <string.h>
#include "module.h"
#include "log.h"

/* Common dialog bring-up: LittleBigPlanet drives the dialog API surface
 * (net check, message, save data, NP and store dialogs). Library NID
 * 0xE537816C per vitasdk db/360. No host dialog UI is rendered: every
 * dialog opens and immediately reports FINISHED with an OK result so the
 * guest proceeds. */

#define SCE_COMMON_DIALOG_STATUS_NONE     0
#define SCE_COMMON_DIALOG_STATUS_RUNNING  1
#define SCE_COMMON_DIALOG_STATUS_FINISHED 2

/* Layout per Vita3K dialog/types.h SceSaveDataDialogResult. */
typedef struct SceSaveDataDialogResult {
    int mode;
    int result;
    int buttonId;
    int slotId;
    void *slotInfo;
    void *userdata;
    char reserved[32];
} SceSaveDataDialogResult;

_Static_assert(sizeof(SceSaveDataDialogResult)==0x38,"SceSaveDataDialogResult must be 0x38");

enum {
    DLG_MESSAGE, DLG_NETCHECK, DLG_SAVEDATA, DLG_IME,
    DLG_NP_FRIENDLIST, DLG_NP_MESSAGE, DLG_NP_PROFILE,
    DLG_STORE_CHECKOUT, DLG_NP_TROPHY_SETUP, DLG_COUNT
};

static Mutex dialog_lock;
static int dialog_status[DLG_COUNT];

static void dialog_set(int dialog,int status)
{
    mutexLock(&dialog_lock);
    dialog_status[dialog]=status;
    mutexUnlock(&dialog_lock);
}

static int dialog_get(int dialog)
{
    mutexLock(&dialog_lock);
    int status=dialog_status[dialog];
    mutexUnlock(&dialog_lock);
    return status;
}

EXPORT(SceCommonDialog,0xBECD35C8,int,sceCommonDialogSetConfigParam,const void *configParam)
{
    LOG("sceCommonDialogSetConfigParam configParam=%p",(void*)configParam);
    return 0;
}

EXPORT(SceCommonDialog,0x90530F2F,int,sceCommonDialogUpdate,void)
{
    return 0;
}

/* Message dialog */
EXPORT(SceCommonDialog,0x755FF270,int,sceMsgDialogInit,const void *param)
{
    LOG("sceMsgDialogInit param=%p",(void*)param);
    dialog_set(DLG_MESSAGE,SCE_COMMON_DIALOG_STATUS_FINISHED);
    return 0;
}

EXPORT(SceCommonDialog,0x4107019E,int,sceMsgDialogGetStatus,void)
{
    return dialog_get(DLG_MESSAGE);
}

EXPORT(SceCommonDialog,0x81ACF695,int,sceMsgDialogTerm,void)
{
    dialog_set(DLG_MESSAGE,SCE_COMMON_DIALOG_STATUS_NONE);
    return 0;
}

/* Net check dialog */
EXPORT(SceCommonDialog,0xA38A4A0D,int,sceNetCheckDialogInit,void)
{
    LOG("sceNetCheckDialogInit");
    dialog_set(DLG_NETCHECK,SCE_COMMON_DIALOG_STATUS_FINISHED);
    return 0;
}

EXPORT(SceCommonDialog,0x8027292A,int,sceNetCheckDialogGetStatus,void)
{
    return dialog_get(DLG_NETCHECK);
}

EXPORT(SceCommonDialog,0xB05FCE9E,int,sceNetCheckDialogGetResult,void *result)
{
    LOG("sceNetCheckDialogGetResult result=%p",(void*)result);
    return 0;
}

EXPORT(SceCommonDialog,0x2D8EDF09,int,sceNetCheckDialogAbort,void)
{
    dialog_set(DLG_NETCHECK,SCE_COMMON_DIALOG_STATUS_NONE);
    return 0;
}

EXPORT(SceCommonDialog,0x8BE51C15,int,sceNetCheckDialogTerm,void)
{
    dialog_set(DLG_NETCHECK,SCE_COMMON_DIALOG_STATUS_NONE);
    return 0;
}

/* Save data dialog: no UI, reports an immediate finished OK so the game's
 * save flow uses its default slot behavior. */
EXPORT(SceCommonDialog,0xBF5248FA,int,sceSaveDataDialogInit,const void *param)
{
    LOG("sceSaveDataDialogInit param=%p",(void*)param);
    dialog_set(DLG_SAVEDATA,SCE_COMMON_DIALOG_STATUS_FINISHED);
    return 0;
}

EXPORT(SceCommonDialog,0x6E258046,int,sceSaveDataDialogGetStatus,void)
{
    return dialog_get(DLG_SAVEDATA);
}

EXPORT(SceCommonDialog,0xBA0542CA,int,sceSaveDataDialogGetSubStatus,void)
{
    return dialog_get(DLG_SAVEDATA);
}

EXPORT(SceCommonDialog,0xB2FF576E,int,sceSaveDataDialogGetResult,SceSaveDataDialogResult *result)
{
    LOG("sceSaveDataDialogGetResult result=%p",(void*)result);
    if(result) {
        memset(result,0,sizeof(*result));
        result->result=0; /* SCE_COMMON_DIALOG_RESULT_OK */
    }
    return 0;
}

EXPORT(SceCommonDialog,0x6C49924B,int,sceSaveDataDialogFinish,void)
{
    dialog_set(DLG_SAVEDATA,SCE_COMMON_DIALOG_STATUS_NONE);
    return 0;
}

EXPORT(SceCommonDialog,0x2192A10A,int,sceSaveDataDialogTerm,void)
{
    dialog_set(DLG_SAVEDATA,SCE_COMMON_DIALOG_STATUS_NONE);
    return 0;
}

/* IME dialog */
EXPORT(SceCommonDialog,0xCF0431FD,int,sceImeDialogGetStatus,void)
{
    return dialog_get(DLG_IME);
}

/* NP dialogs */
EXPORT(SceCommonDialog,0x93FCFEC6,int,sceNpFriendListDialogInit,const void *param)
{
    LOG("sceNpFriendListDialogInit param=%p",(void*)param);
    dialog_set(DLG_NP_FRIENDLIST,SCE_COMMON_DIALOG_STATUS_FINISHED);
    return 0;
}

EXPORT(SceCommonDialog,0x1FD5D373,int,sceNpFriendListDialogGetStatus,void)
{
    return dialog_get(DLG_NP_FRIENDLIST);
}

EXPORT(SceCommonDialog,0x58FA2062,int,sceNpFriendListDialogAbort,void)
{
    dialog_set(DLG_NP_FRIENDLIST,SCE_COMMON_DIALOG_STATUS_NONE);
    return 0;
}

EXPORT(SceCommonDialog,0x4A880C6A,int,sceNpFriendListDialogTerm,void)
{
    dialog_set(DLG_NP_FRIENDLIST,SCE_COMMON_DIALOG_STATUS_NONE);
    return 0;
}

EXPORT(SceCommonDialog,0x4535A358,int,sceNpMessageDialogInit,const void *param)
{
    LOG("sceNpMessageDialogInit param=%p",(void*)param);
    dialog_set(DLG_NP_MESSAGE,SCE_COMMON_DIALOG_STATUS_FINISHED);
    return 0;
}

EXPORT(SceCommonDialog,0x2A0D060F,int,sceNpMessageDialogGetStatus,void)
{
    return dialog_get(DLG_NP_MESSAGE);
}

EXPORT(SceCommonDialog,0x47AB6D04,int,sceNpMessageDialogAbort,void)
{
    dialog_set(DLG_NP_MESSAGE,SCE_COMMON_DIALOG_STATUS_NONE);
    return 0;
}

EXPORT(SceCommonDialog,0x7AB50F63,int,sceNpMessageDialogTerm,void)
{
    dialog_set(DLG_NP_MESSAGE,SCE_COMMON_DIALOG_STATUS_NONE);
    return 0;
}

EXPORT(SceCommonDialog,0x5E0AFDF8,int,sceNpProfileDialogInit,const void *param)
{
    LOG("sceNpProfileDialogInit param=%p",(void*)param);
    dialog_set(DLG_NP_PROFILE,SCE_COMMON_DIALOG_STATUS_FINISHED);
    return 0;
}

EXPORT(SceCommonDialog,0xFDFE6042,int,sceNpProfileDialogGetStatus,void)
{
    return dialog_get(DLG_NP_PROFILE);
}

EXPORT(SceCommonDialog,0x2B02BE3F,int,sceNpProfileDialogAbort,void)
{
    dialog_set(DLG_NP_PROFILE,SCE_COMMON_DIALOG_STATUS_NONE);
    return 0;
}

EXPORT(SceCommonDialog,0x04B63D6F,int,sceNpProfileDialogTerm,void)
{
    dialog_set(DLG_NP_PROFILE,SCE_COMMON_DIALOG_STATUS_NONE);
    return 0;
}

/* Store checkout dialog */
EXPORT(SceCommonDialog,0x52ECD8A5,int,sceStoreCheckoutDialogInit,const void *param)
{
    LOG("sceStoreCheckoutDialogInit param=%p",(void*)param);
    dialog_set(DLG_STORE_CHECKOUT,SCE_COMMON_DIALOG_STATUS_FINISHED);
    return 0;
}

EXPORT(SceCommonDialog,0x7004BB2E,int,sceStoreCheckoutDialogGetStatus,void)
{
    return dialog_get(DLG_STORE_CHECKOUT);
}

EXPORT(SceCommonDialog,0x07ED1E26,int,sceStoreCheckoutDialogGetResult,void *result)
{
    LOG("sceStoreCheckoutDialogGetResult result=%p",(void*)result);
    return 0;
}

EXPORT(SceCommonDialog,0xB787F4B0,int,sceStoreCheckoutDialogTerm,void)
{
    dialog_set(DLG_STORE_CHECKOUT,SCE_COMMON_DIALOG_STATUS_NONE);
    return 0;
}

/* Trophy setup dialog */
EXPORT(SceCommonDialog,0x9E2C02C9,int,sceNpTrophySetupDialogInit,const void *param)
{
    LOG("sceNpTrophySetupDialogInit param=%p",(void*)param);
    dialog_set(DLG_NP_TROPHY_SETUP,SCE_COMMON_DIALOG_STATUS_FINISHED);
    return 0;
}

EXPORT(SceCommonDialog,0xC3A59547,int,sceNpTrophySetupDialogGetStatus,void)
{
    return dialog_get(DLG_NP_TROPHY_SETUP);
}

EXPORT(SceCommonDialog,0xE37069D5,int,sceNpTrophySetupDialogGetResult,void *result)
{
    LOG("sceNpTrophySetupDialogGetResult result=%p",(void*)result);
    return 0;
}

EXPORT(SceCommonDialog,0xA81082DD,int,sceNpTrophySetupDialogTerm,void)
{
    dialog_set(DLG_NP_TROPHY_SETUP,SCE_COMMON_DIALOG_STATUS_NONE);
    return 0;
}

DECLARE_LIBRARY(SceCommonDialog,0xE537816C);
