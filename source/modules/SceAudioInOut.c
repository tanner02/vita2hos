#include <switch.h>
#include "module.h"
#include "log.h"

/* Audio port bring-up: LittleBigPlanet opens output ports for its NGS
 * mixer buses. Libraries SceAudio 0x438BB957 and SceAudioIn 0xF8DC61A3 per
 * vitasdk db/360. Output ports open and accept buffers that are dropped
 * (silence); input ports fail since no microphone exists. */

static Mutex audio_lock;
static int audio_next_out_port;

EXPORT(SceAudio,0x5BC341E4,int,sceAudioOutOpenPort,unsigned int portType,int len,int freq,unsigned int mode)
{
    mutexLock(&audio_lock);
    int id=++audio_next_out_port;
    mutexUnlock(&audio_lock);
    LOG("sceAudioOutOpenPort type=%u len=%d freq=%d mode=%u -> port=%d (silent)",
        portType,len,freq,mode,id);
    return id;
}

EXPORT(SceAudio,0x69E2E6B5,int,sceAudioOutReleasePort,int port)
{
    LOG("sceAudioOutReleasePort port=%d",port);
    return 0;
}

/* Buffers are accepted and dropped; zero means the buffer was consumed. */
EXPORT(SceAudio,0x02DB3F5F,int,sceAudioOutOutput,int port,const void *buf)
{
    return 0;
}

EXPORT(SceAudio,0x64167F11,int,sceAudioOutSetVolume,int port,unsigned int channels,unsigned int *volumes)
{
    return 0;
}

EXPORT(SceAudio,0x12FB1767,int,sceAudioOutGetAdopt,unsigned int portType,int len,int freq,unsigned int mode)
{
    return 1;
}

EXPORT(SceAudioIn,0x2F940377,int,sceAudioInGetStatus,unsigned int port)
{
    return 0;
}

EXPORT(SceAudioIn,0x566AC433,int,sceAudioInGetAdopt,void *param,int len,int freq,unsigned int mode)
{
    return 0;
}

EXPORT(SceAudioIn,0x39B50DC1,int,sceAudioInOpenPort,void *param,int len,int freq,unsigned int mode,unsigned int blocks,unsigned int channels)
{
    LOG("sceAudioInOpenPort len=%d freq=%d -> no microphone",len,freq);
    return -1;
}

EXPORT(SceAudioIn,0x3A61B8C4,int,sceAudioInReleasePort,int port)
{
    return 0;
}

EXPORT(SceAudioIn,0x638ADD2D,int,sceAudioInInput,int port,void *buffer)
{
    return -1;
}

DECLARE_LIBRARY(SceAudio,0x438BB957);
DECLARE_LIBRARY(SceAudioIn,0xF8DC61A3);
