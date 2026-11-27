#include <switch.h>
#include <psp2/sysmodule.h>
#include "module.h"
#include "log.h"
#include "modules/SceNet.h"
#include "modules/SceNpTrophy.h"
#include "modules/SceSysmodule.h"
static Mutex sysmodule_lock;
static bool net_module_loaded,trophy_module_loaded;
void SceSysmodule_finish(void)
{
    mutexLock(&sysmodule_lock);net_module_loaded=false;trophy_module_loaded=false;mutexUnlock(&sysmodule_lock);
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
    mutexUnlock(&sysmodule_lock);
    LOG("sceSysmoduleLoadModule id=0x%x result=0x%x",(unsigned)id,result);
    return result;
}
EXPORT(SceSysmodule,0x53099B7A,int,sceSysmoduleIsLoaded,SceSysmoduleModuleId id)
{
    if(!valid_module(id))return (int)SCE_SYSMODULE_ERROR_INVALID_VALUE;
    mutexLock(&sysmodule_lock);
    int result=((id==SCE_SYSMODULE_NET && net_module_loaded) ||
                (id==SCE_SYSMODULE_NP_TROPHY && trophy_module_loaded)) ?
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
    mutexUnlock(&sysmodule_lock);
    return result;
}
DECLARE_LIBRARY(SceSysmodule,0x03FCF19D);
