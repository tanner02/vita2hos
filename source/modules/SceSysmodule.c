#include <switch.h>
#include <psp2/sysmodule.h>
#include "module.h"
#include "log.h"
#include "modules/SceNet.h"
#include "modules/SceAppUtil.h"
#include "modules/SceNpTrophy.h"
#include "modules/SceSysmodule.h"
static Mutex sysmodule_lock;
static bool net_module_loaded,trophy_module_loaded,apputil_module_loaded;
/* Modules that load as stubs: the guest sees a successful load, and its
 * imports resolve to HLE exports or missing-import stubs as usual.
 * LittleBigPlanet requests this set during boot. */
static uint64_t stub_loaded_mask;
static bool stub_loadable_module(SceSysmoduleModuleId id)
{
    switch(id) {
    case SCE_SYSMODULE_HTTPS:          /* 0x04 */
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
        return true;
    default:
        return false;
    }
}
void SceSysmodule_finish(void)
{
    mutexLock(&sysmodule_lock);net_module_loaded=false;trophy_module_loaded=false;apputil_module_loaded=false;stub_loaded_mask=0;mutexUnlock(&sysmodule_lock);
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
    if(stub_loadable_module(id)){stub_loaded_mask|=(uint64_t)1<<(unsigned)id;result=0;}
    mutexUnlock(&sysmodule_lock);
    LOG("sceSysmoduleLoadModule id=0x%x result=0x%x",(unsigned)id,result);
    return result;
}
EXPORT(SceSysmodule,0x53099B7A,int,sceSysmoduleIsLoaded,SceSysmoduleModuleId id)
{
    if(!valid_module(id))return (int)SCE_SYSMODULE_ERROR_INVALID_VALUE;
    mutexLock(&sysmodule_lock);
    int result=((id==SCE_SYSMODULE_NET && net_module_loaded) ||
                (id==SCE_SYSMODULE_NP_TROPHY && trophy_module_loaded) ||
                (id==SCE_SYSMODULE_APPUTIL && apputil_module_loaded) ||
                (stub_loaded_mask&((uint64_t)1<<(unsigned)id))) ?
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
    if(stub_loaded_mask&((uint64_t)1<<(unsigned)id)) {
        stub_loaded_mask&=~((uint64_t)1<<(unsigned)id);
        result=0;
    }
    mutexUnlock(&sysmodule_lock);
    return result;
}
DECLARE_LIBRARY(SceSysmodule,0x03FCF19D);
