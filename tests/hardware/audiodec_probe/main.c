/* Owned AT9 ABI batch. Original synthetic input; no libc startup or playback. */
#include <stdint.h>
#include <stddef.h>
#include <psp2/audiodec.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/processmgr.h>

#define REPORT "app0:/audiodec-probe-v2.txt"
#define CANARY UINT32_C(0x6d219ac7)
#define CODEC SCE_AUDIODEC_TYPE_AT9
#define GUARDED(type,name) static struct { uint32_t before; type value; uint32_t after; } name
#define ARM(name) do{(name).before=CANARY;(name).after=CANARY;}while(0)
#define GOOD(name) ((name).before==CANARY && (name).after==CANARY)
typedef struct { unsigned char bytes[78+8]; } Es;
typedef struct { unsigned char bytes[1024]; } Pcm;
typedef struct { uint32_t lead[64]; unsigned char bytes[1024*1024]; uint32_t tail[64]; } Context;
static Context context __attribute__((aligned(256)));
GUARDED(SceAudiodecInitParam, init);
GUARDED(SceAudiodecInfo, info);
GUARDED(SceAudiodecInfo, external_info);
GUARDED(SceAudiodecInfo, bad_info);
GUARDED(SceAudiodecCtrl, ctrl);
GUARDED(SceAudiodecCtrl, external);
GUARDED(SceAudiodecCtrl, bad);
GUARDED(Es, es);
GUARDED(Pcm, pcm);
GUARDED(Pcm, first_pcm);
GUARDED(SceUIntVAddr, returned_va);
GUARDED(SceInt32, internal_error);
static unsigned failures,passes;
static unsigned context_allowed;

static void fill(void *ptr,unsigned count,unsigned char value)
{ unsigned char *p=ptr;for(unsigned i=0;i<count;++i)p[i]=value; }
static int same(const void *a,const void *b,unsigned count)
{ const unsigned char *x=a,*y=b;for(unsigned i=0;i<count;++i)if(x[i]!=y[i])return 0;return 1; }
static int write_all(int fd,const char *text,unsigned count)
{
    while(count){int n=sceIoWrite(fd,text,count);if(n<=0||(unsigned)n>count)return -1;text+=n;count-=(unsigned)n;}
    return 0;
}
static void report(const char *tag,int ok,uint32_t detail)
{
    char text[128];unsigned n=0;const char *status=ok?"PASS ":"FAIL ";
    while(*status)text[n++]=*status++;
    while(*tag && n<100)text[n++]=*tag++;
    text[n++]=' ';text[n++]='0';text[n++]='x';
    for(unsigned i=0;i<8;++i)text[n++]="0123456789abcdef"[(detail>>(28-4*i))&15];
    text[n++]='\n';
    int fd=sceIoOpen(REPORT,SCE_O_WRONLY|SCE_O_APPEND,0);
    int io=fd<0?-1:write_all(fd,text,n);
    if(fd>=0 && sceIoClose(fd)<0)io=-1;
    if(ok&&!io)++passes;else ++failures;
}
static void prepare(SceAudiodecCtrl *c,SceAudiodecInfo *i)
{
    fill(c,sizeof(*c),0);fill(i,sizeof(*i),0);
    i->at9.size=sizeof(i->at9);
    i->at9.configData[0]=0xfe;i->at9.configData[1]=0x70;i->at9.configData[2]=0x04;i->at9.configData[3]=0xc0;
    c->size=sizeof(*c);c->wordLength=SCE_AUDIODEC_WORD_LENGTH_16BITS;c->pInfo=i;
    c->pEs=es.value.bytes;c->pPcm=pcm.value.bytes;
}
static int zero(unsigned count)
{ for(unsigned i=0;i<count;++i)if(pcm.value.bytes[i])return 0;return 1; }
static int tail_untouched(unsigned first)
{ for(unsigned i=first;i<sizeof(pcm.value.bytes);++i)if(pcm.value.bytes[i]!=0xa5)return 0;return 1; }
static int guards(void)
{
    for(unsigned i=0;i<64;++i)if(context.lead[i]!=CANARY || context.tail[i]!=CANARY)return 0;
    if(context_allowed)for(unsigned i=context_allowed;i<sizeof(context.bytes);++i)
        if(context.bytes[i]!=0xa5)return 0;
    return GOOD(init)&&GOOD(info)&&GOOD(external_info)&&GOOD(bad_info)&&GOOD(ctrl)&&GOOD(external)&&
        GOOD(bad)&&GOOD(es)&&GOOD(pcm)&&GOOD(first_pcm)&&GOOD(returned_va)&&GOOD(internal_error);
}
#define REQUIRE(tag,expression) do{int ok=(expression);report(tag,ok,(uint32_t)rc);if(!ok)goto cleanup;}while(0)

int module_start(unsigned args,void *argp)
{
    (void)args;(void)argp;
    int active=0,internal_live=0,external_live=0,bad_live=0,rc=0;
    int fd=sceIoOpen(REPORT,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
    if(fd<0){sceKernelExitProcess(1);return -1;}
    const char header[]="AUDIODEC_PROBE version=2\n";
    int io=write_all(fd,header,sizeof(header)-1);if(sceIoClose(fd)<0)io=-1;
    if(io){sceKernelExitProcess(1);return -1;}
    ARM(init);ARM(info);ARM(external_info);ARM(bad_info);ARM(ctrl);ARM(external);ARM(bad);
    ARM(es);ARM(pcm);ARM(first_pcm);ARM(returned_va);ARM(internal_error);
    for(unsigned i=0;i<64;++i)context.lead[i]=context.tail[i]=CANARY;
    /* Source-guided original39Bframe, plus secondframe and decoder lookahead. */
    es.value.bytes[4]=0x07;es.value.bytes[5]=0x42;es.value.bytes[6]=0x10;
    es.value.bytes[7]=0x84;es.value.bytes[8]=0x21;es.value.bytes[9]=0x08;es.value.bytes[10]=0x42;
    for(unsigned i=0;i<39;++i)es.value.bytes[39+i]=es.value.bytes[i];
    init.value.at9=(SceAudiodecInitChParam){sizeof(SceAudiodecInitChParam),2};
    rc=sceAudiodecInitLibrary(CODEC,&init.value);active=rc==0;
    REQUIRE("library_init",rc==0);
    prepare(&ctrl.value,&info.value);
    rc=sceAudiodecGetContextSize(&ctrl.value,CODEC);
    REQUIRE("context_size",rc>0 && (unsigned)rc<=sizeof(context.bytes));
    unsigned context_size=(unsigned)rc;
    rc=sceAudiodecCreateDecoder(&ctrl.value,CODEC);internal_live=rc==0;
    REQUIRE("internal_create",rc==0 && ctrl.value.handle>0);
    REQUIRE("ctrl_sizes",ctrl.value.maxEsSize==39 && ctrl.value.maxPcmSize==512);
    REQUIRE("codec_info",info.value.at9.ch==1 && info.value.at9.samplingRate==48000 &&
        info.value.at9.superFrameSize==39 && info.value.at9.framesInSuperFrame==1);
    fill(pcm.value.bytes,sizeof(pcm.value.bytes),0xa5);
    rc=sceAudiodecDecode(&ctrl.value);
    REQUIRE("decode_one",rc==0 && ctrl.value.inputEsSize==39 && ctrl.value.outputPcmSize==512);
    REQUIRE("decode_pcm",zero(512) && tail_untouched(512) && GOOD(pcm));
    internal_error.value=123;
    rc=sceAudiodecGetInternalError(&ctrl.value,&internal_error.value);
    REQUIRE("internal_error_zero",rc==0 && internal_error.value==0);
    rc=sceAudiodecClearContext(&ctrl.value);
    REQUIRE("clear_context",rc==0);
    fill(pcm.value.bytes,sizeof(pcm.value.bytes),0xa5);
    rc=sceAudiodecDecodeNFrames(&ctrl.value,2);
    REQUIRE("decode_two",rc==0 && ctrl.value.inputEsSize==78 && ctrl.value.outputPcmSize==1024);
    REQUIRE("decode_two_pcm",zero(1024) && GOOD(pcm));
    /* Original coefficient0=+128, scale24/gradient16: raw9-bit precision8.
     * Explicit bytes0..10; remaining28frame bytes and lookahead are zero. */
    fill(es.value.bytes,sizeof(es.value.bytes),0);
    const unsigned char nonzero_frame[11]={0,0,0x04,0x20,0x07,0xc6,0x31,0x8c,0x63,0x18,0xc6};
    /* The coefficient starts at bit90: remaining bits continue in byte11. */
    for(unsigned i=0;i<sizeof(nonzero_frame);++i)es.value.bytes[i]=nonzero_frame[i];
    es.value.bytes[11]=0x10;
    rc=sceAudiodecClearContext(&ctrl.value);
    REQUIRE("nonzero_clear",rc==0);
    fill(pcm.value.bytes,sizeof(pcm.value.bytes),0xa5);
    rc=sceAudiodecDecode(&ctrl.value);
    REQUIRE("nonzero_decode",rc==0 && ctrl.value.inputEsSize==39 && ctrl.value.outputPcmSize==512);
    REQUIRE("nonzero_pcm",!zero(512) && tail_untouched(512) && guards());
    for(unsigned i=0;i<512;++i)first_pcm.value.bytes[i]=pcm.value.bytes[i];
    fill(pcm.value.bytes,sizeof(pcm.value.bytes),0xa5);
    rc=sceAudiodecDecode(&ctrl.value);
    REQUIRE("nonzero_stream_decode",rc==0 && ctrl.value.inputEsSize==39 && ctrl.value.outputPcmSize==512);
    REQUIRE("nonzero_stream_history",!zero(512) && !same(pcm.value.bytes,first_pcm.value.bytes,512) &&
        tail_untouched(512) && guards());
    rc=sceAudiodecClearContext(&ctrl.value);
    REQUIRE("nonzero_reset",rc==0);
    fill(pcm.value.bytes,sizeof(pcm.value.bytes),0xa5);
    rc=sceAudiodecDecode(&ctrl.value);
    REQUIRE("nonzero_replay",rc==0 && ctrl.value.inputEsSize==39 && ctrl.value.outputPcmSize==512 &&
        same(pcm.value.bytes,first_pcm.value.bytes,512) && tail_untouched(512) && guards());
    for(unsigned test=0;test<2;++test){
        fill(es.value.bytes,sizeof(es.value.bytes),0);
        es.value.bytes[0]=test?0x23:0x02;
        fill(pcm.value.bytes,sizeof(pcm.value.bytes),0xa5);
        ctrl.value.inputEsSize=ctrl.value.outputPcmSize=123;
        rc=sceAudiodecDecode(&ctrl.value);
        REQUIRE(test?"bex_high_rejected":"bex_low_rejected",rc<0 &&
            ctrl.value.inputEsSize==0 && ctrl.value.outputPcmSize==0 && tail_untouched(0) && guards());
        rc=sceAudiodecClearContext(&ctrl.value);
        REQUIRE(test?"bex_high_clear":"bex_low_clear",rc==0);
        fill(es.value.bytes,sizeof(es.value.bytes),0);
        for(unsigned i=0;i<sizeof(nonzero_frame);++i)es.value.bytes[i]=nonzero_frame[i];
        es.value.bytes[11]=0x10;
        rc=sceAudiodecDecode(&ctrl.value);
        REQUIRE(test?"bex_high_recovered":"bex_low_recovered",rc==0 && ctrl.value.inputEsSize==39 &&
            ctrl.value.outputPcmSize==512 && same(pcm.value.bytes,first_pcm.value.bytes,512) &&
            tail_untouched(512) && guards());
    }
    fill(es.value.bytes,sizeof(es.value.bytes),0);
    es.value.bytes[4]=0x07;es.value.bytes[5]=0x42;es.value.bytes[6]=0x10;
    es.value.bytes[7]=0x84;es.value.bytes[8]=0x21;es.value.bytes[9]=0x08;es.value.bytes[10]=0x42;
    for(unsigned i=0;i<39;++i)es.value.bytes[39+i]=es.value.bytes[i];
    SceAudiodecCtrl snapshot=ctrl.value;
    snapshot.inputEsSize=snapshot.outputPcmSize=0;
    fill(pcm.value.bytes,sizeof(pcm.value.bytes),0xa5);
    rc=sceAudiodecDecodeNFrames(&ctrl.value,0);
    REQUIRE("zero_frames_rejected",rc==(int)SCE_AUDIODEC_ERROR_INVALID_NFRAMES &&
        same(&ctrl.value,&snapshot,sizeof(snapshot)) && tail_untouched(0));
    rc=sceAudiodecDecode(0);
    REQUIRE("null_ctrl_rejected",rc==(int)SCE_AUDIODEC_ERROR_INVALID_PTR);
    ctrl.value.size=sizeof(ctrl.value)-4;
    rc=sceAudiodecDecode(&ctrl.value);
    ctrl.value.size=sizeof(ctrl.value);
    REQUIRE("ctrl_shape_rejected",rc==(int)SCE_AUDIODEC_ERROR_INVALID_SIZE &&
        ctrl.value.inputEsSize==snapshot.inputEsSize && ctrl.value.outputPcmSize==snapshot.outputPcmSize && tail_untouched(0));
    internal_error.value=123;
    rc=sceAudiodecGetInternalError(&ctrl.value,&internal_error.value);
    REQUIRE("validation_not_backend_error",rc==0 && internal_error.value==0);
    prepare(&bad.value,&bad_info.value);bad_info.value.at9.configData[0]=0;
    SceAudiodecCtrl bad_snapshot=bad.value;
    rc=sceAudiodecCreateDecoder(&bad.value,CODEC);
    bad_live=rc==0;
    REQUIRE("config_rejected",rc==(int)SCE_AUDIODEC_AT9_ERROR_INVALID_CONFIG &&
        same(&bad.value,&bad_snapshot,sizeof(bad_snapshot)));
    prepare(&external.value,&external_info.value);
    rc=sceAudiodecGetContextSize(&external.value,CODEC);
    REQUIRE("external_size",rc>0 && (unsigned)rc<=sizeof(context.bytes) && (unsigned)rc==context_size &&
        ((uintptr_t)context.bytes&255)==0);
    context_allowed=context_size;
    fill(context.bytes+context_allowed,sizeof(context.bytes)-context_allowed,0xa5);
    rc=sceAudiodecCreateDecoderExternal(&external.value,CODEC,(SceUIntVAddr)(uintptr_t)context.bytes,context_size);
    external_live=rc==0;
    REQUIRE("external_create",rc==0 && external.value.handle>0 && external.value.handle!=ctrl.value.handle);
    REQUIRE("external_sizes",external.value.maxEsSize==39 && external.value.maxPcmSize==512 &&
        external_info.value.at9.ch==1 && external_info.value.at9.samplingRate==48000);
    prepare(&bad.value,&bad_info.value);bad_snapshot=bad.value;
    rc=sceAudiodecCreateDecoder(&bad.value,CODEC);
    bad_live=rc==0;
    REQUIRE("channel_budget",rc==(int)SCE_AUDIODEC_ERROR_CH_SHORTAGE && same(&bad.value,&bad_snapshot,sizeof(bad_snapshot)));
    rc=sceAudiodecTermLibrary(CODEC);
    REQUIRE("term_active_rejected",rc==(int)SCE_AUDIODEC_ERROR_A_HANDLE_IN_USE);
    fill(pcm.value.bytes,sizeof(pcm.value.bytes),0xa5);
    rc=sceAudiodecDecode(&external.value);
    REQUIRE("external_decode",rc==0 && external.value.inputEsSize==39 && external.value.outputPcmSize==512 && zero(512) && tail_untouched(512));
    returned_va.value=0;
    rc=sceAudiodecDeleteDecoderExternal(&external.value,&returned_va.value);
    if(rc==0)external_live=0;
    REQUIRE("external_delete",rc==0 && returned_va.value==(SceUIntVAddr)(uintptr_t)context.bytes);
    rc=sceAudiodecDecode(&external.value);
    REQUIRE("external_stale",rc==(int)SCE_AUDIODEC_ERROR_INVALID_HANDLE);
    rc=sceAudiodecDeleteDecoder(&ctrl.value);
    if(rc==0)internal_live=0;
    REQUIRE("internal_delete",rc==0);
    rc=sceAudiodecClearContext(&ctrl.value);
    REQUIRE("internal_stale",rc==(int)SCE_AUDIODEC_ERROR_INVALID_HANDLE);
    rc=sceAudiodecTermLibrary(CODEC);if(rc==0)active=0;
    REQUIRE("library_term",rc==0);
    rc=sceAudiodecTermLibrary(CODEC);
    REQUIRE("term_stale",rc==(int)SCE_AUDIODEC_ERROR_NOT_INITIALIZED);
cleanup:
    if(bad_live)sceAudiodecDeleteDecoder(&bad.value);
    if(external_live)sceAudiodecDeleteDecoderExternal(&external.value,&returned_va.value);
    if(internal_live)sceAudiodecDeleteDecoder(&ctrl.value);
    if(active)sceAudiodecTermLibrary(CODEC);
    report("guards",guards(),0);
    report("batch_complete",failures==0,passes);
    sceKernelExitProcess(failures?1:0);
    return failures?-1:0;
}
