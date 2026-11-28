#ifndef MODULES_SCE_NGS_H
#define MODULES_SCE_NGS_H

#include <stdint.h>

typedef uint32_t NgsHandle;
typedef struct NgsVoiceDefinition NgsVoiceDefinition;
typedef struct { void *data; uint32_t size; } NgsBufferInfo;
typedef struct {
    int32_t max_racks, max_voices, granularity, sample_rate, unk16;
} NgsSystemParams;
typedef struct {
    const NgsVoiceDefinition *definition;
    int32_t voice_count, channels_per_voice, max_patches_per_input, patches_per_output;
    void *unk14;
} NgsRackDescription;
typedef struct {
    uint32_t voice_state, num_modules, num_inputs, num_outputs;
    uint32_t num_patches_per_output, update_passed;
} NgsVoiceInfo;
typedef struct {
    NgsHandle source;
    int32_t source_output_index, source_output_subindex;
    NgsHandle dest;
    int32_t dest_input_index;
} NgsPatchSetup;
typedef struct { float matrix[2][2]; } NgsVolumeMatrix;
typedef struct { int32_t module_id, channel; } NgsModuleParamHeader;
typedef struct { uint32_t id, size; } NgsParamsDescriptor;
typedef struct {
    NgsHandle voice_handle, rack_handle;
    uint32_t module_id, callback_reason, callback_reason_2;
    void *callback_ptr, *userdata;
} NgsCallbackInfo;
typedef void (*NgsRackReleaseCallback)(const NgsCallbackInfo *info);

#define NGS_ERROR ((int)UINT32_C(0x804a0001))
#define NGS_INVALID_ARG ((int)UINT32_C(0x804a0002))
#define NGS_PARAM_OUT_OF_RANGE ((int)UINT32_C(0x804a0009))
#define NGS_INVALID_HANDLE ((int)UINT32_C(0x804a000c))
#define NGS_SIZE_MISMATCH ((int)UINT32_C(0x804a000d))
#define NGS_INVALID_STATE ((int)UINT32_C(0x804a0010))
#define NGS_UNSUPPORTED NGS_ERROR
#define NGS_VOICE_ACTIVE UINT32_C(1)
#define NGS_VOICE_PAUSED UINT32_C(32)

#ifdef __cplusplus
extern "C" {
#endif
int sceNgsSystemGetRequiredMemorySize(const NgsSystemParams *, uint32_t *);
int sceNgsSystemInit(void *, uint32_t, const NgsSystemParams *, NgsHandle *);
int sceNgsSystemSetFlags(NgsHandle, uint32_t);
int sceNgsSystemUpdate(NgsHandle);
int sceNgsSystemRelease(NgsHandle);
int sceNgsRackGetRequiredMemorySize(NgsHandle, const NgsRackDescription *, uint32_t *);
int sceNgsRackInit(NgsHandle, const NgsBufferInfo *, const NgsRackDescription *, NgsHandle *);
int sceNgsRackGetVoiceHandle(NgsHandle, uint32_t, NgsHandle *);
int sceNgsRackRelease(NgsHandle, NgsRackReleaseCallback);
const NgsVoiceDefinition *sceNgsVoiceDefGetSimpleVoice(void);
const NgsVoiceDefinition *sceNgsVoiceDefGetMasterBuss(void);
const NgsVoiceDefinition *sceNgsVoiceDefGetReverbBuss(void);
int sceNgsVoicePlay(NgsHandle);
int sceNgsVoiceKill(NgsHandle);
int sceNgsVoicePause(NgsHandle);
int sceNgsVoiceResume(NgsHandle);
int sceNgsVoiceGetInfo(NgsHandle, NgsVoiceInfo *);
int sceNgsVoiceGetStateData(NgsHandle, uint32_t, void *, uint32_t);
int sceNgsVoiceLockParams(NgsHandle, uint32_t, uint32_t, NgsBufferInfo *);
int sceNgsVoiceUnlockParams(NgsHandle, uint32_t);
int sceNgsVoiceSetParamsBlock(NgsHandle, const NgsModuleParamHeader *, uint32_t, int32_t *);
int sceNgsVoiceBypassModule(NgsHandle, uint32_t, uint32_t);
int sceNgsPatchCreateRouting(const NgsPatchSetup *, NgsHandle *);
int sceNgsPatchRemoveRouting(NgsHandle);
int sceNgsVoicePatchSetVolumesMatrix(NgsHandle, const NgsVolumeMatrix *);

/* Runtime shutdown only, after guest threads and updates have quiesced. */
void SceNgs_finish(void);
#ifdef __cplusplus
}
#endif
#endif
