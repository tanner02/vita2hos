#include <switch.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "module.h"
#include "log.h"
#include "modules/SceNgs.h"

/* Stateful offline NGS contract. Handles and parameter storage are real owned
 * records; synthesis, sample decoding and asynchronous callbacks are unsupported.
 * Private definition tokens identify profiles, not a hardware definition ABI. */
_Static_assert(sizeof(NgsSystemParams) == 20, "NGS system parameters");
_Static_assert(sizeof(NgsVoiceInfo) == 24, "NGS voice info");
_Static_assert(sizeof(NgsVolumeMatrix) == 16, "NGS matrix");
_Static_assert(sizeof(NgsModuleParamHeader) == 8, "NGS module header");
_Static_assert(sizeof(NgsParamsDescriptor) == 8, "NGS parameter descriptor");
#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(NgsBufferInfo) == 8, "NGS buffer ABI");
_Static_assert(sizeof(NgsRackDescription) == 24, "NGS rack ABI");
_Static_assert(sizeof(NgsPatchSetup) == 20, "NGS patch ABI");
#endif

/* Bound diagnostics even when a guest retries an unsupported operation. */
static unsigned ngs_failure_reports;
static int ngs_result(const char *operation, int result)
{
    if (result && __atomic_load_n(&ngs_failure_reports, __ATOMIC_RELAXED) < 64 &&
        __atomic_fetch_add(&ngs_failure_reports, 1, __ATOMIC_RELAXED) < 64)
        LOG("NGS_API_FAIL operation=%s result=0x%x", operation, (unsigned)result);
    return result;
}

enum { NGS_SYSTEM_BYTES = 4096, NGS_MAX_SYSTEMS = 16,
       NGS_MAX_TOTAL_VOICES = 65536, NGS_MAX_TOTAL_RACKS = 4096,
       NGS_MAX_TOTAL_PATCHES = 65536, NGS_MAX_STATE_BYTES = 1024*1024,
       NGS_MAX_PARAM_BYTES = 1024*1024 };
typedef struct { uint32_t id, alternate, bytes, alternate_bytes; } NgsModuleSpec;
struct NgsVoiceDefinition {
    unsigned modules, outputs;
    NgsModuleSpec spec[6];
};
static const NgsVoiceDefinition simple_definition = {6, 2, {
    {0x01015ce6, 0, 84, 0}, {0x01015cec, 0x02015cec, 88, 88},
    {0x01015ce3, 0, 72, 0}, {0x01015ce5, 0, 16, 0},
    {0x01015ce4, 0x02015ce4, 28, 28}, {0x01015ce4, 0x02015ce4, 28, 28}}};
static const NgsVoiceDefinition master_definition = {2, 1, {{0}}};
static const NgsVoiceDefinition reverb_definition = {2, 1, {
    {0}, {0x01015ce7, 0x01025ce7, 76, 76}}};

typedef struct { unsigned char *data; bool locked, bypassed; } NgsModule;
typedef struct NgsSystem NgsSystem;
typedef struct NgsRack NgsRack;
typedef struct {
    NgsHandle id;
    NgsRack *rack;
    bool active, paused;
    uint32_t updates, progress[6];
    NgsModule module[6];
} NgsVoice;
typedef struct NgsPatch {
    NgsHandle id;
    NgsVoice *source, *dest;
    int32_t output, subindex, input;
    NgsVolumeMatrix matrix;
    struct NgsPatch *next;
} NgsPatch;
struct NgsRack {
    NgsHandle id;
    NgsSystem *system;
    NgsRackDescription description;
    NgsVoice *voices;
    NgsRack *next;
};
struct NgsSystem {
    NgsHandle id;
    NgsSystemParams params;
    uint32_t flags, racks_used, voices_used;
    NgsRack *racks;
    NgsPatch *patches;
    NgsSystem *next;
};
static Mutex ngs_lock;
static NgsSystem *ngs_systems;
static unsigned ngs_system_count, ngs_rack_count, ngs_voice_count, ngs_patch_count;
/* Never reset this sequence, including finish: old handles cannot alias new ones. */
static uint64_t ngs_next_handle = 1;

static NgsSystem *find_system(NgsHandle id)
{
    if (id) for (NgsSystem *s = ngs_systems; s; s = s->next)
        if (s->id == id) return s;
    return NULL;
}
static NgsRack *find_rack(NgsHandle id)
{
    if (id) for (NgsSystem *s = ngs_systems; s; s = s->next)
        for (NgsRack *r = s->racks; r; r = r->next)
            if (r->id == id) return r;
    return NULL;
}
static NgsVoice *find_voice(NgsHandle id)
{
    if (id) for (NgsSystem *s = ngs_systems; s; s = s->next)
        for (NgsRack *r = s->racks; r; r = r->next)
            for (int32_t i = 0; i < r->description.voice_count; ++i)
                if (r->voices[i].id == id) return &r->voices[i];
    return NULL;
}
static NgsPatch *find_patch(NgsHandle id)
{
    if (id) for (NgsSystem *s = ngs_systems; s; s = s->next)
        for (NgsPatch *p = s->patches; p; p = p->next)
            if (p->id == id) return p;
    return NULL;
}
static bool handles_available(uint32_t count)
{ return ngs_next_handle <= UINT32_MAX && count <= UINT64_C(1)+UINT32_MAX-ngs_next_handle; }
static NgsHandle take_handle(void) { return (NgsHandle)ngs_next_handle++; }
static bool known_definition(const NgsVoiceDefinition *d)
{ return d == &simple_definition || d == &master_definition || d == &reverb_definition; }
static bool valid_params(const NgsSystemParams *p)
{
    return p && p->max_racks >= 1 && p->max_racks <= 256 &&
        p->max_voices >= 1 && p->max_voices <= 65536 &&
        p->granularity >= 1 && p->granularity <= 4096 &&
        p->sample_rate >= 1 && p->sample_rate <= 192000;
}
static int rack_size(NgsSystem *s, const NgsRackDescription *d, uint32_t *size)
{
    if (!d || !known_definition(d->definition) || d->voice_count <= 0 ||
        d->channels_per_voice < 1 || d->channels_per_voice > 2 ||
        d->max_patches_per_input < 0 || d->max_patches_per_input > 64 ||
        d->patches_per_output < 0 || d->patches_per_output > 64) return ngs_result(__func__, NGS_INVALID_ARG);
    if (s->racks_used >= (uint32_t)s->params.max_racks ||
        (uint32_t)d->voice_count > (uint32_t)s->params.max_voices-s->voices_used)
        return ngs_result(__func__, NGS_PARAM_OUT_OF_RANGE);
    *size = 64 + (uint32_t)d->voice_count*64;
    return 0;
}
static bool rack_borrowed(const NgsRack *r)
{
    for (int32_t i = 0; i < r->description.voice_count; ++i)
        for (unsigned m = 0; m < r->description.definition->modules; ++m)
            if (r->voices[i].module[m].locked) return true;
    return false;
}
static void remove_patch(NgsSystem *s, NgsPatch *patch)
{
    NgsPatch **link = &s->patches;
    while (*link != patch) link = &(*link)->next;
    *link = patch->next;
    free(patch);
    --ngs_patch_count;
}
static void release_rack(NgsRack *r)
{
    NgsSystem *s = r->system;
    NgsPatch *p = s->patches;
    while (p) {
        NgsPatch *next = p->next;
        if (p->source->rack == r || p->dest->rack == r) remove_patch(s, p);
        p = next;
    }
    for (int32_t i = 0; i < r->description.voice_count; ++i)
        for (unsigned m = 0; m < r->description.definition->modules; ++m)
            free(r->voices[i].module[m].data);
    NgsRack **link = &s->racks;
    while (*link != r) link = &(*link)->next;
    *link = r->next;
    s->voices_used -= (uint32_t)r->description.voice_count;
    ngs_voice_count -= (unsigned)r->description.voice_count;
    --s->racks_used;
    --ngs_rack_count;
    free(r->voices);
    free(r);
}
static void release_system(NgsSystem *s)
{
    while (s->racks) release_rack(s->racks);
    NgsSystem **link = &ngs_systems;
    while (*link != s) link = &(*link)->next;
    *link = s->next;
    --ngs_system_count;
    free(s);
}
void SceNgs_finish(void)
{
    mutexLock(&ngs_lock);
    while (ngs_systems) release_system(ngs_systems);
    mutexUnlock(&ngs_lock);
}

EXPORT(SceNgs,0x6CE8B36F,int,sceNgsSystemGetRequiredMemorySize,
       const NgsSystemParams *params, uint32_t *size)
{
    if (!valid_params(params) || !size) return ngs_result(__func__, NGS_INVALID_ARG);
    *size = NGS_SYSTEM_BYTES;
    return 0;
}
EXPORT(SceNgs,0xED14CF4A,int,sceNgsSystemInit,void *buffer, uint32_t bytes,
       const NgsSystemParams *params, NgsHandle *output)
{
    if (!buffer || !output || !valid_params(params)) return ngs_result(__func__, NGS_INVALID_ARG);
    if (bytes < NGS_SYSTEM_BYTES) return ngs_result(__func__, NGS_SIZE_MISMATCH);
    mutexLock(&ngs_lock);
    int result = NGS_ERROR;
    if (ngs_system_count < NGS_MAX_SYSTEMS && handles_available(1)) {
        NgsSystem *s = calloc(1, sizeof(*s));
        if (s) {
            s->params = *params;
            s->id = take_handle();
            s->next = ngs_systems;
            ngs_systems = s;
            ++ngs_system_count;
            *output = s->id;
            result = 0;
        }
    }
    mutexUnlock(&ngs_lock);
    return ngs_result(__func__, result);
}
EXPORT(SceNgs,0x64D80013,int,sceNgsSystemSetFlags,NgsHandle id, uint32_t flags)
{
    mutexLock(&ngs_lock);
    NgsSystem *s = find_system(id);
    if (s) s->flags = flags;
    int result = s ? 0 : NGS_INVALID_HANDLE;
    mutexUnlock(&ngs_lock);
    return ngs_result(__func__, result);
}
EXPORT(SceNgs,0x684F080C,int,sceNgsSystemUpdate,NgsHandle id)
{
    mutexLock(&ngs_lock);
    NgsSystem *s = find_system(id);
    if (s) for (NgsRack *r = s->racks; r; r = r->next) {
        for (int32_t i = 0; i < r->description.voice_count; ++i) {
            NgsVoice *v = &r->voices[i];
            if (!v->active || v->paused) continue;
            ++v->updates;
            /* Synthetic progress only: no sample payload is read or decoded. */
            uint32_t samples = (uint32_t)s->params.granularity * (uint32_t)r->description.channels_per_voice;
            v->progress[0] += samples*2;
            v->progress[2] += samples;
            v->progress[3] += samples*2;
            v->progress[4] += samples;
            v->progress[5] += samples*2;
        }
    }
    int result = s ? 0 : NGS_INVALID_HANDLE;
    mutexUnlock(&ngs_lock);
    return ngs_result(__func__, result);
}
EXPORT(SceNgs,0x4A25BEBC,int,sceNgsSystemRelease,NgsHandle id)
{
    mutexLock(&ngs_lock);
    NgsSystem *s = find_system(id);
    int result = s ? 0 : NGS_INVALID_HANDLE;
    if (s) for (NgsRack *r = s->racks; r; r = r->next)
        if (rack_borrowed(r)) result = NGS_INVALID_STATE;
    if (!result) release_system(s);
    mutexUnlock(&ngs_lock);
    return ngs_result(__func__, result);
}
EXPORT(SceNgs,0x477318C0,int,sceNgsRackGetRequiredMemorySize,NgsHandle id,
       const NgsRackDescription *description, uint32_t *output)
{
    if (!output) return ngs_result(__func__, NGS_INVALID_ARG);
    mutexLock(&ngs_lock);
    NgsSystem *s = find_system(id);
    uint32_t size = 0;
    int result = s ? rack_size(s, description, &size) : NGS_INVALID_HANDLE;
    if (!result) *output = size;
    mutexUnlock(&ngs_lock);
    return ngs_result(__func__, result);
}
EXPORT(SceNgs,0x0A92E4EC,int,sceNgsRackInit,NgsHandle id, const NgsBufferInfo *memory,
       const NgsRackDescription *description, NgsHandle *output)
{
    if (!memory || !memory->data || !output) return ngs_result(__func__, NGS_INVALID_ARG);
    mutexLock(&ngs_lock);
    NgsSystem *s = find_system(id);
    uint32_t size = 0;
    int result = s ? rack_size(s, description, &size) : NGS_INVALID_HANDLE;
    if (!result && memory->size < size) result = NGS_SIZE_MISMATCH;
    if (!result && (ngs_rack_count >= NGS_MAX_TOTAL_RACKS ||
        (uint32_t)description->voice_count > NGS_MAX_TOTAL_VOICES-ngs_voice_count ||
        !handles_available(1+(uint32_t)description->voice_count))) result = NGS_ERROR;
    if (!result) {
        NgsRack *r = calloc(1, sizeof(*r));
        NgsVoice *voices = calloc((size_t)description->voice_count, sizeof(*voices));
        if (!r || !voices) {
            free(r);
            free(voices);
            result = NGS_ERROR;
        } else {
            r->id = take_handle();
            r->system = s;
            r->description = *description;
            r->voices = voices;
            for (int32_t i = 0; i < description->voice_count; ++i) {
                voices[i].id = take_handle();
                voices[i].rack = r;
            }
            r->next = s->racks;
            s->racks = r;
            ++s->racks_used;
            ++ngs_rack_count;
            s->voices_used += (uint32_t)description->voice_count;
            ngs_voice_count += (unsigned)description->voice_count;
            *output = r->id;
        }
    }
    mutexUnlock(&ngs_lock);
    return ngs_result(__func__, result);
}
EXPORT(SceNgs,0xFE1A98E9,int,sceNgsRackGetVoiceHandle,NgsHandle id, uint32_t index, NgsHandle *output)
{
    if (!output) return ngs_result(__func__, NGS_INVALID_ARG);
    mutexLock(&ngs_lock);
    NgsRack *r = find_rack(id);
    int result = !r ? NGS_INVALID_HANDLE : index >= (uint32_t)r->description.voice_count ? NGS_INVALID_ARG : 0;
    if (!result) *output = r->voices[index].id;
    mutexUnlock(&ngs_lock);
    return ngs_result(__func__, result);
}
EXPORT(SceNgs,0xDD5CA10B,int,sceNgsRackRelease,NgsHandle id, NgsRackReleaseCallback callback)
{
    mutexLock(&ngs_lock);
    NgsRack *r = find_rack(id);
    int result = !r ? NGS_INVALID_HANDLE : callback ? NGS_UNSUPPORTED : rack_borrowed(r) ? NGS_INVALID_STATE : 0;
    if (!result) release_rack(r);
    mutexUnlock(&ngs_lock);
    return ngs_result(__func__, result);
}
EXPORT(SceNgs,0x0D5399CF,const NgsVoiceDefinition *,sceNgsVoiceDefGetSimpleVoice,void)
{ return &simple_definition; }
EXPORT(SceNgs,0x79A121D1,const NgsVoiceDefinition *,sceNgsVoiceDefGetMasterBuss,void)
{ return &master_definition; }
EXPORT(SceNgs,0x9DCF50F5,const NgsVoiceDefinition *,sceNgsVoiceDefGetReverbBuss,void)
{ return &reverb_definition; }

static int voice_action(NgsHandle id, unsigned action)
{
    mutexLock(&ngs_lock);
    NgsVoice *v = find_voice(id);
    int result = v ? 0 : NGS_INVALID_HANDLE;
    if (v) {
        if (action == 0) {
            if (v->active) result = NGS_INVALID_STATE;
            else {
                v->active = true;
                v->paused = false;
                /* Default synthetic positions only. Configured Player start
                 * positions, looping and real PCM consumption are not modeled. */
                memset(v->progress, 0, 4*sizeof(v->progress[0]));
            }
        } else if (action == 1) { v->active = false; v->paused = false; }
        else if (!v->active || (action == 2 && v->paused) || (action == 3 && !v->paused))
            result = NGS_INVALID_STATE;
        else v->paused = action == 2;
    }
    mutexUnlock(&ngs_lock);
    return ngs_result(__func__, result);
}
EXPORT(SceNgs,0xFA0A0F34,int,sceNgsVoicePlay,NgsHandle id) { return voice_action(id, 0); }
EXPORT(SceNgs,0x0E291AAD,int,sceNgsVoiceKill,NgsHandle id) { return voice_action(id, 1); }
EXPORT(SceNgs,0xD7786E99,int,sceNgsVoicePause,NgsHandle id) { return voice_action(id, 2); }
EXPORT(SceNgs,0x54CFB981,int,sceNgsVoiceResume,NgsHandle id) { return voice_action(id, 3); }
EXPORT(SceNgs,0x5551410D,int,sceNgsVoiceGetInfo,NgsHandle id, NgsVoiceInfo *output)
{
    if (!output) return ngs_result(__func__, NGS_INVALID_ARG);
    mutexLock(&ngs_lock);
    NgsVoice *v = find_voice(id);
    if (v) {
        const NgsRackDescription *d = &v->rack->description;
        *output = (NgsVoiceInfo){(v->active ? NGS_VOICE_ACTIVE : 0) | (v->paused ? NGS_VOICE_PAUSED : 0),
            d->definition->modules, 1, d->definition->outputs,
            (uint32_t)d->patches_per_output, v->updates};
    }
    int result = v ? 0 : NGS_INVALID_HANDLE;
    mutexUnlock(&ngs_lock);
    return ngs_result(__func__, result);
}
EXPORT(SceNgs,0xC9B8C0B4,int,sceNgsVoiceGetStateData,NgsHandle id, uint32_t module, void *output, uint32_t bytes)
{
    if ((!output && bytes) || bytes > NGS_MAX_STATE_BYTES) return ngs_result(__func__, NGS_INVALID_ARG);
    mutexLock(&ngs_lock);
    NgsVoice *v = find_voice(id);
    int result = !v ? NGS_INVALID_HANDLE : module >= v->rack->description.definition->modules ? NGS_INVALID_ARG : 0;
    if (!result && bytes) {
        memset(output, 0, bytes);
        if (v->rack->description.definition == &simple_definition && module == 0) {
            size_t copied = bytes < sizeof(v->progress) ? bytes : sizeof(v->progress);
            memcpy(output, v->progress, copied);
        }
        /* Master module1 is silent stereo S16 (granularity*4 bytes). Other
         * supported modules expose zero state, including Envelope's20byte prefix. */
    }
    mutexUnlock(&ngs_lock);
    return ngs_result(__func__, result);
}

static uint32_t interface_size(const NgsModuleSpec *spec, uint32_t interface)
{
    if (interface == spec->id && spec->bytes) {
        if (interface == UINT32_C(0x01015cec)) return 72;
        if (interface == UINT32_C(0x01015ce4)) return 24;
        return spec->bytes;
    }
    return spec->alternate && interface == spec->alternate ? spec->alternate_bytes : 0;
}
EXPORT(SceNgs,0xAB6BEF8F,int,sceNgsVoiceLockParams,NgsHandle id, uint32_t module,
       uint32_t interface, NgsBufferInfo *output)
{
    if (!output) return ngs_result(__func__, NGS_INVALID_ARG);
    mutexLock(&ngs_lock);
    NgsVoice *v = find_voice(id);
    int result = !v ? NGS_INVALID_HANDLE : module >= v->rack->description.definition->modules ? NGS_INVALID_ARG : 0;
    if (!result) {
        const NgsModuleSpec *spec = &v->rack->description.definition->spec[module];
        NgsModule *m = &v->module[module];
        uint32_t size = interface_size(spec, interface);
        if (!size) result = NGS_INVALID_ARG;
        else if (m->locked) result = NGS_INVALID_STATE;
        else {
            if (!m->data) m->data = calloc(1, spec->bytes);
            if (!m->data) result = NGS_ERROR;
            else {
                NgsParamsDescriptor descriptor = {interface, size};
                memcpy(m->data, &descriptor, sizeof(descriptor));
                m->locked = true;
                *output = (NgsBufferInfo){m->data, spec->bytes};
            }
        }
    }
    mutexUnlock(&ngs_lock);
    return ngs_result(__func__, result);
}
EXPORT(SceNgs,0x3D46D8A7,int,sceNgsVoiceUnlockParams,NgsHandle id, uint32_t module)
{
    mutexLock(&ngs_lock);
    NgsVoice *v = find_voice(id);
    int result = !v ? NGS_INVALID_HANDLE : module >= v->rack->description.definition->modules ? NGS_INVALID_ARG : 0;
    if (!result) {
        if (!v->module[module].locked) result = NGS_INVALID_STATE;
        else v->module[module].locked = false;
    }
    mutexUnlock(&ngs_lock);
    return ngs_result(__func__, result);
}
EXPORT(SceNgs,0xFB8174B1,int,sceNgsVoiceSetParamsBlock,NgsHandle id,
       const NgsModuleParamHeader *records, uint32_t bytes, int32_t *errors)
{
    if ((!records && bytes) || bytes > NGS_MAX_PARAM_BYTES ||
        (uintptr_t)records > UINTPTR_MAX-bytes) {
        if (errors) *errors = 1;
        return ngs_result(__func__, NGS_INVALID_ARG);
    }
    mutexLock(&ngs_lock);
    NgsVoice *v = find_voice(id);
    int result = v ? 0 : NGS_INVALID_HANDLE;
    uint32_t modules = 0;
    /* A caller can build records inside a previously borrowed parameter
     * allocation. Publishing one record must not change any later header. */
    unsigned char *data = NULL;
    if (!result && bytes) {
        data = malloc(bytes);
        if (!data) result = NGS_ERROR;
        else memcpy(data, records, bytes);
    }
    for (size_t used = 0; !result && used < bytes;) {
        NgsModuleParamHeader header;
        NgsParamsDescriptor descriptor;
        if (bytes-used < sizeof(header)+sizeof(descriptor)) { result = NGS_INVALID_ARG; break; }
        memcpy(&header, data+used, sizeof(header));
        memcpy(&descriptor, data+used+sizeof(header), sizeof(descriptor));
        if (header.module_id < 0 || (unsigned)header.module_id >= v->rack->description.definition->modules ||
            header.channel < -1 || header.channel >= v->rack->description.channels_per_voice ||
            descriptor.size < sizeof(descriptor) || descriptor.size > bytes-used-sizeof(header)) {
            result = NGS_INVALID_ARG; break;
        }
        const NgsModuleSpec *spec = &v->rack->description.definition->spec[header.module_id];
        uint32_t size = interface_size(spec, descriptor.id);
        if (!size || descriptor.size > size) { result = NGS_SIZE_MISMATCH; break; }
        if (v->module[header.module_id].locked) { result = NGS_INVALID_STATE; break; }
        modules |= UINT32_C(1) << header.module_id;
        used += sizeof(header)+descriptor.size;
    }
    unsigned char *pending[6] = {0};
    if (!result) for (unsigned i = 0; i < v->rack->description.definition->modules; ++i) {
        if (!(modules & (UINT32_C(1)<<i)) || v->module[i].data) continue;
        pending[i] = calloc(1, v->rack->description.definition->spec[i].bytes);
        if (!pending[i]) { result = NGS_ERROR; break; }
    }
    if (!result) {
        for (unsigned i = 0; i < 6; ++i) if (pending[i]) v->module[i].data = pending[i];
        for (size_t used = 0; used < bytes;) {
            NgsModuleParamHeader header;
            NgsParamsDescriptor descriptor;
            memcpy(&header, data+used, sizeof(header));
            memcpy(&descriptor, data+used+sizeof(header), sizeof(descriptor));
            memmove(v->module[header.module_id].data, data+used+sizeof(header), descriptor.size);
            used += sizeof(header)+descriptor.size;
        }
    } else for (unsigned i = 0; i < 6; ++i) free(pending[i]);
    if (errors) *errors = result ? 1 : 0;
    free(data);
    mutexUnlock(&ngs_lock);
    return ngs_result(__func__, result);
}
EXPORT(SceNgs,0x9AB87E71,int,sceNgsVoiceBypassModule,NgsHandle id, uint32_t module, uint32_t bypass)
{
    mutexLock(&ngs_lock);
    NgsVoice *v = find_voice(id);
    int result = !v ? NGS_INVALID_HANDLE : module >= v->rack->description.definition->modules ||
        (bypass != 0 && bypass != 2) ? NGS_INVALID_ARG : 0;
    if (!result) v->module[module].bypassed = bypass == 2;
    mutexUnlock(&ngs_lock);
    return ngs_result(__func__, result);
}

EXPORT(SceNgs,0xD668B49C,int,sceNgsPatchCreateRouting,const NgsPatchSetup *setup, NgsHandle *output)
{
    if (!setup || !output) return ngs_result(__func__, NGS_INVALID_ARG);
    mutexLock(&ngs_lock);
    NgsVoice *source = find_voice(setup->source), *dest = find_voice(setup->dest);
    int result = !source || !dest ? NGS_INVALID_HANDLE : 0;
    int32_t slot = setup->source_output_subindex;
    if (!result) {
        const NgsRackDescription *s = &source->rack->description, *d = &dest->rack->description;
        if (source->rack->system != dest->rack->system || setup->source_output_index < 0 ||
            (unsigned)setup->source_output_index >= s->definition->outputs ||
            slot < -1 || slot >= s->patches_per_output || setup->dest_input_index != 0)
            result = NGS_INVALID_ARG;
        else {
            uint64_t occupied = 0;
            unsigned incoming = 0;
            for (NgsPatch *p = source->rack->system->patches; p; p = p->next) {
                if (p->source == source && p->output == setup->source_output_index)
                    occupied |= UINT64_C(1) << p->subindex;
                if (p->dest == dest && p->input == setup->dest_input_index) ++incoming;
            }
            if (incoming >= (unsigned)d->max_patches_per_input) result = NGS_PARAM_OUT_OF_RANGE;
            else if (slot == -1) {
                for (slot = 0; slot < s->patches_per_output; ++slot)
                    if (!(occupied & (UINT64_C(1)<<slot))) break;
                if (slot == s->patches_per_output) result = NGS_PARAM_OUT_OF_RANGE;
            } else if (occupied & (UINT64_C(1)<<slot)) result = NGS_INVALID_STATE;
        }
    }
    if (!result && (ngs_patch_count >= NGS_MAX_TOTAL_PATCHES || !handles_available(1))) result = NGS_ERROR;
    if (!result) {
        NgsPatch *p = calloc(1, sizeof(*p));
        if (!p) result = NGS_ERROR;
        else {
            p->id = take_handle();
            p->source = source;
            p->dest = dest;
            p->output = setup->source_output_index;
            p->subindex = slot;
            p->input = setup->dest_input_index;
            p->next = source->rack->system->patches;
            source->rack->system->patches = p;
            ++ngs_patch_count;
            *output = p->id;
        }
    }
    mutexUnlock(&ngs_lock);
    return ngs_result(__func__, result);
}
EXPORT(SceNgs,0xD0C9AE5A,int,sceNgsPatchRemoveRouting,NgsHandle id)
{
    mutexLock(&ngs_lock);
    NgsPatch *p = find_patch(id);
    int result = p ? 0 : NGS_INVALID_HANDLE;
    if (p) remove_patch(p->source->rack->system, p);
    mutexUnlock(&ngs_lock);
    return ngs_result(__func__, result);
}
EXPORT(SceNgs,0xA0F5402D,int,sceNgsVoicePatchSetVolumesMatrix,NgsHandle id, const NgsVolumeMatrix *matrix)
{
    if (!matrix) return ngs_result(__func__, NGS_INVALID_ARG);
    mutexLock(&ngs_lock);
    NgsPatch *p = find_patch(id);
    int result = p ? 0 : NGS_INVALID_HANDLE;
    if (p) p->matrix = *matrix;
    mutexUnlock(&ngs_lock);
    return ngs_result(__func__, result);
}

DECLARE_LIBRARY(SceNgs,0xB01598D9);
