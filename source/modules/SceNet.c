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

/* Both guest and host are little endian. These pure conversions are usable
 * before NetInit, without acquiring or borrowing any network service. Keep
 * the SDK's exact unsigned types, including the 64-bit r0/r1 return ABI. */
EXPORT(SceNet,0x4C30B03C,unsigned int,sceNetHtonl,unsigned int host32)
{return __builtin_bswap32(host32);}
EXPORT(SceNet,0x12C19209,unsigned long long int,sceNetHtonll,unsigned long long int host64)
{return __builtin_bswap64(host64);}
EXPORT(SceNet,0x9FA3207B,unsigned short int,sceNetHtons,unsigned short int host16)
{return __builtin_bswap16(host16);}
EXPORT(SceNet,0xD2EAA645,unsigned int,sceNetNtohl,unsigned int net32)
{return __builtin_bswap32(net32);}
EXPORT(SceNet,0xFB3336A6,unsigned long long int,sceNetNtohll,unsigned long long int net64)
{return __builtin_bswap64(net64);}
EXPORT(SceNet,0x07845128,unsigned short int,sceNetNtohs,unsigned short int net16)
{return __builtin_bswap16(net16);}

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
/* The inet callback is recorded but never fired: without a host network
 * event source the guest just observes a quiet, registered callback. */
EXPORT(SceNetCtl,0xEAEE6185,int,sceNetCtlInetRegisterCallback,SceNetCtlCallback func,void *arg,int *cid)
{
    mutexLock(&net_lock);
    int result=0;
    if(!ctl_initialized)result=CTL_NOT_INITIALIZED;
    else if(!func||!cid)result=CTL_INVALID_ADDR;
    else {
        *cid=0;
        LOG("sceNetCtlInetRegisterCallback callback=%p arg=%p",(void*)func,arg);
    }
    mutexUnlock(&net_lock);
    return result;
}
EXPORT(SceNetCtl,0xD0C3BF3F,int,sceNetCtlInetUnregisterCallback,int cid)
{
    mutexLock(&net_lock);
    int result=0;
    if(!ctl_initialized)result=CTL_NOT_INITIALIZED;
    else if(cid<0)result=CTL_INVALID_ADDR;
    else LOG("sceNetCtlInetUnregisterCallback cid=%d",cid);
    mutexUnlock(&net_lock);
    return result;
}

/* ZCode batch additions: NetCtl info getters. */
EXPORT(SceNetCtl,0x6B20EC02,int,sceNetCtlInetGetResult,int eventType,int *errorCode)
{
    LOG("sceNetCtlInetGetResult eventType=0x%x -> 0",eventType);
    if(errorCode)*errorCode=0;
    return 0;
}

EXPORT(SceNetCtl,0xB26D07F3,int,sceNetCtlInetGetInfo,int code,SceNetCtlInfo *info)
{
    LOG("sceNetCtlInetGetInfo code=0x%x -> unsupported",code);
    return (int)0x80412102;
}

EXPORT(SceNetCtl,0x4DDD6149,int,sceNetCtlGetNatInfo,SceNetCtlNatInfo *natinfo)
{
    return (int)0x80412102;
}

EXPORT(SceNetCtl,0xDFFC3ED4,int,sceNetCtlCheckCallback,void)
{
    return 0;
}

DECLARE_LIBRARY(SceNet,0x6BF8B2A2);
DECLARE_LIBRARY(SceNetCtl,0xC6083AE1);
