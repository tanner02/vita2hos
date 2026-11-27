#include <switch.h>
#include "module.h"
#include "log.h"

/* Small system providers that LBP probes once: LiveArea status, BGM port
 * ownership, system events, motion sampling, power mode, shutter sound,
 * screenshots and the debug assertion hook. Library NIDs per vitasdk
 * db/360. */

/* --- SceLiveAreaUtil (0x5303CB07) --- */

EXPORT(SceLiveAreaUtil,0x7FE5B83F,int,sceLiveAreaGetStatus,void)
{
    return 0;
}

EXPORT(SceLiveAreaUtil,0xCFCDE95,int,sceLiveAreaUpdateFrameSync,void)
{
    return 0;
}

EXPORT(SceLiveAreaUtil,0xD330285D,int,sceLiveAreaUpdateFrameAsync,void)
{
    return 0;
}

/* --- SceAppMgr (0x8AF17416) --- */

EXPORT(SceAppMgr,0xAFCEAB96,int,sceAppMgrAcquireBgmPort,int len,int freq,unsigned int mode)
{
    LOG("sceAppMgrAcquireBgmPort len=%d freq=%d -> 0 (no BGM)",len,freq);
    return 0;
}

EXPORT(SceAppMgr,0xF3717E37,int,sceAppMgrReleaseBgmPort,void)
{
    return 0;
}

EXPORT(SceAppMgr,0xFFF8F7F0,int,sceAppMgrIsGameProgram,unsigned int pid)
{
    LOG("sceAppMgrIsGameProgram pid=%u -> 1",(unsigned)pid);
    return 1;
}

/* --- SceAppMgrUser (0xA6605D6F) --- */

EXPORT(SceAppMgrUser,0x10B5765F,int,sceAppMgrReceiveSystemEvent,void *event)
{
    /* No system events are delivered; a zeroed event reads as "none". */
    return 1;
}

EXPORT(SceAppMgrUser,0x5E86319A,int,_sceAppMgrGetAppState,void *state,unsigned int size,unsigned int flag)
{
    return -1;
}

/* --- SceMotion (0xDC571B3F) --- */

EXPORT(SceMotion,0x0FD2CDA2,int,sceMotionReset,void)
{
    return 0;
}

EXPORT(SceMotion,0x28034AC9,int,sceMotionStartSampling,void)
{
    LOG("sceMotionStartSampling (no motion sensor data)");
    return 0;
}

EXPORT(SceMotion,0xBDB32767,int,sceMotionGetState,void *state)
{
    /* Zeroed state: no rotation, no acceleration, samples at rest. */
    return 0;
}

/* --- ScePower (0x1082DA7F) --- */

EXPORT(ScePower,0x3CE187B6,int,scePowerSetConfigurationMode,unsigned int mode)
{
    LOG("scePowerSetConfigurationMode mode=0x%x",(unsigned)mode);
    return 0;
}

/* --- SceShutterSound (0x877181ED) --- */

EXPORT(SceShutterSound,0x7FFB6D79,int,sceShutterSoundPlay,unsigned int type)
{
    LOG("sceShutterSoundPlay type=%u (silent)",(unsigned)type);
    return 0;
}

/* --- SceScreenShot (0xF26FC97D) --- */

EXPORT(SceScreenShot,0x7061665B,int,sceScreenShotSetOverlayImage,unsigned int type,const void *path,unsigned int offset)
{
    LOG("sceScreenShotSetOverlayImage type=%u path=%s",type,path?(const char*)path:"(null)");
    return 0;
}

/* --- SceDbg (0x623DF986) --- */

/* Guest assertion handler: log and continue, never abort the process. */
EXPORT(SceDbg,0x1AF3678B,int,sceDbgAssertionHandler,unsigned int section,const void *file,int line,unsigned int type,const char *text)
{
    LOG("sceDbgAssertionHandler file=%s line=%d type=0x%x text=%s",
        file?(const char*)file:"(null)",line,(unsigned)type,text?text:"(null)");
    return 0;
}

DECLARE_LIBRARY(SceLiveAreaUtil,0x5303CB07);
DECLARE_LIBRARY(SceAppMgr,0x8AF17416);
DECLARE_LIBRARY(SceAppMgrUser,0xA6605D6F);
DECLARE_LIBRARY(SceMotion,0xDC571B3F);
DECLARE_LIBRARY(ScePower,0x1082DA7F);
DECLARE_LIBRARY(SceShutterSound,0x877181ED);
DECLARE_LIBRARY(SceScreenShot,0xF26FC97D);
DECLARE_LIBRARY(SceDbg,0x623DF986);
