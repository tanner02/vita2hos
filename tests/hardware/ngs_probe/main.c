/* Owned silent NGS ABI/lifecycle batch; no Vita libc startup or audio device. */
#include <stdint.h>
#include <stddef.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/sysmodule.h>
#include <psp2/audioout.h>
#include "modules/SceNgs.h"

#define REPORT "app0:/ngs-probe-v2.txt"
#define CANARY UINT32_C(0x719ac35e)
#define GRANULARITY 128
_Static_assert(sizeof(void *)==4,"native32 guest required");
_Static_assert(sizeof(NgsSystemParams)==20,"system ABI");
_Static_assert(sizeof(NgsBufferInfo)==8,"buffer ABI");
_Static_assert(sizeof(NgsRackDescription)==24,"rack ABI");
_Static_assert(sizeof(NgsVoiceInfo)==24,"voice ABI");
_Static_assert(sizeof(NgsPatchSetup)==20,"patch ABI");
_Static_assert(sizeof(NgsVolumeMatrix)==16,"matrix ABI");
_Static_assert(sizeof(NgsModuleParamHeader)==8,"params ABI");
#define GUARDED(type, name) static struct { uint32_t before; type value; uint32_t after; } name
#define ARM(name) do { (name).before=CANARY; (name).after=CANARY; } while (0)
#define GOOD(name) ((name).before==CANARY && (name).after==CANARY)
typedef struct { uint32_t data[GRANULARITY]; } OutputState;
typedef struct { uint32_t data[6]; } PlayerState;
typedef struct { NgsModuleParamHeader header; unsigned char data[72]; } EqRecord;
typedef struct { int16_t samples[128*2]; } SilentPcm;
typedef struct { uint32_t lead[16]; unsigned char data[65536]; uint32_t tail[16]; } Backing;
static Backing system_mem __attribute__((aligned(64)));
static Backing simple_mem __attribute__((aligned(64)));
static Backing master_mem __attribute__((aligned(64)));
GUARDED(NgsSystemParams, params);
GUARDED(NgsRackDescription, simple_desc);
GUARDED(NgsRackDescription, master_desc);
GUARDED(NgsBufferInfo, simple_buf);
GUARDED(NgsBufferInfo, master_buf);
GUARDED(NgsBufferInfo, eq);
GUARDED(NgsBufferInfo, eq_other);
GUARDED(NgsVoiceInfo, info);
GUARDED(NgsPatchSetup, routing);
GUARDED(NgsVolumeMatrix, volumes);
GUARDED(OutputState, output);
GUARDED(PlayerState, player);
GUARDED(uint32_t, system_size);
GUARDED(uint32_t, simple_size);
GUARDED(uint32_t, master_size);
GUARDED(NgsHandle, system_handle);
GUARDED(NgsHandle, simple_rack);
GUARDED(NgsHandle, master_rack);
GUARDED(NgsHandle, voice);
GUARDED(NgsHandle, same_voice);
GUARDED(NgsHandle, other_voice);
GUARDED(NgsHandle, master_voice);
GUARDED(NgsHandle, patch);
GUARDED(int32_t, errors);
GUARDED(EqRecord, eq_record);
GUARDED(SilentPcm, pcm);
GUARDED(int, main_port);
GUARDED(int, bgm_port);
static unsigned failures, passes;

static int write_all(int fd, const char *text, unsigned count)
{
    while (count) {
        int n=sceIoWrite(fd,text,count);
        if (n<=0 || (unsigned)n>count) return -1;
        text+=n; count-=(unsigned)n;
    }
    return 0;
}
static void report(const char *tag, int ok, uint32_t detail)
{
    char text[128]; unsigned n=0;
    const char *status=ok?"PASS ":"FAIL ";
    while (*status) text[n++]=*status++;
    while (*tag && n<100) text[n++]=*tag++;
    text[n++]=' ';text[n++]='0';text[n++]='x';
    for (unsigned i=0;i<8;++i) text[n++]="0123456789abcdef"[(detail>>(28-4*i))&15];
    text[n++]='\n';
    int fd=sceIoOpen(REPORT,SCE_O_WRONLY|SCE_O_APPEND,0);
    int io=fd<0?-1:write_all(fd,text,n);
    if (fd>=0 && sceIoClose(fd)<0) io=-1;
    if (ok && !io) ++passes; else ++failures;
}
static void arm_backing(Backing *b)
{ for (unsigned i=0;i<16;++i) b->lead[i]=b->tail[i]=CANARY; }
static int good_backing(const Backing *b)
{
    for (unsigned i=0;i<16;++i) if(b->lead[i]!=CANARY || b->tail[i]!=CANARY)return 0;
    return 1;
}
static int guards(void)
{
    return good_backing(&system_mem) && good_backing(&simple_mem) && good_backing(&master_mem) &&
        GOOD(params) && GOOD(simple_desc) && GOOD(master_desc) && GOOD(simple_buf) && GOOD(master_buf) &&
        GOOD(eq) && GOOD(eq_other) && GOOD(info) && GOOD(routing) && GOOD(volumes) && GOOD(output) &&
        GOOD(player) && GOOD(system_size) && GOOD(simple_size) && GOOD(master_size) && GOOD(system_handle) &&
        GOOD(simple_rack) && GOOD(master_rack) && GOOD(voice) && GOOD(same_voice) && GOOD(other_voice) &&
        GOOD(master_voice) && GOOD(patch) && GOOD(errors) && GOOD(eq_record) &&
        GOOD(pcm) && GOOD(main_port) && GOOD(bgm_port);
}
#define REQUIRE(tag, expression) do { int ok=(expression); report(tag,ok,(uint32_t)rc); if(!ok)goto cleanup; } while(0)

int module_start(unsigned args, void *argp)
{
    (void)args; (void)argp;
    int rc=0, eq_locked=0, other_locked=0;
    int fd=sceIoOpen(REPORT,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
    if(fd<0) { sceKernelExitProcess(1); return -1; }
    const char header[]="NGS_PROBE version=2\n";
    int header_result=write_all(fd,header,sizeof(header)-1);
    if(sceIoClose(fd)<0)header_result=-1;
    if(header_result) {
        sceKernelExitProcess(1); return -1;
    }
    ARM(params); ARM(simple_desc); ARM(master_desc); ARM(simple_buf); ARM(master_buf);
    ARM(eq); ARM(eq_other); ARM(info); ARM(routing); ARM(volumes); ARM(output); ARM(player);
    ARM(system_size); ARM(simple_size); ARM(master_size); ARM(system_handle); ARM(simple_rack);
    ARM(master_rack); ARM(voice); ARM(same_voice); ARM(other_voice); ARM(master_voice); ARM(patch); ARM(errors); ARM(eq_record);
    ARM(pcm); ARM(main_port); ARM(bgm_port);
    main_port.value=bgm_port.value=-1;
    arm_backing(&system_mem); arm_backing(&simple_mem); arm_backing(&master_mem);
    rc=sceSysmoduleLoadModule(0x0b);
    REQUIRE("sysmodule",rc==0);
    const NgsVoiceDefinition *simple=sceNgsVoiceDefGetSimpleVoice();
    const NgsVoiceDefinition *master=sceNgsVoiceDefGetMasterBuss();
    const NgsVoiceDefinition *reverb=sceNgsVoiceDefGetReverbBuss();
    REQUIRE("definitions",simple && master && reverb && simple!=master && simple!=reverb && master!=reverb);
    params.value=(NgsSystemParams){2,4,GRANULARITY,48000,0};
    rc=sceNgsSystemGetRequiredMemorySize(&params.value,&system_size.value);
    REQUIRE("system_query",rc==0 && system_size.value>0 && system_size.value<=sizeof(system_mem.data));
    rc=sceNgsSystemInit(system_mem.data,system_size.value,&params.value,&system_handle.value);
    REQUIRE("system_init",rc==0 && system_handle.value);
    rc=sceNgsSystemSetFlags(system_handle.value,0);
    REQUIRE("system_flags",rc==0);
    simple_desc.value=(NgsRackDescription){simple,2,2,2,2,0};
    master_desc.value=(NgsRackDescription){master,2,2,2,2,0};
    rc=sceNgsRackGetRequiredMemorySize(system_handle.value,&simple_desc.value,&simple_size.value);
    REQUIRE("simple_query",rc==0 && simple_size.value>0 && simple_size.value<=sizeof(simple_mem.data));
    rc=sceNgsRackGetRequiredMemorySize(system_handle.value,&master_desc.value,&master_size.value);
    REQUIRE("master_query",rc==0 && master_size.value>0 && master_size.value<=sizeof(master_mem.data));
    simple_buf.value=(NgsBufferInfo){simple_mem.data,simple_size.value};
    master_buf.value=(NgsBufferInfo){master_mem.data,master_size.value};
    rc=sceNgsRackInit(system_handle.value,&simple_buf.value,&simple_desc.value,&simple_rack.value);
    REQUIRE("simple_init",rc==0 && simple_rack.value);
    rc=sceNgsRackInit(system_handle.value,&master_buf.value,&master_desc.value,&master_rack.value);
    REQUIRE("master_init",rc==0 && master_rack.value && master_rack.value!=simple_rack.value);
    rc=sceNgsRackGetVoiceHandle(simple_rack.value,0,&voice.value);
    REQUIRE("voice",rc==0 && voice.value);
    rc=sceNgsRackGetVoiceHandle(simple_rack.value,0,&same_voice.value);
    REQUIRE("voice_stable",rc==0 && voice.value==same_voice.value);
    rc=sceNgsRackGetVoiceHandle(simple_rack.value,1,&other_voice.value);
    REQUIRE("voice_distinct",rc==0 && other_voice.value && other_voice.value!=voice.value);
    rc=sceNgsRackGetVoiceHandle(master_rack.value,0,&master_voice.value);
    REQUIRE("master_voice",rc==0 && master_voice.value && master_voice.value!=voice.value);
    rc=sceNgsVoiceGetInfo(voice.value,&info.value);
    REQUIRE("simple_info",rc==0 && info.value.num_modules==6 && info.value.num_outputs==2);
    rc=sceNgsVoiceGetInfo(master_voice.value,&info.value);
    REQUIRE("master_info",rc==0 && info.value.num_modules==2 && info.value.num_outputs==1);
    rc=sceNgsVoiceLockParams(voice.value,1,0x01015cec,&eq.value);
    eq_locked=rc==0;
    REQUIRE("eq_lock",eq_locked && eq.value.data && eq.value.size>=0x58);
    *(uint32_t *)((unsigned char *)eq.value.data+0x54)=UINT32_C(0x3f000000);
    rc=sceNgsVoiceUnlockParams(voice.value,1); eq_locked=rc!=0;
    REQUIRE("eq_unlock",rc==0);
    rc=sceNgsVoiceLockParams(voice.value,1,0x01015cec,&eq.value); eq_locked=rc==0;
    REQUIRE("eq_persist",eq_locked && eq.value.data && eq.value.size>=0x58 &&
        *(uint32_t *)((unsigned char *)eq.value.data+0x54)==UINT32_C(0x3f000000));
    rc=sceNgsVoiceLockParams(other_voice.value,1,0x01015cec,&eq_other.value); other_locked=rc==0;
    REQUIRE("eq_isolation",other_locked && eq_other.value.data && eq_other.value.size>=0x58 &&
        eq_other.value.data!=eq.value.data && *(uint32_t *)((unsigned char *)eq_other.value.data+0x54)==0);
    eq_record.value.header=(NgsModuleParamHeader){1,0};
    for(unsigned i=0;i<sizeof(eq_record.value.data);++i)
        eq_record.value.data[i]=((unsigned char *)eq.value.data)[i];
    rc=sceNgsVoiceUnlockParams(other_voice.value,1); other_locked=rc!=0;
    if(rc==0){rc=sceNgsVoiceUnlockParams(voice.value,1);eq_locked=rc!=0;}
    REQUIRE("eq_final_unlock",rc==0);
    errors.value=99;
    rc=sceNgsVoiceSetParamsBlock(voice.value,&eq_record.value.header,sizeof(eq_record.value),&errors.value);
    REQUIRE("params_block",rc==0 && errors.value==0 && GOOD(eq_record));
    for(unsigned i=0;i<GRANULARITY;++i)output.value.data[i]=UINT32_C(0xa5a5a5a5);
    rc=sceNgsVoiceGetStateData(master_voice.value,1,&output.value,sizeof(output.value));
    int zero=1;for(unsigned i=0;i<GRANULARITY;++i)if(output.value.data[i])zero=0;
    REQUIRE("master_silent_pcm",rc==0 && zero && GOOD(output));
    for(unsigned i=0;i<6;++i)player.value.data[i]=UINT32_C(0xa5a5a5a5);
    rc=sceNgsVoiceGetStateData(voice.value,0,&player.value,sizeof(player.value));
    zero=1;for(unsigned i=0;i<6;++i)if(player.value.data[i])zero=0;
    REQUIRE("player_state",rc==0 && zero && GOOD(player));
    rc=sceNgsVoiceBypassModule(voice.value,1,2);
    REQUIRE("bypass",rc==0);
    errors.value=99;
    rc=sceNgsVoiceSetParamsBlock(voice.value,0,8,&errors.value);
    REQUIRE("params_reject_null",rc==NGS_INVALID_ARG && errors.value==1);
    routing.value=(NgsPatchSetup){voice.value,0,-1,master_voice.value,0};
    rc=sceNgsPatchCreateRouting(&routing.value,&patch.value);
    REQUIRE("patch_create",rc==0 && patch.value);
    volumes.value=(NgsVolumeMatrix){{{1.0f,0.0f},{0.0f,1.0f}}};
    rc=sceNgsVoicePatchSetVolumesMatrix(patch.value,&volumes.value);
    REQUIRE("patch_matrix",rc==0);
    rc=sceNgsVoicePlay(voice.value);
    if(rc==0)rc=sceNgsVoiceGetInfo(voice.value,&info.value);
    REQUIRE("play",rc==0 && (info.value.voice_state&NGS_VOICE_ACTIVE));
    rc=sceNgsVoicePause(voice.value);
    if(rc==0)rc=sceNgsVoiceGetInfo(voice.value,&info.value);
    REQUIRE("pause",rc==0 && (info.value.voice_state&NGS_VOICE_PAUSED));
    rc=sceNgsVoiceResume(voice.value);
    if(rc==0)rc=sceNgsVoiceGetInfo(voice.value,&info.value);
    REQUIRE("resume",rc==0 && (info.value.voice_state&NGS_VOICE_ACTIVE) && !(info.value.voice_state&NGS_VOICE_PAUSED));
    rc=sceNgsVoiceKill(voice.value);
    if(rc==0)rc=sceNgsVoiceGetInfo(voice.value,&info.value);
    REQUIRE("kill",rc==0 && !(info.value.voice_state&(NGS_VOICE_ACTIVE|NGS_VOICE_PAUSED)));
    rc=sceNgsSystemUpdate(system_handle.value);
    REQUIRE("system_update",rc==0);
    rc=sceNgsPatchRemoveRouting(patch.value);
    REQUIRE("patch_remove",rc==0);
    rc=sceNgsPatchRemoveRouting(patch.value);patch.value=0;
    REQUIRE("patch_stale",rc==NGS_INVALID_HANDLE);
    rc=sceNgsRackRelease(simple_rack.value,0);
    if(rc==0)simple_rack.value=0;
    REQUIRE("simple_release",rc==0);
    rc=sceNgsVoiceGetInfo(voice.value,&info.value);
    REQUIRE("voice_stale",rc==NGS_INVALID_HANDLE);
    rc=sceNgsRackRelease(master_rack.value,0);
    if(rc==0)master_rack.value=0;
    REQUIRE("master_release",rc==0);
    rc=sceNgsSystemRelease(system_handle.value);
    REQUIRE("system_release",rc==0);
    rc=sceNgsSystemUpdate(system_handle.value);system_handle.value=0;
    REQUIRE("system_stale",rc==NGS_INVALID_HANDLE);
    /* No report I/O inside the measured sequence: logging must not consume a
     * queued sample interval before its next submission. PCM remains all zero. */
    main_port.value=sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN,128,48000,SCE_AUDIO_OUT_MODE_STEREO);
    bgm_port.value=sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM,128,48000,SCE_AUDIO_OUT_MODE_STEREO);
    rc=main_port.value<0?main_port.value:bgm_port.value<0?bgm_port.value:0;
    REQUIRE("audio_two_ports",rc==0 && main_port.value!=bgm_port.value);
    uint64_t audio_begin=sceKernelGetProcessTimeWide();
    int first_main=sceAudioOutOutput(main_port.value,&pcm.value);
    int first_bgm=sceAudioOutOutput(bgm_port.value,&pcm.value);
    uint64_t first_queued=sceKernelGetProcessTimeWide();
    int second_main=sceAudioOutOutput(main_port.value,&pcm.value);
    uint64_t main_submitted=sceKernelGetProcessTimeWide();
    int second_bgm=sceAudioOutOutput(bgm_port.value,&pcm.value);
    uint64_t bgm_submitted=sceKernelGetProcessTimeWide();
    int flush_main=sceAudioOutOutput(main_port.value,0);
    uint64_t main_flushed=sceKernelGetProcessTimeWide();
    int flush_bgm=sceAudioOutOutput(bgm_port.value,0);
    uint64_t all_flushed=sceKernelGetProcessTimeWide();
    uint64_t first_us=first_queued-audio_begin, main_us=main_submitted-first_queued;
    uint64_t bgm_us=bgm_submitted-main_submitted, flush_us=all_flushed-bgm_submitted;
    uint64_t total_us=all_flushed-first_queued;
    report("audio_first_queue_us",first_main==0 && first_bgm==0 && first_us<=1000000,(uint32_t)first_us);
    report("audio_main_wait_us",second_main==0 && main_us<=1000000,(uint32_t)main_us);
    report("audio_bgm_wait_us",second_bgm==0 && bgm_us<=1000000,(uint32_t)bgm_us);
    report("audio_null_flush_us",flush_main==0 && flush_bgm==0 && flush_us<=1000000 &&
        main_flushed>=bgm_submitted && all_flushed>=main_flushed,(uint32_t)flush_us);
    report("audio_total_paced_us",total_us>=4000 && total_us<=1000000,(uint32_t)total_us);
    int old_main=main_port.value,old_bgm=bgm_port.value;
    rc=sceAudioOutReleasePort(main_port.value);
    if(rc==0)main_port.value=-1;
    int release_bgm=sceAudioOutReleasePort(bgm_port.value);
    if(release_bgm==0)bgm_port.value=-1;
    REQUIRE("audio_release",rc==0 && release_bgm==0);
    rc=sceAudioOutOutput(old_main,&pcm.value);
    int stale_bgm=sceAudioOutOutput(old_bgm,&pcm.value);
    REQUIRE("audio_stale",rc==(int)SCE_AUDIO_OUT_ERROR_INVALID_PORT && stale_bgm==rc);
cleanup:
    if(main_port.value>=0)sceAudioOutReleasePort(main_port.value);
    if(bgm_port.value>=0)sceAudioOutReleasePort(bgm_port.value);
    if(other_locked)sceNgsVoiceUnlockParams(other_voice.value,1);
    if(eq_locked)sceNgsVoiceUnlockParams(voice.value,1);
    if(patch.value)sceNgsPatchRemoveRouting(patch.value);
    if(simple_rack.value)sceNgsRackRelease(simple_rack.value,0);
    if(master_rack.value)sceNgsRackRelease(master_rack.value,0);
    if(system_handle.value)sceNgsSystemRelease(system_handle.value);
    report("guards",guards(),0);
    report("batch_complete",failures==0,passes);
    sceKernelExitProcess(failures?1:0);
    return failures?-1:0;
}
