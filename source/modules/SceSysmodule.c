#include <switch.h>
#include <psp2/sysmodule.h>
#include <stdbool.h>
#include <string.h>
#include "module.h"
#include "log.h"
#include "media_probe.h"
#include "modules/SceNet.h"
#include "modules/SceAppUtil.h"
#include "modules/SceNpTrophy.h"
#include "modules/SceSysmodule.h"
static Mutex sysmodule_lock;
static bool net_module_loaded,trophy_module_loaded,apputil_module_loaded;
/* Modules that load as stubs: the guest sees a successful load, and its
 * imports resolve to HLE exports or missing-import stubs as usual.
 * LittleBigPlanet requests this set during boot. */
static bool stub_loaded[SCE_SYSMODULE_JSON + 1];
static bool stub_loadable_module(SceSysmoduleModuleId id)
{
    switch(id) {
    case SCE_SYSMODULE_MP4:
        /* Diagnostic only: pass this gate to capture the first missing media
         * calls. Import stubs still return NO_FUNC_NID and write no outputs. */
        return media_probe_active();
    case SCE_SYSMODULE_HTTPS:          /* 0x04 */
    case SCE_SYSMODULE_NGS:            /* 0x0B: LBP's Audio post-stage aborts
        * startup when this load fails (call chain audited by Codex:
        * lbp-ngs-startup-audit - SystemGetRequiredMemorySize/SystemInit/
        * SetFlags/VoiceDefGetMasterBuss/RackGetRequiredMemorySize/RackInit/
        * RackGetVoiceHandle/VoiceDefGetSimpleVoice/VoicePlay, each accepting
        * exactly zero); the silent SceNgs.c provider backs these imports */
    case SCE_SYSMODULE_FIBER:          /* 0x06 */
    case SCE_SYSMODULE_IME:            /* 0x11 */
    case SCE_SYSMODULE_LIVEAREA:       /* 0x23: LBP aborts online bring-up when this fails */
    case SCE_SYSMODULE_NP_BASIC:       /* 0x12 */
    case SCE_SYSMODULE_NP:             /* 0x15 */
    case SCE_SYSMODULE_NP_COMMERCE2:   /* 0x18 */
    case SCE_SYSMODULE_NP_UTILITY:     /* 0x19 */
    case SCE_SYSMODULE_NP_MATCHING2:   /* 0x1C */
    case SCE_SYSMODULE_NP_ACTIVITY:    /* 0x24 */
    case SCE_SYSMODULE_NP_MESSAGE:     /* 0x26 */
    case SCE_SYSMODULE_NEAR_UTIL:      /* 0x2B */
    case SCE_SYSMODULE_NP_SIGNALING:   /* 0x46 */
    case SCE_SYSMODULE_SHUTTER_SOUND:  /* 0x27: LBP's ShutterSound init stage
        * returns false (fatal startup failure) when this load fails */
        return true;
    default:
        return false;
    }
}
void SceSysmodule_finish(void)
{
    media_probe_finish();
    mutexLock(&sysmodule_lock);net_module_loaded=false;trophy_module_loaded=false;apputil_module_loaded=false;memset(stub_loaded,0,sizeof(stub_loaded));mutexUnlock(&sysmodule_lock);
}
static bool valid_module(SceSysmoduleModuleId id)
{ return (unsigned)id>=1 && (unsigned)id<=(unsigned)SCE_SYSMODULE_JSON; }
EXPORT(SceSysmodule,0x79A0160A,int,sceSysmoduleLoadModule,SceSysmoduleModuleId id)
{
    if(!valid_module(id))return (int)SCE_SYSMODULE_ERROR_INVALID_VALUE;
    mutexLock(&sysmodule_lock);
    int result=(int)SCE_SYSMODULE_ERROR_FATAL;
    if(id==SCE_SYSMODULE_NET && SceNet_host_ready()){net_module_loaded=true;result=0;}
    if(id==SCE_SYSMODULE_NP_TROPHY){trophy_module_loaded=true;result=0;}
    if(id==SCE_SYSMODULE_APPUTIL){apputil_module_loaded=true;result=0;}
    if(stub_loadable_module(id)){stub_loaded[id]=true;result=0;}
    mutexUnlock(&sysmodule_lock);
    LOG("sceSysmoduleLoadModule id=0x%x result=0x%x",(unsigned)id,result);
    if(id==SCE_SYSMODULE_MP4)
        media_probe_record("SYSMODULE_LOAD id=0x002d result=0x%08x caller=0x%08" PRIxPTR
            " module_presence_only=1 decoder_implemented=0",(unsigned)result,
            (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0)));
    return result;
}
EXPORT(SceSysmodule,0x53099B7A,int,sceSysmoduleIsLoaded,SceSysmoduleModuleId id)
{
    if(!valid_module(id))return (int)SCE_SYSMODULE_ERROR_INVALID_VALUE;
    mutexLock(&sysmodule_lock);
    int result=((id==SCE_SYSMODULE_NET && net_module_loaded) ||
                (id==SCE_SYSMODULE_NP_TROPHY && trophy_module_loaded) ||
                (id==SCE_SYSMODULE_APPUTIL && apputil_module_loaded) ||
                (stub_loaded[id])) ?
                0 : (int)SCE_SYSMODULE_ERROR_UNLOADED;
    mutexUnlock(&sysmodule_lock);
    return result;
}
EXPORT(SceSysmodule,0x31D87805,int,sceSysmoduleUnloadModule,SceSysmoduleModuleId id)
{
    if(!valid_module(id))return (int)SCE_SYSMODULE_ERROR_INVALID_VALUE;
    mutexLock(&sysmodule_lock);
    int result=(int)SCE_SYSMODULE_ERROR_UNLOADED;
    if(id==SCE_SYSMODULE_NET && net_module_loaded) {
        /* Do not remove a live provider out from under guest users. */
        if(SceNet_is_initialized())result=(int)SCE_SYSMODULE_ERROR_FATAL;
        else {net_module_loaded=false;result=0;}
    }
    if(id==SCE_SYSMODULE_NP_TROPHY && trophy_module_loaded) {
        if(SceNpTrophy_is_initialized())result=(int)SCE_SYSMODULE_ERROR_FATAL;
        else {trophy_module_loaded=false;result=0;}
    }
    if(id==SCE_SYSMODULE_APPUTIL && apputil_module_loaded) {
        if(SceAppUtil_is_initialized())result=(int)SCE_SYSMODULE_ERROR_FATAL;
        else {apputil_module_loaded=false;result=0;}
    }
    if(stub_loaded[id]) {
        stub_loaded[id]=false;
        result=0;
    }
    mutexUnlock(&sysmodule_lock);
    return result;
}
bool SceSysmodule_stub_id_loaded(SceSysmoduleModuleId id)
{
    mutexLock(&sysmodule_lock);
    bool loaded = (unsigned)id <= (unsigned)SCE_SYSMODULE_JSON && stub_loaded[id];
    mutexUnlock(&sysmodule_lock);
    return loaded;
}

DECLARE_LIBRARY(SceSysmodule,0x03FCF19D);
