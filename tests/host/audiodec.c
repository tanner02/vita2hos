#define TEST_THREADED_MUTEX
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "bit_allocation.h"
#include "../../source/modules/SceAudiodec.c"
#include "../../source/module.c"
void log_printf(const char *format,...){(void)format;}
#define ERR(code) ((int)(code))
typedef int (*DecodeAbi)(SceAudiodecCtrl *);
typedef int (*FramesAbi)(SceAudiodecCtrl *,SceUInt32);
typedef int (*ExternalAbi)(SceAudiodecCtrl *,SceUInt32,SceUIntVAddr,SceUInt32);
_Static_assert(__builtin_types_compatible_p(__typeof__(&sceAudiodecDecode),DecodeAbi),"Decode has one argument");
_Static_assert(__builtin_types_compatible_p(__typeof__(&sceAudiodecDecodeNFrames),FramesAbi),"DecodeNFrames has two arguments");
_Static_assert(__builtin_types_compatible_p(__typeof__(&sceAudiodecCreateDecoderExternal),ExternalAbi),"External constructor has four arguments");
static struct {
    unsigned char before[256],data[1024*1024],after[256];
} external_storage __attribute__((aligned(256)));
static void all_bytes(const void *data,size_t size,unsigned char value)
{const unsigned char *p=data;for(size_t i=0;i<size;++i)assert(p[i]==value);}
static SceAudiodecCtrl control(SceAudiodecInfo *info)
{
    memset(info,0xa5,sizeof(*info));info->at9.size=sizeof(info->at9);
    const unsigned char config[4]={0xfe,0x74,0x00,0x60};memcpy(info->at9.configData,config,4);
    SceAudiodecCtrl result={.size=sizeof(result),.wordLength=16,.pInfo=info};
    return result;
}
static void rejected_create(SceAudiodecCtrl *ctrl,unsigned type)
{
    SceAudiodecCtrl before=*ctrl;SceAudiodecInfo saved;
    if(ctrl->pInfo) saved=*ctrl->pInfo;
    assert(sceAudiodecCreateDecoder(ctrl,type)<0);
    assert(!memcmp(ctrl,&before,sizeof(before)));
    if(ctrl->pInfo)assert(!memcmp(ctrl->pInfo,&saved,sizeof(saved)));
}
static void reject_decode(SceAudiodecCtrl *ctrl,unsigned frames)
{
    ctrl->inputEsSize=77;ctrl->outputPcmSize=88;
    int result=frames==1 ? sceAudiodecDecode(ctrl) : sceAudiodecDecodeNFrames(ctrl,frames);
    assert(result<0 && !ctrl->inputEsSize && !ctrl->outputPcmSize);
}
static void owned_silence(void)
{
    /* No media input: mono48000,39B frame, raw spectral coefficients allzero.
     * Nonzero syntax fields were constructed from LibAtrac9's bit reader. */
    unsigned char frame[128]={0};
    const unsigned char syntax[]={0x07,0x42,0x10,0x84,0x21,0x08,0x42};
    memcpy(frame+4,syntax,sizeof(syntax));memcpy(frame+39,frame,39);
    SceAudiodecInitParam init={.at9={sizeof(init),2}};
    assert(!sceAudiodecInitLibrary(SCE_AUDIODEC_TYPE_AT9,&init));
    SceAudiodecInfo info;SceAudiodecCtrl ctrl=control(&info);
    const unsigned char config[]={0xfe,0x70,0x04,0xc0};memcpy(info.at9.configData,config,4);
    assert(!sceAudiodecCreateDecoder(&ctrl,SCE_AUDIODEC_TYPE_AT9));
    assert(info.at9.ch==1 && info.at9.samplingRate==48000 && info.at9.superFrameSize==39);
    assert(ctrl.maxEsSize==39 && ctrl.maxPcmSize==512);
    unsigned char guarded[16+1024+16];memset(guarded,0xa5,sizeof(guarded));
    ctrl.pEs=frame;ctrl.pPcm=guarded+16;
    DecodeAbi decode=(DecodeAbi)module_get_func_export(0x2aa62046,0xccdaba04);assert(decode);
    assert(!decode(&ctrl) && ctrl.inputEsSize==39 && ctrl.outputPcmSize==512);
    all_bytes(guarded,16,0xa5);all_bytes(guarded+16,512,0);
    all_bytes(guarded+16+512,sizeof(guarded)-16-512,0xa5);
    assert(!sceAudiodecClearContext(&ctrl));memset(guarded,0xa5,sizeof(guarded));
    assert(ctrl.maxEsSize==39 && ctrl.maxPcmSize==512);
    FramesAbi decode_frames=(FramesAbi)module_get_func_export(0x2aa62046,0x8018aa9b);assert(decode_frames);
    assert(!decode_frames(&ctrl,2) && ctrl.inputEsSize==78 && ctrl.outputPcmSize==1024);
    all_bytes(guarded,16,0xa5);all_bytes(guarded+16,1024,0);all_bytes(guarded+1040,16,0xa5);
    /* Band headers can set BEX with ten or21 coded units, outside the eight
     * groups for units13..20. Both bounds must reject before the lookup. */
    const unsigned char band_headers[] = {2, 0x23};
    for (unsigned i=0;i<sizeof(band_headers);++i) {
        assert(!sceAudiodecClearContext(&ctrl));
        unsigned char no_bex_group[128] = {0};no_bex_group[0]=band_headers[i];
        ctrl.pEs=no_bex_group;memset(guarded,0xa5,sizeof(guarded));
        assert(sceAudiodecDecode(&ctrl)<0 && ctrl.inputEsSize==0 && ctrl.outputPcmSize==0);
        all_bytes(guarded,sizeof(guarded),0xa5);
    }
    assert(!sceAudiodecClearContext(&ctrl));ctrl.pEs=frame;
    assert(!decode(&ctrl) && ctrl.inputEsSize==39 && ctrl.outputPcmSize==512);
    /* Original raw-spectrum fixture: constant gradient16, scale factors24,
     * coefficient0=+128, remaining23 zero. No media input or golden decode
     * oracle: check real nonzero output, reset determinism and overlap state. */
    unsigned char nonzero_frame[128] = {0,0,4,0x20,7,0xc6,0x31,0x8c,0x63,0x18,0xc6,0x10};
    unsigned char first_pcm[512];
    ctrl.pEs=nonzero_frame;assert(!sceAudiodecClearContext(&ctrl));
    memset(guarded,0xa5,sizeof(guarded));
    assert(!decode(&ctrl) && ctrl.inputEsSize==39 && ctrl.outputPcmSize==512);
    unsigned nonzero_bytes=0;
    for(unsigned i=0;i<512;++i)nonzero_bytes+=guarded[16+i]!=0;
    assert(nonzero_bytes>0);memcpy(first_pcm,guarded+16,512);
    all_bytes(guarded,16,0xa5);all_bytes(guarded+528,sizeof(guarded)-528,0xa5);
    assert(!sceAudiodecClearContext(&ctrl));
    assert(!decode(&ctrl) && !memcmp(first_pcm,guarded+16,512));
    assert(!decode(&ctrl) && memcmp(first_pcm,guarded+16,512));
    all_bytes(guarded,16,0xa5);all_bytes(guarded+528,sizeof(guarded)-528,0xa5);
    assert(!sceAudiodecDeleteDecoder(&ctrl));assert(!sceAudiodecTermLibrary(SCE_AUDIODEC_TYPE_AT9));
}
static void gradient_endpoints(void)
{
    /* Legal six-bit gradient syntax may extend beyond the coded band. The
     * 31-entry buffer only needs its coded units; interpolation keeps the full
     * endpoint distance. UBSan catches the old array[31] write here. */
    GenerateGradientCurves();
    for (int start = 0; start < 48; ++start)
        for (int end = start; end < 48; ++end)
            for (int delta = -1; delta <= 1; ++delta) {
                Block block = {0};
                block.QuantizationUnitCount = 30;
                block.GradientStartUnit = start;
                block.GradientEndUnit = end;
                block.GradientStartValue = 15;
                block.GradientEndValue = 15 + delta;
                assert(!CreateGradient(&block));
                assert(block.QuantizationUnitCount == 30);
                assert(block.GradientStartUnit == start && block.GradientEndUnit == end);
                for (int unit = 0; unit <= 30; ++unit)
                    assert(block.Gradient[unit] >= 14 && block.Gradient[unit] <= 16);
            }
}
int main(void)
{
    gradient_endpoints();
    assert(sceAudiodecInitLibrary(SCE_AUDIODEC_TYPE_AT9,NULL)==ERR(SCE_AUDIODEC_ERROR_INVALID_PTR));
    SceAudiodecInitParam init={.at9={sizeof(init),2}};
    assert(sceAudiodecInitLibrary(SCE_AUDIODEC_TYPE_MP3,&init)<0);
    init.at9.size=0;assert(sceAudiodecInitLibrary(SCE_AUDIODEC_TYPE_AT9,&init)<0);
    init.at9.size=sizeof(init);init.at9.totalCh=0;
    assert(sceAudiodecInitLibrary(SCE_AUDIODEC_TYPE_AT9,&init)<0);
    init.at9.totalCh=17;assert(sceAudiodecInitLibrary(SCE_AUDIODEC_TYPE_AT9,&init)<0);
    SceAudiodecInfo info;SceAudiodecCtrl ctrl=control(&info);
    rejected_create(&ctrl,SCE_AUDIODEC_TYPE_AT9);
    assert(sceAudiodecGetContextSize(&ctrl,SCE_AUDIODEC_TYPE_AT9)>0); /* Pure query before library init. */
    init.at9.totalCh=2;assert(!sceAudiodecInitLibrary(SCE_AUDIODEC_TYPE_AT9,&init));
    assert(sceAudiodecInitLibrary(SCE_AUDIODEC_TYPE_AT9,&init)==ERR(SCE_AUDIODEC_ERROR_ALREADY_INITIALIZED));
    assert(sceAudiodecCreateDecoder(NULL,SCE_AUDIODEC_TYPE_AT9)<0);
    rejected_create(&ctrl,SCE_AUDIODEC_TYPE_AAC);
    ctrl.size=0;rejected_create(&ctrl,SCE_AUDIODEC_TYPE_AT9);ctrl.size=sizeof(ctrl);
    ctrl.wordLength=24;rejected_create(&ctrl,SCE_AUDIODEC_TYPE_AT9);ctrl.wordLength=16;
    info.at9.configData[0]=0;rejected_create(&ctrl,SCE_AUDIODEC_TYPE_AT9);ctrl=control(&info);
    info.at9.configData[1]=0x7c;rejected_create(&ctrl,SCE_AUDIODEC_TYPE_AT9);ctrl=control(&info); /* Reserved channel config6. */
    info.at9.configData[2]=0x80;info.at9.configData[3]=0;
    rejected_create(&ctrl,SCE_AUDIODEC_TYPE_AT9);ctrl=control(&info); /* 1025-byte frame exceeds bounded ABI. */
    info.size=0;rejected_create(&ctrl,SCE_AUDIODEC_TYPE_AT9);ctrl=control(&info);
    ctrl.pInfo=NULL;rejected_create(&ctrl,SCE_AUDIODEC_TYPE_AT9);ctrl=control(&info);
    SceAudiodecCtrl query_saved=ctrl;SceAudiodecInfo query_info=info;
    int context_size=sceAudiodecGetContextSize(&ctrl,SCE_AUDIODEC_TYPE_AT9);
    assert(context_size>0 && (unsigned)context_size%256==0 && context_size<=1024*1024);
    assert(!memcmp(&ctrl,&query_saved,sizeof(ctrl)) && !memcmp(&info,&query_info,sizeof(info)));
    assert(!sceAudiodecCreateDecoder(&ctrl,SCE_AUDIODEC_TYPE_AT9) && ctrl.handle>0);
    assert(info.at9.ch==2 && info.at9.samplingRate==48000 && info.at9.superFrameSize==4);
    assert(info.at9.framesInSuperFrame==1 && info.at9.bitRate==6000);
    assert(ctrl.maxEsSize==4 && ctrl.maxPcmSize==1024 && !ctrl.inputEsSize && !ctrl.outputPcmSize);
    rejected_create(&ctrl,SCE_AUDIODEC_TYPE_AT9);
    SceAudiodecInfo spare_info;SceAudiodecCtrl spare=control(&spare_info);
    rejected_create(&spare,SCE_AUDIODEC_TYPE_AT9); /* Stereo channel budget exhausted. */
    assert(sceAudiodecTermLibrary(SCE_AUDIODEC_TYPE_AT9)<0);
    SceUIntVAddr untouched=UINT32_C(0x12345678);
    assert(sceAudiodecDeleteDecoderExternal(&ctrl,&untouched)<0 && untouched==UINT32_C(0x12345678));
    unsigned char invalid_es[128];memset(invalid_es,0xff,sizeof(invalid_es));
    unsigned char guarded[16+8192+16];memset(guarded,0xa5,sizeof(guarded));
    ctrl.pEs=invalid_es;ctrl.pPcm=guarded+16;
    reject_decode(&ctrl,1);all_bytes(guarded,sizeof(guarded),0xa5);
    int32_t internal=0;assert(!sceAudiodecGetInternalError(&ctrl,&internal) && internal);
    assert(!sceAudiodecClearContext(&ctrl));internal=123;
    assert(!sceAudiodecGetInternalError(&ctrl,&internal) && !internal);
    ctrl.maxEsSize=4;ctrl.maxPcmSize=1024;reject_decode(&ctrl,2);
    all_bytes(guarded,sizeof(guarded),0xa5);
    assert(!sceAudiodecClearContext(&ctrl));reject_decode(&ctrl,8);
    all_bytes(guarded,sizeof(guarded),0xa5);
    reject_decode(&ctrl,0);reject_decode(&ctrl,9);all_bytes(guarded,sizeof(guarded),0xa5);
    ctrl.maxEsSize=3;reject_decode(&ctrl,1);
    ctrl.maxEsSize=1025;reject_decode(&ctrl,1);ctrl.maxEsSize=4;
    ctrl.maxPcmSize=1023;reject_decode(&ctrl,1);ctrl.maxPcmSize=1024;
    ctrl.pEs=NULL;reject_decode(&ctrl,1);ctrl.pEs=invalid_es;
    ctrl.pPcm=NULL;reject_decode(&ctrl,1);ctrl.pPcm=guarded+16;
    all_bytes(guarded,sizeof(guarded),0xa5);
    SceAudiodecCtrl stale=ctrl;
    assert(!sceAudiodecDeleteDecoder(&ctrl) && !ctrl.handle);
    assert(sceAudiodecDeleteDecoder(&stale)<0 && sceAudiodecClearContext(&stale)<0);
    internal=123;assert(sceAudiodecGetInternalError(&stale,&internal)<0 && internal==123);
    assert(!sceAudiodecTermLibrary(SCE_AUDIODEC_TYPE_AT9));
    assert(sceAudiodecTermLibrary(SCE_AUDIODEC_TYPE_AT9)<0);

    init.at9.totalCh=4;
    assert(!sceAudiodecInitLibrary(SCE_AUDIODEC_TYPE_AT9,&init));ctrl=control(&info);
    assert((uintptr_t)external_storage.data<=UINT32_MAX && !((uintptr_t)external_storage.data&255));
    SceUIntVAddr address=(SceUIntVAddr)(uintptr_t)external_storage.data;
    memset(&external_storage,0xa5,sizeof(external_storage));
    query_saved=ctrl;query_info=info;
    assert(sceAudiodecCreateDecoderExternal(&ctrl,SCE_AUDIODEC_TYPE_AT9,address+1,(unsigned)context_size)<0);
    assert(sceAudiodecCreateDecoderExternal(&ctrl,SCE_AUDIODEC_TYPE_AT9,address,(unsigned)context_size-1)<0);
    assert(!memcmp(&ctrl,&query_saved,sizeof(ctrl)) && !memcmp(&info,&query_info,sizeof(info)));
    ExternalAbi create_external=(ExternalAbi)module_get_func_export(0x2aa62046,0x56085dfb);assert(create_external);
    assert(!create_external(&ctrl,SCE_AUDIODEC_TYPE_AT9,address,(unsigned)context_size));
    spare=control(&spare_info);SceAudiodecCtrl spare_saved=spare;SceAudiodecInfo spare_info_saved=spare_info;
    unsigned char *context_snapshot=malloc((size_t)context_size);assert(context_snapshot);
    memcpy(context_snapshot,external_storage.data,(size_t)context_size);
    assert(create_external(&spare,SCE_AUDIODEC_TYPE_AT9,address,(unsigned)context_size)==ERR(SCE_AUDIODEC_ERROR_A_HANDLE_IN_USE));
    assert(!memcmp(&spare,&spare_saved,sizeof(spare)) && !memcmp(&spare_info,&spare_info_saved,sizeof(spare_info)));
    assert(!memcmp(context_snapshot,external_storage.data,(size_t)context_size));free(context_snapshot);
    all_bytes(external_storage.before,sizeof(external_storage.before),0xa5);
    all_bytes(external_storage.data+context_size,sizeof(external_storage.data)-(unsigned)context_size,0xa5);
    all_bytes(external_storage.after,sizeof(external_storage.after),0xa5);
    assert(sceAudiodecDeleteDecoder(&ctrl)<0);assert(!sceAudiodecClearContext(&ctrl));
    untouched=UINT32_C(0x12345678);
    assert(!sceAudiodecDeleteDecoderExternal(&ctrl,&untouched) && untouched==address && !ctrl.handle);
    all_bytes(external_storage.data+context_size,sizeof(external_storage.data)-(unsigned)context_size,0xa5);
    all_bytes(external_storage.before,sizeof(external_storage.before),0xa5);
    all_bytes(external_storage.after,sizeof(external_storage.after),0xa5);
    assert(!sceAudiodecTermLibrary(SCE_AUDIODEC_TYPE_AT9));
    owned_silence();
    init.at9.totalCh=2;assert(!sceAudiodecInitLibrary(SCE_AUDIODEC_TYPE_AT9,&init));ctrl=control(&info);
    assert(!sceAudiodecCreateDecoder(&ctrl,SCE_AUDIODEC_TYPE_AT9));
    SceAudiodec_finish();internal=123;
    assert(sceAudiodecGetInternalError(&ctrl,&internal)<0 && internal==123); /* Shutdown owns live internal handles. */
    puts("Audiodec SDK ABI, AT9 configuration, channel budgets, guarded failures and context ownership passed.");
}
