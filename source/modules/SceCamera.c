#include <switch.h>
#include "module.h"
#include "log.h"

/* Camera bring-up: LittleBigPlanet probes the camera for AR features.
 * Library NID 0xDA91B3ED per vitasdk db/360. No host camera exists: open
 * fails with the not-available error and the game disables camera paths. */

#define SCE_CAMERA_ERROR_NOT_OPEN 0x802B0000

EXPORT(SceCamera,0xA462F801,int,sceCameraOpen,unsigned int deviceHandle,void *param)
{
    LOG("sceCameraOpen device=%u -> not available",(unsigned)deviceHandle);
    return (int)SCE_CAMERA_ERROR_NOT_OPEN;
}

EXPORT(SceCamera,0xCD6E1CFC,int,sceCameraClose,unsigned int deviceHandle)
{
    return 0;
}

EXPORT(SceCamera,0xA8FEAE35,int,sceCameraStart,unsigned int deviceHandle)
{
    return (int)SCE_CAMERA_ERROR_NOT_OPEN;
}

EXPORT(SceCamera,0x1DD9C9CE,int,sceCameraStop,unsigned int deviceHandle)
{
    return 0;
}

EXPORT(SceCamera,0x79B5C2DE,int,sceCameraRead,unsigned int deviceHandle,void *param)
{
    return (int)SCE_CAMERA_ERROR_NOT_OPEN;
}

EXPORT(SceCamera,0x103A75B8,int,sceCameraIsActive,unsigned int deviceHandle)
{
    return 0;
}

EXPORT(SceCamera,0xE9D2CFB1,int,sceCameraSetEffect,unsigned int deviceHandle,unsigned int effect,void *param)
{
    return 0;
}

DECLARE_LIBRARY(SceCamera,0xDA91B3ED);
