#include <stdint.h>
#include <switch.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include "modules/SceNet.h"
#include "module.h"
#include "log.h"

/* Error values from the pinned SceNetCtl reference; older VitaSDK headers do
 * not name these codes. Native service errors are not Vita error numbers. */
#define CTL_NOT_INITIALIZED ((int)0x80412101)
#define CTL_NOT_TERMINATED ((int)0x80412102)
#define CTL_INVALID_ADDR ((int)0x80412107)
#define CTL_UNKNOWN_DEVICE ((int)0x80412188)
static Mutex net_lock;
static bool host_ready,net_initialized,ctl_initialized;

void SceNet_set_host_ready(bool ready)
{ mutexLock(&net_lock);host_ready=ready;mutexUnlock(&net_lock); }
bool SceNet_host_ready(void)
{mutexLock(&net_lock);bool ready=host_ready;mutexUnlock(&net_lock);return ready;}
bool SceNet_is_initialized(void)
{mutexLock(&net_lock);bool active=net_initialized || ctl_initialized;mutexUnlock(&net_lock);return active;}
void SceNet_finish(void)
{
    mutexLock(&net_lock);
    if(ctl_initialized)nifmExit();
    ctl_initialized=false;net_initialized=false;
    mutexUnlock(&net_lock);
}
EXPORT(SceNet,0xEB03E265,int,sceNetInit,SceNetInitParam *param)
{
    mutexLock(&net_lock);
    int result=0;
    if(net_initialized)result=(int)SCE_NET_ERROR_EBUSY;
    else if(!param || !param->memory || param->size<0x4000 || param->flags ||
            (uintptr_t)param->memory>UINTPTR_MAX-(unsigned)param->size)
        result=(int)SCE_NET_ERROR_EINVAL;
    else if(!host_ready)result=(int)SCE_NET_ERROR_ENODEV;
    else net_initialized=true;
    mutexUnlock(&net_lock);
    LOG("sceNetInit result=0x%x native_socket_device=borrowed",result);
    return result;
}
EXPORT(SceNet,0xEA3CC286,int,sceNetTerm,void)
{
    mutexLock(&net_lock);
    int result=net_initialized ? 0 : (int)SCE_NET_ERROR_ENOTINIT;
    net_initialized=false;
    mutexUnlock(&net_lock);
    return result;
}
EXPORT(SceNetCtl,0x495CA1DB,int,sceNetCtlInit,void)
{
    mutexLock(&net_lock);
    int result=0;
    if(ctl_initialized)result=CTL_NOT_TERMINATED;
    else {
        Result native=nifmInitialize(NifmServiceType_User);
        if(R_FAILED(native))result=CTL_UNKNOWN_DEVICE;
        else ctl_initialized=true;
    }
    mutexUnlock(&net_lock);
    LOG("sceNetCtlInit result=0x%x",result);
    return result;
}
EXPORT(SceNetCtl,0xCD188648,void,sceNetCtlTerm,void)
{
    mutexLock(&net_lock);
    if(ctl_initialized){nifmExit();ctl_initialized=false;}
    mutexUnlock(&net_lock);
}
EXPORT(SceNetCtl,0x6D26AC68,int,sceNetCtlInetGetState,int *state)
{
    mutexLock(&net_lock);
    int result=0;
    if(!ctl_initialized)result=CTL_NOT_INITIALIZED;
    else if(!state)result=CTL_INVALID_ADDR;
    else {
        NifmInternetConnectionType type;
        NifmInternetConnectionStatus status;
        uint32_t strength;
        Result native=nifmGetInternetConnectionStatus(&type,&strength,&status);
        if(R_FAILED(native))result=CTL_UNKNOWN_DEVICE;
        else if(status==NifmInternetConnectionStatus_Connected)*state=SCE_NETCTL_STATE_CONNECTED;
        else if((unsigned)status<=3)*state=SCE_NETCTL_STATE_CONNECTING;
        else result=CTL_UNKNOWN_DEVICE;
    }
    mutexUnlock(&net_lock);
    return result;
}
DECLARE_LIBRARY(SceNet,0x6BF8B2A2);
DECLARE_LIBRARY(SceNetCtl,0xC6083AE1);
