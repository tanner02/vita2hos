#include <switch.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "module.h"
#include "log.h"
#include "modules/SceAudiodec.h"
#include "libatrac9.h"
#include "structures.h"

/* Actual AT9 decoding only. Public declarations come directly from the SDK. */
_Static_assert(sizeof(SceAudiodecInfo) == 28, "Audiodec info ABI");
#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(SceAudiodecCtrl) == 40, "Audiodec control ABI");
#endif
/* LibAtrac9's bit reader has no size argument. Copy the admitted <=1KiB input
 * into owned zero-padded lookahead storage, then reject consumption beyond the
 * supplied superframe remainder before publishing PCM. Padding does not
 * replace the backend's internal array/index validation. */
enum { AUDIO_ES_SCRATCH = 8192+8, AUDIO_MAX_PCM_SAMPLES = 256*2 };
typedef struct {
    Atrac9Handle backend;
    unsigned char es[AUDIO_ES_SCRATCH];
    short pcm[AUDIO_MAX_PCM_SAMPLES];
} AudioContext;
typedef struct AudioDecoder {
    int32_t handle, internal_error;
    bool external, failed;
    SceUIntVAddr external_va;
    AudioContext *context;
    Atrac9CodecInfo info;
    uint32_t superframe_left, frame_index;
    unsigned char config[4];
    struct AudioDecoder *next;
} AudioDecoder;
static Mutex audio_lock;
static AudioDecoder *audio_decoders;
static bool audio_ready;
static uint32_t audio_channel_limit, audio_channels_used;
static uint64_t audio_next_handle = 1;
static unsigned audio_failure_logs;

static uint32_t context_bytes(void)
{ return ((uint32_t)sizeof(AudioContext)+255U)&~255U; }
static int codec_supported(uint32_t codec)
{
    if (codec == SCE_AUDIODEC_TYPE_AT9) return 0;
    if (codec >= SCE_AUDIODEC_TYPE_MP3 && codec <= SCE_AUDIODEC_TYPE_CELP)
        return (int)SCE_AUDIODEC_ERROR_UNSUPPORTED;
    return (int)SCE_AUDIODEC_ERROR_INVALID_TYPE;
}
static int valid_ctrl(const SceAudiodecCtrl *ctrl)
{
    if (!ctrl) return (int)SCE_AUDIODEC_ERROR_INVALID_PTR;
    return ctrl->size == sizeof(*ctrl) ? 0 : (int)SCE_AUDIODEC_ERROR_INVALID_SIZE;
}
/* Validate indexes BEFORE the backend uses ChannelConfigs[index] (six entries).
 * This provider admits only mono, dual mono and stereo (indexes0..2). */
static int config_input(const SceAudiodecCtrl *ctrl, unsigned char config[4])
{
    int result = valid_ctrl(ctrl);
    if (result) return result;
    if (ctrl->wordLength != SCE_AUDIODEC_WORD_LENGTH_16BITS)
        return (int)SCE_AUDIODEC_ERROR_INVALID_WORD_LENGTH;
    if (!ctrl->pInfo) return (int)SCE_AUDIODEC_ERROR_INVALID_PTR;
    if (ctrl->pInfo->size != sizeof(SceAudiodecInfoAt9))
        return (int)SCE_AUDIODEC_ERROR_INVALID_SIZE;
    memcpy(config, ctrl->pInfo->at9.configData, 4);
    uint32_t frame_bytes = (((uint32_t)config[2]<<3) | (config[3]>>5))+1;
    uint32_t frames = 1U << ((config[3]>>3)&3);
    if (config[0] != 0xfe || (config[1]&1) || ((config[1]>>1)&7)>2 ||
        frame_bytes*frames > SCE_AUDIODEC_AT9_MAX_ES_SIZE)
        return (int)SCE_AUDIODEC_AT9_ERROR_INVALID_CONFIG;
    return 0;
}
static AudioDecoder *find_decoder(int32_t handle)
{
    if (handle > 0) for (AudioDecoder *d = audio_decoders; d; d = d->next)
        if (d->handle == handle) return d;
    return NULL;
}
static bool span(const void *p, size_t bytes)
{ return p && (uintptr_t)p <= UINTPTR_MAX-bytes; }
static bool overlap(const void *a, size_t a_size, const void *b, size_t b_size)
{
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    return x <= y ? y-x < a_size : x-y < b_size;
}
static void backend_failure(AudioDecoder *d, int error)
{
    d->internal_error = error;
    d->failed = true;
    if (audio_failure_logs < 64) {
        ++audio_failure_logs;
        LOG("AUDIODEC_FAIL handle=%d internal=0x%08x", d->handle, (unsigned)error);
    }
}
static int initialize_backend(AudioDecoder *d)
{
    memset(d->context, 0, sizeof(*d->context));
    int error = Atrac9InitDecoder(&d->context->backend, d->config);
    if (!error) error = Atrac9GetCodecInfo(&d->context->backend, &d->info);
    if (error) { d->internal_error = error; d->failed = true; return (int)SCE_AUDIODEC_AT9_ERROR_INVALID_CONFIG; }
    const Atrac9CodecInfo *i = &d->info;
    if (i->channels < 1 || i->channels > 2 || i->frameSamples < 1 || i->frameSamples > 256 ||
        i->framesInSuperframe < 1 || i->framesInSuperframe > 8 || i->superframeSize < 1 ||
        i->superframeSize > SCE_AUDIODEC_AT9_MAX_ES_SIZE || i->samplingRate <= 0 || i->wlength != 16)
        return (int)SCE_AUDIODEC_AT9_ERROR_INVALID_CONFIG;
    d->superframe_left = (uint32_t)i->superframeSize;
    d->frame_index = 0;
    d->internal_error = 0;
    d->failed = false;
    return 0;
}
static void destroy_decoder(AudioDecoder *d)
{
    AudioDecoder **link = &audio_decoders;
    while (*link != d) link = &(*link)->next;
    *link = d->next;
    audio_channels_used -= (uint32_t)d->info.channels;
    if (!d->external) free(d->context);
    free(d);
}
void SceAudiodec_finish(void)
{
    mutexLock(&audio_lock);
    while (audio_decoders) destroy_decoder(audio_decoders);
    audio_ready = false;
    audio_channel_limit = 0;
    mutexUnlock(&audio_lock);
}

EXPORT(SceAudiodecUser,0x445C2CEF,int,sceAudiodecInitLibrary,uint32_t codec, SceAudiodecInitParam *params)
{
    int result = codec_supported(codec);
    if (result) return result;
    if (!params) return (int)SCE_AUDIODEC_ERROR_INVALID_PTR;
    if (params->size != sizeof(*params) || !params->at9.totalCh || params->at9.totalCh > 16)
        return (int)SCE_AUDIODEC_ERROR_INVALID_INIT_PARAM;
    mutexLock(&audio_lock);
    result = audio_ready ? (int)SCE_AUDIODEC_ERROR_ALREADY_INITIALIZED : 0;
    if (!result) { audio_ready = true; audio_channel_limit = params->at9.totalCh; }
    mutexUnlock(&audio_lock);
    return result;
}
EXPORT(SceAudiodecUser,0x45719B9D,int,sceAudiodecTermLibrary,uint32_t codec)
{
    int result = codec_supported(codec);
    if (result) return result;
    mutexLock(&audio_lock);
    result = !audio_ready ? (int)SCE_AUDIODEC_ERROR_NOT_INITIALIZED :
        audio_decoders ? (int)SCE_AUDIODEC_ERROR_A_HANDLE_IN_USE : 0;
    if (!result) { audio_ready = false; audio_channel_limit = 0; }
    mutexUnlock(&audio_lock);
    return result;
}
EXPORT(SceAudiodecUser,0xDB712ABC,int,sceAudiodecGetContextSize,SceAudiodecCtrl *ctrl, uint32_t codec)
{
    int result = codec_supported(codec);
    unsigned char config[4];
    if (!result) result = config_input(ctrl, config);
    return result ? result : (int)context_bytes();
}
static int create_decoder(SceAudiodecCtrl *ctrl, uint32_t codec, SceUIntVAddr va, uint32_t bytes, bool external)
{
    int result = codec_supported(codec);
    unsigned char config[4];
    if (!result) result = config_input(ctrl, config);
    if (result) return result;
    AudioContext *context = (AudioContext *)(uintptr_t)va;
    if (external && (!va || (va&255U) || bytes < context_bytes() ||
        (uint64_t)va+bytes > UINT64_C(0x100000000) ||
        overlap(context, context_bytes(), ctrl, sizeof(*ctrl)) ||
        overlap(context, context_bytes(), ctrl->pInfo, sizeof(*ctrl->pInfo))))
        return (int)SCE_AUDIODEC_ERROR_INVALID_SIZE;
    mutexLock(&audio_lock);
    if (!audio_ready) result = (int)SCE_AUDIODEC_ERROR_NOT_INITIALIZED;
    else if (ctrl->handle) result = find_decoder(ctrl->handle) ?
        (int)SCE_AUDIODEC_ERROR_A_HANDLE_IN_USE : (int)SCE_AUDIODEC_ERROR_INVALID_HANDLE;
    else if (audio_next_handle > INT32_MAX) result = (int)SCE_AUDIODEC_ERROR_ALL_HANDLES_IN_USE;
    else {
        uint32_t channels = ((config[1]>>1)&7) == 0 ? 1 : 2;
        if (channels > audio_channel_limit-audio_channels_used)
            result = (int)SCE_AUDIODEC_ERROR_CH_SHORTAGE;
    }
    if (!result && external) {
        for (AudioDecoder *live = audio_decoders; live; live = live->next) {
            if (overlap(context, context_bytes(), live->context, context_bytes())) {
                result = (int)SCE_AUDIODEC_ERROR_A_HANDLE_IN_USE;
                break;
            }
        }
    }
    AudioDecoder *d = NULL;
    if (!result) {
        d = calloc(1, sizeof(*d));
        if (!d) result = (int)SCE_AUDIODEC_ERROR_OUT_OF_MEMORY;
        else {
            d->external = external;
            d->external_va = va;
            d->context = external ? context : aligned_alloc(256, context_bytes());
            if (!d->context) result = (int)SCE_AUDIODEC_ERROR_OUT_OF_MEMORY;
            else { memcpy(d->config, config, 4); result = initialize_backend(d); }
        }
    }
    if (!result) {
        SceAudiodecInfoAt9 info = ctrl->pInfo->at9;
        info.ch = (uint32_t)d->info.channels;
        info.samplingRate = (uint32_t)d->info.samplingRate;
        info.superFrameSize = (uint32_t)d->info.superframeSize;
        info.framesInSuperFrame = (uint32_t)d->info.framesInSuperframe;
        info.bitRate = (uint32_t)((uint64_t)d->info.superframeSize*8*(uint32_t)d->info.samplingRate /
            ((uint32_t)d->info.frameSamples*(uint32_t)d->info.framesInSuperframe));
        d->handle = (int32_t)audio_next_handle++;
        d->next = audio_decoders;
        audio_decoders = d;
        audio_channels_used += info.ch;
        ctrl->pInfo->at9 = info;
        ctrl->handle = d->handle;
        ctrl->maxEsSize = (uint32_t)d->info.superframeSize;
        ctrl->maxPcmSize = (uint32_t)d->info.frameSamples*info.ch*sizeof(short);
        ctrl->inputEsSize = ctrl->outputPcmSize = 0;
    } else if (d) {
        if (!d->external) free(d->context);
        free(d);
    }
    mutexUnlock(&audio_lock);
    return result;
}
EXPORT(SceAudiodecUser,0x4DFD3AAA,int,sceAudiodecCreateDecoder,SceAudiodecCtrl *ctrl, uint32_t codec)
{ return create_decoder(ctrl, codec, 0, 0, false); }
EXPORT(SceAudiodecUser,0x56085DFB,int,sceAudiodecCreateDecoderExternal,SceAudiodecCtrl *ctrl,
       uint32_t codec, SceUIntVAddr context, uint32_t bytes)
{ return create_decoder(ctrl, codec, context, bytes, true); }
static int delete_decoder(SceAudiodecCtrl *ctrl, SceUIntVAddr *va, bool external)
{
    int result = valid_ctrl(ctrl);
    if (result) return result;
    if (external && !va) return (int)SCE_AUDIODEC_ERROR_INVALID_PTR;
    mutexLock(&audio_lock);
    AudioDecoder *d = find_decoder(ctrl->handle);
    result = !audio_ready ? (int)SCE_AUDIODEC_ERROR_NOT_INITIALIZED :
        !d || d->external != external ? (int)SCE_AUDIODEC_ERROR_INVALID_HANDLE : 0;
    if (!result) {
        SceUIntVAddr original = d->external_va;
        destroy_decoder(d);
        ctrl->handle = 0;
        ctrl->inputEsSize = ctrl->outputPcmSize = 0;
        if (external) *va = original;
    }
    mutexUnlock(&audio_lock);
    return result;
}
EXPORT(SceAudiodecUser,0xE7A24E16,int,sceAudiodecDeleteDecoder,SceAudiodecCtrl *ctrl)
{ return delete_decoder(ctrl, NULL, false); }
EXPORT(SceAudiodecUser,0xE4EA05BB,int,sceAudiodecDeleteDecoderExternal,SceAudiodecCtrl *ctrl, SceUIntVAddr *va)
{ return delete_decoder(ctrl, va, true); }

static int decode_frames(SceAudiodecCtrl *ctrl, uint32_t frames)
{
    int result = valid_ctrl(ctrl);
    if (result) return result;
    mutexLock(&audio_lock);
    AudioDecoder *d = find_decoder(ctrl->handle);
    if (!audio_ready) result = (int)SCE_AUDIODEC_ERROR_NOT_INITIALIZED;
    else if (!d) result = (int)SCE_AUDIODEC_ERROR_INVALID_HANDLE;
    else {
        ctrl->inputEsSize = ctrl->outputPcmSize = 0;
        uint32_t pcm_bytes = (uint32_t)d->info.frameSamples*(uint32_t)d->info.channels*sizeof(short);
        if (!frames || frames > SCE_AUDIODEC_AT9_MAX_NFRAMES) result = (int)SCE_AUDIODEC_ERROR_INVALID_NFRAMES;
        else if (ctrl->wordLength != 16) result = (int)SCE_AUDIODEC_ERROR_INVALID_WORD_LENGTH;
        else if (ctrl->maxEsSize < (uint32_t)d->info.superframeSize || ctrl->maxEsSize > SCE_AUDIODEC_AT9_MAX_ES_SIZE ||
            ctrl->maxPcmSize < pcm_bytes || ctrl->maxPcmSize > 256U*2*sizeof(short))
            result = (int)SCE_AUDIODEC_ERROR_INVALID_SIZE;
        else {
            size_t es_capacity = (size_t)ctrl->maxEsSize*frames, pcm_capacity = (size_t)ctrl->maxPcmSize*frames;
            if (!span(ctrl->pEs, es_capacity) || !span(ctrl->pPcm, pcm_capacity) ||
                overlap(ctrl->pEs, es_capacity, ctrl->pPcm, pcm_capacity) ||
                overlap(ctrl->pEs, es_capacity, d->context, context_bytes()) ||
                overlap(ctrl->pPcm, pcm_capacity, d->context, context_bytes()) ||
                overlap(ctrl->pEs, es_capacity, ctrl, sizeof(*ctrl)) ||
                overlap(ctrl->pPcm, pcm_capacity, ctrl, sizeof(*ctrl)))
                result = (int)SCE_AUDIODEC_ERROR_INVALID_PTR;
        }
        if (!result && d->failed) result = (int)SCE_AUDIODEC_ERROR_API_FAIL;
        for (uint32_t i = 0; !result && i < frames; ++i) {
            memset(d->context->es, 0, sizeof(d->context->es));
            memcpy(d->context->es, ctrl->pEs+ctrl->inputEsSize, d->superframe_left);
            int used = 0;
            int error = Atrac9Decode(&d->context->backend, d->context->es, d->context->pcm, &used);
            if (error || used <= 0 || (uint32_t)used > d->superframe_left) {
                backend_failure(d, error ? error : (int)SCE_AUDIODEC_ERROR_INVALID_SIZE);
                result = (int)SCE_AUDIODEC_ERROR_API_FAIL;
                break;
            }
            d->superframe_left -= (uint32_t)used;
            uint32_t consumed = (uint32_t)used;
            if (++d->frame_index == (uint32_t)d->info.framesInSuperframe) {
                consumed += d->superframe_left; /* superframe padding */
                d->superframe_left = (uint32_t)d->info.superframeSize;
                d->frame_index = 0;
            } else if (!d->superframe_left) {
                backend_failure(d, (int)SCE_AUDIODEC_ERROR_INVALID_SIZE);
                result = (int)SCE_AUDIODEC_ERROR_API_FAIL;
                break;
            }
            memcpy((unsigned char *)ctrl->pPcm+ctrl->outputPcmSize, d->context->pcm, pcm_bytes);
            ctrl->inputEsSize += consumed;
            ctrl->outputPcmSize += pcm_bytes;
            d->internal_error = 0;
        }
    }
    mutexUnlock(&audio_lock);
    return result;
}
EXPORT(SceAudiodecUser,0xCCDABA04,int,sceAudiodecDecode,SceAudiodecCtrl *ctrl)
{ return decode_frames(ctrl, 1); }
EXPORT(SceAudiodecUser,0x8018AA9B,int,sceAudiodecDecodeNFrames,SceAudiodecCtrl *ctrl, uint32_t frames)
{ return decode_frames(ctrl, frames); }
EXPORT(SceAudiodecUser,0xF72F9B64,int,sceAudiodecClearContext,SceAudiodecCtrl *ctrl)
{
    int result = valid_ctrl(ctrl);
    if (result) return result;
    mutexLock(&audio_lock);
    AudioDecoder *d = find_decoder(ctrl->handle);
    result = !audio_ready ? (int)SCE_AUDIODEC_ERROR_NOT_INITIALIZED :
        !d ? (int)SCE_AUDIODEC_ERROR_INVALID_HANDLE : initialize_backend(d);
    if (!result) ctrl->inputEsSize = ctrl->outputPcmSize = 0;
    mutexUnlock(&audio_lock);
    return result;
}
EXPORT(SceAudiodecUser,0x883B0CF5,int,sceAudiodecGetInternalError,SceAudiodecCtrl *ctrl, int32_t *error)
{
    int result = valid_ctrl(ctrl);
    if (result) return result;
    if (!error) return (int)SCE_AUDIODEC_ERROR_INVALID_PTR;
    mutexLock(&audio_lock);
    AudioDecoder *d = find_decoder(ctrl->handle);
    result = !audio_ready ? (int)SCE_AUDIODEC_ERROR_NOT_INITIALIZED :
        !d ? (int)SCE_AUDIODEC_ERROR_INVALID_HANDLE : 0;
    if (!result) *error = d->internal_error;
    mutexUnlock(&audio_lock);
    return result;
}
DECLARE_LIBRARY(SceAudiodecUser,0x2AA62046);
