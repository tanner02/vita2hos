#include <switch.h>
#include "module.h"
#include "log.h"

/* Codec engine bring-up: LittleBigPlanet decodes intro/logo video (H.264)
 * and audio streams. Three libraries in the SceAvcodecUser module per
 * vitasdk db/360. No host decoders exist: decoder creation succeeds with
 * local ids, decode calls report failure so the game skips streams. */

static Mutex codec_lock;
static int codec_next_decoder_id,codec_next_memblock_id;

/* --- SceAudiodecUser (0x2AA62046) --- */

EXPORT(SceAudiodecUser,0x445C2CEF,int,sceAudiodecInitLibrary,unsigned int codec,unsigned int codecSpecific,void *memBlock,unsigned int *size)
{
    LOG("sceAudiodecInitLibrary codec=0x%x memBlock=%p",(unsigned)codec,memBlock);
    return 0;
}

EXPORT(SceAudiodecUser,0x45719B9D,int,sceAudiodecTermLibrary,unsigned int codec)
{
    LOG("sceAudiodecTermLibrary codec=0x%x",(unsigned)codec);
    return 0;
}

EXPORT(SceAudiodecUser,0xDB712ABC,unsigned int,sceAudiodecGetContextSize,unsigned int codec,unsigned int codecSpecific)
{
    return 0x400;
}

EXPORT(SceAudiodecUser,0x4DFD3AAA,int,sceAudiodecCreateDecoder,void *context,void *info)
{
    mutexLock(&codec_lock);
    int id=++codec_next_decoder_id;
    mutexUnlock(&codec_lock);
    LOG("sceAudiodecCreateDecoder context=%p -> decoder=%d",context,id);
    return id;
}

EXPORT(SceAudiodecUser,0xE7A24E16,int,sceAudiodecDeleteDecoder,void *decoder)
{
    LOG("sceAudiodecDeleteDecoder decoder=%p",decoder);
    return 0;
}

EXPORT(SceAudiodecUser,0x56085DFB,int,sceAudiodecCreateDecoderExternal,void *context,void *info)
{
    mutexLock(&codec_lock);
    int id=++codec_next_decoder_id;
    mutexUnlock(&codec_lock);
    LOG("sceAudiodecCreateDecoderExternal context=%p -> decoder=%d",context,id);
    return id;
}

EXPORT(SceAudiodecUser,0xE4EA05BB,int,sceAudiodecDeleteDecoderExternal,void *decoder)
{
    LOG("sceAudiodecDeleteDecoderExternal decoder=%p",decoder);
    return 0;
}

/* Without a decoder backend the output stays as-is; callers treat zero
 * decoded frames as end-of-stream and stop pumping. */
EXPORT(SceAudiodecUser,0xCCDABA04,int,sceAudiodecDecode,void *decoder,void *source,void *destination)
{
    LOG("sceAudiodecDecode decoder=%p -> no frames",decoder);
    return 0;
}

EXPORT(SceAudiodecUser,0x8018AA9B,int,sceAudiodecDecodeNFrames,void *decoder,void *source,void *destination,unsigned int frames)
{
    LOG("sceAudiodecDecodeNFrames decoder=%p frames=%u -> no frames",decoder,(unsigned)frames);
    return 0;
}

EXPORT(SceAudiodecUser,0xF72F9B64,int,sceAudiodecClearContext,void *context)
{
    return 0;
}

EXPORT(SceAudiodecUser,0x883B0CF5,int,sceAudiodecGetInternalError,void *decoder,unsigned int *error)
{
    if(error)*error=0;
    return 0;
}

/* --- SceVideodecUser (0x163C3727) --- */

EXPORT(SceVideodecUser,0x230A78FF,int,sceVideodecInitLibraryWithUnmapMem,void *config,void *memBlock,unsigned int *size)
{
    LOG("sceVideodecInitLibraryWithUnmapMem config=%p memBlock=%p",config,memBlock);
    return 0;
}

EXPORT(SceVideodecUser,0x3A5F4924,int,sceVideodecTermLibrary,unsigned int codec)
{
    LOG("sceVideodecTermLibrary codec=0x%x",(unsigned)codec);
    return 0;
}

EXPORT(SceVideodecUser,0x6731BB2D,unsigned int,sceVideodecQueryMemSize,void *config)
{
    return 0x4000;
}

EXPORT(SceVideodecUser,0x97E95EDB,unsigned int,sceAvcdecQueryDecoderMemSize,void *config)
{
    return 0x4000;
}

EXPORT(SceVideodecUser,0xE82BB69B,int,sceAvcdecCreateDecoder,unsigned int codec,void *config,void *memBlock)
{
    mutexLock(&codec_lock);
    int id=++codec_next_decoder_id;
    mutexUnlock(&codec_lock);
    LOG("sceAvcdecCreateDecoder codec=0x%x config=%p -> decoder=%d",(unsigned)codec,config,id);
    return id;
}

EXPORT(SceVideodecUser,0x8A0E359E,int,sceAvcdecDeleteDecoder,unsigned int codec,void *decoder)
{
    LOG("sceAvcdecDeleteDecoder codec=0x%x decoder=%p",(unsigned)codec,decoder);
    return 0;
}

/* Video decode reports failure so LBP skips its intro/logo movies instead
 * of waiting on frames that can never arrive. */
EXPORT(SceVideodecUser,0xD6190A06,int,sceAvcdecDecode,unsigned int codec,void *decoder,void *info)
{
    LOG("sceAvcdecDecode codec=0x%x decoder=%p -> failure (skip)",(unsigned)codec,decoder);
    return -1;
}

EXPORT(SceVideodecUser,0x25F31020,int,sceAvcdecDecodeFlush,unsigned int codec,void *decoder,void *info)
{
    LOG("sceAvcdecDecodeFlush codec=0x%x -> failure",(unsigned)codec);
    return -1;
}

EXPORT(SceVideodecUser,0x9648D853,int,sceAvcdecDecodeStop,unsigned int codec,void *decoder,void *info)
{
    LOG("sceAvcdecDecodeStop codec=0x%x -> failure",(unsigned)codec);
    return -1;
}

EXPORT(SceVideodecUser,0x441673E3,unsigned int,sceAvcdecDecodeAvailableSize,unsigned int codec,void *decoder)
{
    return 0;
}

/* --- SceCodecEngineUser (0x469AB062) --- */

EXPORT(SceCodecEngineUser,0xF0B4C892,int,sceCodecEngineOpenUnmapMemBlock,void *base,unsigned int size,void *block)
{
    mutexLock(&codec_lock);
    int id=++codec_next_memblock_id;
    mutexUnlock(&codec_lock);
    LOG("sceCodecEngineOpenUnmapMemBlock base=%p size=0x%x -> block=%d",base,(unsigned)size,id);
    return id;
}

EXPORT(SceCodecEngineUser,0x95EA3B3E,int,sceCodecEngineCloseUnmapMemBlock,void *block)
{
    LOG("sceCodecEngineCloseUnmapMemBlock block=%p",block);
    return 0;
}

EXPORT(SceCodecEngineUser,0x1A3A53E5,void *,sceCodecEngineAllocMemoryFromUnmapMemBlock,void *block,unsigned int size,unsigned int flags)
{
    return NULL;
}

EXPORT(SceCodecEngineUser,0x6A31831D,int,sceCodecEngineFreeMemoryFromUnmapMemBlock,void *block,void *mem)
{
    return 0;
}

DECLARE_LIBRARY(SceAudiodecUser,0x2AA62046);
DECLARE_LIBRARY(SceVideodecUser,0x163C3727);
DECLARE_LIBRARY(SceCodecEngineUser,0x469AB062);
