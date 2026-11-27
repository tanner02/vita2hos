#define TEST_THREADED_MUTEX
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <switch.h>
typedef enum {NifmServiceType_User=0} NifmServiceType;
typedef enum {NifmInternetConnectionType_WiFi=1} NifmInternetConnectionType;
typedef enum {NifmInternetConnectionStatus_Connected=4} NifmInternetConnectionStatus;
static unsigned acquired,released,queries;
static Result init_failure,query_failure;
static unsigned native_status=4;
static Result nifmInitialize(NifmServiceType type){assert(type==NifmServiceType_User);acquired++;return init_failure;}
static void nifmExit(void){released++;}
static Result nifmGetInternetConnectionStatus(NifmInternetConnectionType *type,uint32_t *strength,NifmInternetConnectionStatus *status)
{queries++;*type=NifmInternetConnectionType_WiFi;*strength=3;*status=(NifmInternetConnectionStatus)native_status;return query_failure;}
#include "../../source/modules/SceNet.c"
#include "../../source/module.c"
void log_printf(const char *format,...){(void)format;}
int main(void)
{
    unsigned char pool[0x4000];SceNetInitParam param={pool,sizeof(pool),0};
    SceNet_finish();SceNet_set_host_ready(false);
    assert(sceNetInit(&param)==(int)SCE_NET_ERROR_ENODEV);
    assert(sceSysmoduleLoadModule(SCE_SYSMODULE_NET)==(int)SCE_SYSMODULE_ERROR_FATAL);
    assert(sceSysmoduleIsLoaded(SCE_SYSMODULE_NET)==(int)SCE_SYSMODULE_ERROR_UNLOADED);
    SceNet_set_host_ready(true);
    assert(sceNetInit(NULL)==(int)SCE_NET_ERROR_EINVAL);
    param.size=0x3fff;assert(sceNetInit(&param)==(int)SCE_NET_ERROR_EINVAL);param.size=sizeof(pool);
    param.flags=1;assert(sceNetInit(&param)==(int)SCE_NET_ERROR_EINVAL);param.flags=0;
    param.memory=NULL;assert(sceNetInit(&param)==(int)SCE_NET_ERROR_EINVAL);param.memory=pool;
    assert(!sceSysmoduleLoadModule(SCE_SYSMODULE_NET));
    assert(!sceSysmoduleLoadModule(SCE_SYSMODULE_NET));
    assert(!sceSysmoduleIsLoaded(SCE_SYSMODULE_NET));
    assert(sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP)==(int)SCE_SYSMODULE_ERROR_FATAL);
    assert(sceSysmoduleIsLoaded(SCE_SYSMODULE_HTTP)==(int)SCE_SYSMODULE_ERROR_UNLOADED);
    assert(sceSysmoduleLoadModule((SceSysmoduleModuleId)0)==(int)SCE_SYSMODULE_ERROR_INVALID_VALUE);
    assert(sceSysmoduleLoadModule((SceSysmoduleModuleId)-1)==(int)SCE_SYSMODULE_ERROR_INVALID_VALUE);
    assert(!sceNetInit(&param));assert(sceNetInit(NULL)==(int)SCE_NET_ERROR_EBUSY);
    assert(sceSysmoduleUnloadModule(SCE_SYSMODULE_NET)==(int)SCE_SYSMODULE_ERROR_FATAL);
    int state=99;
    assert(sceNetCtlInetGetState(&state)==CTL_NOT_INITIALIZED && state==99);
    init_failure=42;assert(sceNetCtlInit()==CTL_UNKNOWN_DEVICE && !ctl_initialized);
    assert(acquired==1 && !released);sceNetCtlTerm();assert(!released);
    init_failure=0;assert(!sceNetCtlInit());assert(sceNetCtlInit()==CTL_NOT_TERMINATED && acquired==2);
    assert(sceNetCtlInetGetState(NULL)==CTL_INVALID_ADDR && !queries);
    assert(!sceNetCtlInetGetState(&state) && state==SCE_NETCTL_STATE_CONNECTED);
    for(native_status=0;native_status<4;native_status++)assert(!sceNetCtlInetGetState(&state) && state==SCE_NETCTL_STATE_CONNECTING);
    state=77;query_failure=1;assert(sceNetCtlInetGetState(&state)==CTL_UNKNOWN_DEVICE && state==77);
    query_failure=0;native_status=5;assert(sceNetCtlInetGetState(&state)==CTL_UNKNOWN_DEVICE && state==77);
    assert(!sceNetTerm());assert(sceNetTerm()==(int)SCE_NET_ERROR_ENOTINIT);
    assert(sceSysmoduleUnloadModule(SCE_SYSMODULE_NET)==(int)SCE_SYSMODULE_ERROR_FATAL);
    sceNetCtlTerm();sceNetCtlTerm();assert(released==1);
    assert(!sceSysmoduleUnloadModule(SCE_SYSMODULE_NET));
    assert(sceSysmoduleUnloadModule(SCE_SYSMODULE_NET)==(int)SCE_SYSMODULE_ERROR_UNLOADED);
    assert(!sceNetInit(&param));assert(!sceNetCtlInit());assert(!sceSysmoduleLoadModule(SCE_SYSMODULE_NET));
    SceNet_finish();assert(!net_initialized && !ctl_initialized && !net_module_loaded && host_ready && released==2);
    SceNet_finish();assert(released==2);
    assert(module_get_func_export(0x6bf8b2a2,0xeb03e265));
    assert(module_get_func_export(0xc6083ae1,0x495ca1db));
    assert(module_get_func_export(0x03fcf19d,0x79a0160a));
    puts("Network lifecycle: shared host provider, validation, retry, service ownership, observed state and teardown passed.");
}
