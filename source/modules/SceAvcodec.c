#include <switch.h>
#include "module.h"
#include "log.h"

/* Video and codec-engine stubs. The typed audio decoder provider is in
 * SceAudiodec.c; these remaining interfaces have no decoder backend. */

static Mutex codec_lock;
static int codec_next_decoder_id,codec_next_memblock_id;

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

DECLARE_LIBRARY(SceVideodecUser,0x163C3727);
DECLARE_LIBRARY(SceCodecEngineUser,0x469AB062);
