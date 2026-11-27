#define TEST_THREADED_MUTEX
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
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
#include "../../source/modules/SceSysmodule.c"
#include "../../source/module.c"
bool SceNpTrophy_is_initialized(void){return false;}
bool SceAppUtil_is_initialized(void){return false;}
void log_printf(const char *format,...){(void)format;}
int main(void)
{
    /* Literal wire-order bytes give an oracle independent of the swap itself.
     * Exercise the exports before any host provider or guest network init. */
    unsigned short int (*htons_export)(unsigned short int) =
        (void *)module_get_func_export(0x6bf8b2a2,0x9fa3207b);
    unsigned int (*htonl_export)(unsigned int) =
        (void *)module_get_func_export(0x6bf8b2a2,0x4c30b03c);
    unsigned long long int (*htonll_export)(unsigned long long int) =
        (void *)module_get_func_export(0x6bf8b2a2,0x12c19209);
    assert(htons_export && htonl_export && htonll_export);
    unsigned short int wire16 = htons_export(0x1234);
    unsigned int wire32 = htonl_export(0x89abcdef);
    unsigned long long int wire64 = htonll_export(0xfedcba9876543210ull);
    assert(!memcmp(&wire16, "\x12\x34", 2));
    assert(!memcmp(&wire32, "\x89\xab\xcd\xef", 4));
    assert(!memcmp(&wire64, "\xfe\xdc\xba\x98\x76\x54\x32\x10", 8));
    assert(module_get_func_export(0x6bf8b2a2,0x07845128) == (void *)sceNetNtohs);
    assert(module_get_func_export(0x6bf8b2a2,0xd2eaa645) == (void *)sceNetNtohl);
    assert(module_get_func_export(0x6bf8b2a2,0xfb3336a6) == (void *)sceNetNtohll);
    assert(sceNetNtohs(wire16) == 0x1234 && sceNetNtohl(wire32) == 0x89abcdef);
    assert(sceNetNtohll(wire64) == 0xfedcba9876543210ull);
    assert(htons_export(0) == 0 && htons_export(0xffff) == 0xffff);
    assert(htonl_export(0) == 0 && htonl_export(0xffffffffu) == 0xffffffffu);
    assert(htonll_export(0) == 0 && htonll_export(~0ull) == ~0ull);
    assert(htonll_export(0x8000000000000001ull) == 0x0100000000000080ull);
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
    SceNet_finish();SceSysmodule_finish();assert(!net_initialized && !ctl_initialized && !net_module_loaded && host_ready && released==2);
    SceNet_finish();assert(released==2);
    /* Public module IDs extend beyond a 64-bit loaded-state mask. Each ID
     * must retain independent lifecycle state, including unsupported modules. */
    for(unsigned id=SCE_SYSMODULE_MONO;id<=SCE_SYSMODULE_JSON;++id) {
        SceSysmoduleModuleId module=(SceSysmoduleModuleId)id;
        assert(sceSysmoduleIsLoaded(module)==(int)SCE_SYSMODULE_ERROR_UNLOADED);
        const int low_state=sceSysmoduleIsLoaded(SCE_SYSMODULE_FIBER);
        int result=sceSysmoduleLoadModule(module);
        if(!result) {
            assert(!sceSysmoduleIsLoaded(module));
            assert(!sceSysmoduleUnloadModule(module));
        } else {
            assert(sceSysmoduleIsLoaded(module)==(int)SCE_SYSMODULE_ERROR_UNLOADED);
            assert(sceSysmoduleUnloadModule(module)==(int)SCE_SYSMODULE_ERROR_UNLOADED);
        }
        assert(sceSysmoduleIsLoaded(module)==(int)SCE_SYSMODULE_ERROR_UNLOADED);
        assert(sceSysmoduleIsLoaded(SCE_SYSMODULE_FIBER)==low_state);
    }
    assert(module_get_func_export(0x6bf8b2a2,0xeb03e265));
    assert(module_get_func_export(0xc6083ae1,0x495ca1db));
    assert(module_get_func_export(0x03fcf19d,0x79a0160a));
    puts("Network lifecycle: 16/32/64-bit wire order and NID dispatch, shared host provider, validation, retry, service ownership, observed state and teardown passed.");
}
