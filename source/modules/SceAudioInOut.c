#include <switch.h>
#include <psp2/audioout.h>
#include <psp2/audioin.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include "module.h"
#include "log.h"
#include "modules/SceAudioInOut.h"

/* Audio port bring-up: LittleBigPlanet opens output ports for its NGS
 * mixer buses. Libraries SceAudio 0x438BB957 and SceAudioIn 0xF8DC61A3 per
 * vitasdk db/360. Output ports open and accept buffers that are dropped
 * (silence). Input is an emulated silent mono source with capture pacing. */

static Mutex audio_lock;
static int audio_next_port;
static struct {
    int id, grain;
    uint64_t duration, ready;
    bool busy;
} audio_input;
static struct { int id; uint64_t duration,ready; int volume[2]; } audio_ports[64];

void SceAudioInOut_finish(void)
{
    mutexLock(&audio_lock);
    for(unsigned i=0;i<64;i++)audio_ports[i].id=0;
    audio_input.id=0;
    audio_input.busy=false;
    mutexUnlock(&audio_lock);
}

static int audio_config(SceAudioOutPortType type,int len,int freq,SceAudioOutMode mode)
{
    if(type<SCE_AUDIO_OUT_PORT_TYPE_MAIN||type>SCE_AUDIO_OUT_PORT_TYPE_VOICE)
        return SCE_AUDIO_OUT_ERROR_INVALID_PORT_TYPE;
    if(len<SCE_AUDIO_MIN_LEN||len>SCE_AUDIO_MAX_LEN||len%64)
        return SCE_AUDIO_OUT_ERROR_INVALID_SIZE;
    if(mode!=SCE_AUDIO_OUT_MODE_MONO&&mode!=SCE_AUDIO_OUT_MODE_STEREO)
        return SCE_AUDIO_OUT_ERROR_INVALID_FORMAT;
    if(type==SCE_AUDIO_OUT_PORT_TYPE_MAIN&&freq!=48000)
        return SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ;
    switch(freq) {
    case 8000:case 11025:case 12000:case 16000:case 22050:
    case 24000:case 32000:case 44100:case 48000:return 0;
    default:return SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ;
    }
}

static unsigned audio_find(int id)
{
    for(unsigned i=0;i<64;i++)if(audio_ports[i].id==id&&id>0)return i;
    return 64;
}

EXPORT(SceAudio,0x5BC341E4,int,sceAudioOutOpenPort,SceAudioOutPortType portType,int len,int freq,SceAudioOutMode mode)
{
    int ret=audio_config(portType,len,freq,mode);
    if(ret)return ret;
    mutexLock(&audio_lock);
    unsigned slot=0;while(slot<64&&audio_ports[slot].id)slot++;
    if(slot==64||audio_next_port==INT_MAX) {
        mutexUnlock(&audio_lock);return SCE_AUDIO_OUT_ERROR_PORT_FULL;
    }
    int id=++audio_next_port;
    audio_ports[slot].id=id;
    audio_ports[slot].duration=((uint64_t)len*UINT64_C(1000000000)+(unsigned)freq-1)/(unsigned)freq;
    audio_ports[slot].ready=0;
    audio_ports[slot].volume[0]=audio_ports[slot].volume[1]=SCE_AUDIO_OUT_MAX_VOL;
    mutexUnlock(&audio_lock);
    LOG("sceAudioOutOpenPort type=%u len=%d freq=%d mode=%u -> port=%d (silent)",
        portType,len,freq,mode,id);
    return id;
}

EXPORT(SceAudio,0x69E2E6B5,int,sceAudioOutReleasePort,int port)
{
    mutexLock(&audio_lock);unsigned slot=audio_find(port);
    if(slot<64)audio_ports[slot].id=0;
    mutexUnlock(&audio_lock);
    return slot<64?0:(int)SCE_AUDIO_OUT_ERROR_INVALID_PORT;
}

/* One queued buffer per port. Independent ports can be fed together; later
 * submissions wait for the previous buffer's sample interval. Dropping PCM
 * must not turn the guest mixer into a busy loop. No lock is held while asleep. */
EXPORT(SceAudio,0x02DB3F5F,int,sceAudioOutOutput,int port,const void *buf)
{
    mutexLock(&audio_lock);unsigned slot=audio_find(port);
    if(slot==64){mutexUnlock(&audio_lock);return SCE_AUDIO_OUT_ERROR_INVALID_PORT;}
    uint64_t now=armTicksToNs(armGetSystemTick());
    uint64_t ready=audio_ports[slot].ready;
    uint64_t delay=ready>now?ready-now:0;
    if(buf)audio_ports[slot].ready=(ready>now?ready:now)+audio_ports[slot].duration;
    mutexUnlock(&audio_lock);
    if(delay)svcSleepThread((int64_t)delay);
    return 0;
}

EXPORT(SceAudio,0x64167F11,int,sceAudioOutSetVolume,int port,SceAudioOutChannelFlag channels,int *volumes)
{
    if(!volumes)return SCE_AUDIO_OUT_ERROR_INVALID_POINTER;
    if(!channels||((unsigned)channels&~3u))return SCE_AUDIO_OUT_ERROR_INVALID_VOLUME;
    for(unsigned i=0;i<2;i++)if(((unsigned)channels&(1u<<i))&&
        (volumes[i]<0||volumes[i]>SCE_AUDIO_OUT_MAX_VOL))return SCE_AUDIO_OUT_ERROR_INVALID_VOLUME;
    mutexLock(&audio_lock);unsigned slot=audio_find(port);
    if(slot<64)for(unsigned i=0;i<2;i++)if((unsigned)channels&(1u<<i))audio_ports[slot].volume[i]=volumes[i];
    mutexUnlock(&audio_lock);return slot<64?0:(int)SCE_AUDIO_OUT_ERROR_INVALID_PORT;
}

EXPORT(SceAudio,0x12FB1767,int,sceAudioOutGetAdopt,SceAudioOutPortType portType)
{
    return portType>=SCE_AUDIO_OUT_PORT_TYPE_MAIN&&portType<=SCE_AUDIO_OUT_PORT_TYPE_VOICE?1:(int)SCE_AUDIO_OUT_ERROR_INVALID_PORT_TYPE;
}

/* No physical recording: this virtual microphone is present and unmuted,
 * and returns silence at the requested sample cadence. */
EXPORT(SceAudioIn,0x2F940377,int,sceAudioInGetStatus,int select)
{
    return select==SCE_AUDIO_IN_GETSTATUS_MUTE ? 0 : (int)SCE_AUDIO_IN_ERROR_INVALID_PARAMETER;
}

EXPORT(SceAudioIn,0x566AC433,int,sceAudioInGetAdopt,SceAudioInPortType portType)
{
    return portType==SCE_AUDIO_IN_PORT_TYPE_VOICE||portType==SCE_AUDIO_IN_PORT_TYPE_RAW
        ? 1 : (int)SCE_AUDIO_IN_ERROR_INVALID_PORT_TYPE;
}

EXPORT(SceAudioIn,0x39B50DC1,int,sceAudioInOpenPort,SceAudioInPortType portType,int len,int freq,SceAudioInParam mode)
{
    if(portType!=SCE_AUDIO_IN_PORT_TYPE_VOICE&&portType!=SCE_AUDIO_IN_PORT_TYPE_RAW)
        return SCE_AUDIO_IN_ERROR_INVALID_PORT_TYPE;
    if(mode!=SCE_AUDIO_IN_PARAM_FORMAT_S16_MONO)
        return SCE_AUDIO_IN_ERROR_INVALID_PORT_PARAM;
    if(freq!=16000&&(portType!=SCE_AUDIO_IN_PORT_TYPE_RAW||freq!=48000))
        return SCE_AUDIO_IN_ERROR_INVALID_SAMPLE_FREQ;
    if((portType==SCE_AUDIO_IN_PORT_TYPE_VOICE&&len!=256&&len!=512)||
       (portType==SCE_AUDIO_IN_PORT_TYPE_RAW&&len!=(freq==16000?256:768)))
        return SCE_AUDIO_IN_ERROR_INVALID_PARAMETER;
    mutexLock(&audio_lock);
    if(audio_input.id||audio_next_port==INT_MAX) {
        mutexUnlock(&audio_lock);return SCE_AUDIO_IN_ERROR_PORT_FULL;
    }
    int id=++audio_next_port;
    audio_input.id=id;
    audio_input.grain=len;
    audio_input.duration=((uint64_t)len*UINT64_C(1000000000)+(unsigned)freq-1)/(unsigned)freq;
    audio_input.ready=armTicksToNs(armGetSystemTick())+audio_input.duration;
    audio_input.busy=false;
    mutexUnlock(&audio_lock);
    LOG("sceAudioInOpenPort type=%u len=%d freq=%d -> port=%d (silent)",portType,len,freq,id);
    return id;
}

EXPORT(SceAudioIn,0x3A61B8C4,int,sceAudioInReleasePort,int port)
{
    mutexLock(&audio_lock);
    int result=!audio_input.id?(int)SCE_AUDIO_IN_ERROR_NOT_OPENED:
        audio_input.id!=port?(int)SCE_AUDIO_IN_ERROR_INVALID_PORT:0;
    if(!result) { audio_input.id=0;audio_input.busy=false; }
    mutexUnlock(&audio_lock);
    return result;
}

EXPORT(SceAudioIn,0x638ADD2D,int,sceAudioInInput,int port,void *buffer)
{
    mutexLock(&audio_lock);
    int result=!audio_input.id?(int)SCE_AUDIO_IN_ERROR_NOT_OPENED:
        audio_input.id!=port?(int)SCE_AUDIO_IN_ERROR_INVALID_PORT:
        !buffer?(int)SCE_AUDIO_IN_ERROR_INVALID_POINTER:
        audio_input.busy?(int)SCE_AUDIO_IN_ERROR_BUSY:0;
    if(result) { mutexUnlock(&audio_lock);return result; }
    uint64_t now=armTicksToNs(armGetSystemTick());
    uint64_t ready=audio_input.ready>now?audio_input.ready:now;
    uint64_t delay=ready-now;
    size_t bytes=(size_t)audio_input.grain*sizeof(int16_t);
    audio_input.ready=ready+audio_input.duration;
    audio_input.busy=true;
    mutexUnlock(&audio_lock);
    /* Input waits for samples, including the first read. Revalidate after
     * sleep so closing/reopening a port never writes through an old request. */
    if(delay)svcSleepThread((int64_t)delay);
    mutexLock(&audio_lock);
    if(audio_input.id!=port) {
        mutexUnlock(&audio_lock);return SCE_AUDIO_IN_ERROR_NOT_OPENED;
    }
    memset(buffer,0,bytes);
    audio_input.busy=false;
    mutexUnlock(&audio_lock);
    return 0;
}

DECLARE_LIBRARY(SceAudio,0x438BB957);
DECLARE_LIBRARY(SceAudioIn,0xF8DC61A3);
