#include <switch.h>
#include <string.h>
#include "module.h"
#include "log.h"

/* NGSS audio bring-up: LittleBigPlanet initializes the NGS sound system for
 * its JobManager audio workers. Library NID 0xB01598D9 per vitasdk db/360.
 * No host audio output exists: system/rack/voice creation succeeds with
 * local ids and every playback operation is a silent no-op. */

static Mutex ngs_lock;
static int ngs_next_rack_id,ngs_next_voice_id,ngs_next_patch_id;

/* Voice definition getters hand back a pointer to zeroed template memory;
 * games read default parameter blocks from it. */
static uint8_t ngs_voice_def[0x100];

EXPORT(SceNgs,0xED14CF4A,int,sceNgsSystemInit,void *memBlock,unsigned int size,unsigned int flags,void **system)
{
    LOG("sceNgsSystemInit memBlock=%p size=0x%x flags=0x%x",memBlock,(unsigned)size,(unsigned)flags);
    if(system)*system=(void*)(uintptr_t)0x5E650001;
    return 0;
}

EXPORT(SceNgs,0x6CE8B36F,int,sceNgsSystemGetRequiredMemorySize,void **system,unsigned int *size)
{
    if(size)*size=0x4000;
    LOG("sceNgsSystemGetRequiredMemorySize -> 0x4000");
    return 0;
}

EXPORT(SceNgs,0x64D80013,int,sceNgsSystemSetFlags,void *system,unsigned int flags)
{
    LOG("sceNgsSystemSetFlags system=%p flags=0x%x",system,(unsigned)flags);
    return 0;
}

EXPORT(SceNgs,0x684F080C,int,sceNgsSystemUpdate,void *system)
{
    return 0;
}

EXPORT(SceNgs,0x4A25BEBC,int,sceNgsSystemRelease,void *system)
{
    LOG("sceNgsSystemRelease system=%p",system);
    return 0;
}

EXPORT(SceNgs,0x477318C0,int,sceNgsRackGetRequiredMemorySize,void *system,void *info,unsigned int *size)
{
    if(size)*size=0x800;
    LOG("sceNgsRackGetRequiredMemorySize system=%p info=%p -> 0x800",system,info);
    return 0;
}

EXPORT(SceNgs,0x0A92E4EC,int,sceNgsRackInit,void *system,void *memBlock,void *info,void **rack)
{
    mutexLock(&ngs_lock);
    int id=++ngs_next_rack_id;
    mutexUnlock(&ngs_lock);
    LOG("sceNgsRackInit system=%p info=%p -> rack=%d",system,info,id);
    if(rack)*rack=(void*)(uintptr_t)(0x5E650100+(unsigned)id);
    return 0;
}

EXPORT(SceNgs,0xDD5CA10B,int,sceNgsRackRelease,void *rack,unsigned int flags)
{
    LOG("sceNgsRackRelease rack=%p flags=0x%x",rack,(unsigned)flags);
    return 0;
}

EXPORT(SceNgs,0xFE1A98E9,int,sceNgsRackGetVoiceHandle,void *rack,unsigned int index,void **voice)
{
    mutexLock(&ngs_lock);
    int id=++ngs_next_voice_id;
    mutexUnlock(&ngs_lock);
    LOG("sceNgsRackGetVoiceHandle rack=%p index=%u -> voice=%d",rack,(unsigned)index,id);
    if(voice)*voice=(void*)(uintptr_t)(0x5E650200+(unsigned)id);
    return 0;
}

static void *ngs_voice_def_ptr(unsigned int kind)
{
    (void)kind;
    return ngs_voice_def;
}

EXPORT(SceNgs,0x0D5399CF,void *,sceNgsVoiceDefGetSimpleVoice,unsigned int kind)
{
    return ngs_voice_def_ptr(kind);
}

EXPORT(SceNgs,0x79A121D1,void *,sceNgsVoiceDefGetMasterBuss,unsigned int kind)
{
    return ngs_voice_def_ptr(kind);
}

EXPORT(SceNgs,0x9DCF50F5,void *,sceNgsVoiceDefGetReverbBuss,unsigned int kind)
{
    return ngs_voice_def_ptr(kind);
}

EXPORT(SceNgs,0xFA0A0F34,int,sceNgsVoicePlay,void *voice,void *handle)
{
    LOG("sceNgsVoicePlay voice=%p (silent)",voice);
    return 0;
}

EXPORT(SceNgs,0x0E291AAD,int,sceNgsVoiceKill,void *voice,void *handle)
{
    LOG("sceNgsVoiceKill voice=%p",voice);
    return 0;
}

EXPORT(SceNgs,0x54CFB981,int,sceNgsVoiceResume,void *voice,void *handle)
{
    return 0;
}

EXPORT(SceNgs,0xD7786E99,int,sceNgsVoicePause,void *voice,void *handle)
{
    return 0;
}

EXPORT(SceNgs,0x5551410D,int,sceNgsVoiceGetInfo,void *voice,void *handle,void *info)
{
    return 0;
}

EXPORT(SceNgs,0xC9B8C0B4,int,sceNgsVoiceGetStateData,void *voice,void *handle,unsigned int id,void *state,unsigned int size)
{
    if(state&&size)memset(state,0,size);
    return 0;
}

EXPORT(SceNgs,0xAB6BEF8F,int,sceNgsVoiceLockParams,void *voice,void *handle)
{
    return 0;
}

EXPORT(SceNgs,0x3D46D8A7,int,sceNgsVoiceUnlockParams,void *voice,void *handle)
{
    return 0;
}

EXPORT(SceNgs,0xFB8174B1,int,sceNgsVoiceSetParamsBlock,void *voice,void *handle,void *block,unsigned int size)
{
    return 0;
}

EXPORT(SceNgs,0x9AB87E71,int,sceNgsVoiceBypassModule,void *voice,void *handle,unsigned int module,unsigned int bypass)
{
    return 0;
}

EXPORT(SceNgs,0xD668B49C,int,sceNgsPatchCreateRouting,void *sourceVoice,void *sourceHandle,void *destVoice,void *destHandle,unsigned int rackParam,void **patch)
{
    mutexLock(&ngs_lock);
    int id=++ngs_next_patch_id;
    mutexUnlock(&ngs_lock);
    LOG("sceNgsPatchCreateRouting src=%p dst=%p -> patch=%d",sourceVoice,destVoice,id);
    if(patch)*patch=(void*)(uintptr_t)(0x5E650300+(unsigned)id);
    return 0;
}

EXPORT(SceNgs,0xD0C9AE5A,int,sceNgsPatchRemoveRouting,void *patch,unsigned int flags)
{
    LOG("sceNgsPatchRemoveRouting patch=%p",patch);
    return 0;
}

EXPORT(SceNgs,0xA0F5402D,int,sceNgsVoicePatchSetVolumesMatrix,void *handle,unsigned int param,void *matrix,unsigned int flags)
{
    return 0;
}

DECLARE_LIBRARY(SceNgs,0xB01598D9);
