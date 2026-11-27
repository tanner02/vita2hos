#define TEST_THREADED_MUTEX
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <switch.h>
#include <psp2/ctrl.h>
typedef struct {int unused;} PadState;
typedef struct {int32_t x,y;} HidAnalogStickState;
enum { HidNpadButton_Minus=1<<0,HidNpadButton_StickL=1<<1,HidNpadButton_StickR=1<<2,
    HidNpadButton_Plus=1<<3,HidNpadButton_Up=1<<4,HidNpadButton_Right=1<<5,
    HidNpadButton_Down=1<<6,HidNpadButton_Left=1<<7,HidNpadButton_L=1<<8,
    HidNpadButton_R=1<<9,HidNpadButton_ZL=1<<10,HidNpadButton_ZR=1<<11,
    HidNpadButton_X=1<<12,HidNpadButton_A=1<<13,HidNpadButton_B=1<<14,
    HidNpadButton_Y=1<<15,HidNpadStyleSet_NpadStandard=1 };
static uint64_t native_buttons;
static HidAnalogStickState sticks[2];
static unsigned updates;
static void padConfigureInput(int count,int style){assert(count==1 && style==1);}
static void padInitializeAny(PadState *pad){(void)pad;}
static void padUpdate(PadState *pad){(void)pad;updates++;}
static uint64_t padGetButtons(PadState *pad){(void)pad;return native_buttons;}
static HidAnalogStickState padGetStickPos(PadState *pad,int side){(void)pad;return sticks[side];}
#include "../../source/modules/SceCtrl.c"
#include "../../source/module.c"
SceUInt64 sceKernelGetProcessTimeWide(void){return 123456789;}
int main(void)
{
    assert(!SceCtrl_init());
    SceCtrlPadInputMode mode=(SceCtrlPadInputMode)99;
    assert(!sceCtrlGetSamplingMode(&mode) && mode==SCE_CTRL_MODE_DIGITAL);
    SceCtrlData samples[2];memset(samples,0xa5,sizeof(samples));
    SceCtrlData sentinel=samples[1];
    sticks[0]=(HidAnalogStickState){-32768,32767};
    sticks[1]=(HidAnalogStickState){32767,-32768};
    native_buttons=HidNpadButton_B|HidNpadButton_L;
    assert(sceCtrlPeekBufferPositive(0,samples,64)==1);
    assert(samples[0].timeStamp==123456789 && samples[0].buttons==(SCE_CTRL_CROSS|SCE_CTRL_LTRIGGER));
    assert(samples[0].lx==128 && samples[0].ly==128 && samples[0].rx==128 && samples[0].ry==128);
    assert(!memcmp(&samples[1],&sentinel,sizeof(sentinel)));
    for(unsigned i=0;i<sizeof(samples[0].reserved);i++)assert(!samples[0].reserved[i]);
    assert(!samples[0].up && !samples[0].lt && !samples[0].cross);
    assert(sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG)==SCE_CTRL_MODE_DIGITAL);
    assert(sceCtrlPeekBufferPositive(0,samples,1)==1);
    assert(samples[0].lx==0 && samples[0].ly==0 && samples[0].rx==255 && samples[0].ry==255);
    sticks[0]=sticks[1]=(HidAnalogStickState){0,0};
    assert(sceCtrlPeekBufferPositive(0,samples,1)==1);
    assert(samples[0].lx==128 && samples[0].ly==128 && samples[0].rx==128 && samples[0].ry==128);
    assert(sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE)==SCE_CTRL_MODE_ANALOG);
    assert(sceCtrlSetSamplingMode((SceCtrlPadInputMode)-1)==(int)SCE_CTRL_ERROR_INVALID_ARG);
    assert(sceCtrlSetSamplingMode((SceCtrlPadInputMode)3)==(int)SCE_CTRL_ERROR_INVALID_ARG);
    assert(!sceCtrlGetSamplingMode(&mode) && mode==SCE_CTRL_MODE_ANALOG_WIDE);
    assert(sceCtrlGetSamplingMode(NULL)==(int)SCE_CTRL_ERROR_INVALID_ARG);
    SceCtrlData unchanged=samples[0];unsigned before=updates;
    assert(sceCtrlPeekBufferPositive(1,samples,1)==(int)SCE_CTRL_ERROR_INVALID_ARG);
    assert(sceCtrlPeekBufferPositive(0,NULL,1)==(int)SCE_CTRL_ERROR_INVALID_ARG);
    assert(sceCtrlPeekBufferPositive(0,samples,0)==(int)SCE_CTRL_ERROR_INVALID_ARG);
    assert(sceCtrlPeekBufferPositive(0,samples,65)==(int)SCE_CTRL_ERROR_INVALID_ARG);
    assert(updates==before && !memcmp(samples,&unchanged,sizeof(unchanged)));
    assert(ctrl_axis(INT32_MIN,false)==0 && ctrl_axis(INT32_MAX,false)==255);
    assert(ctrl_axis(INT32_MIN,true)==255 && ctrl_axis(INT32_MAX,true)==0);
    for(int x=-32768;x<32767;x++) {
        assert(ctrl_axis(x,false)<=ctrl_axis(x+1,false));
        assert(ctrl_axis(x,true)>=ctrl_axis(x+1,true));
    }
    assert(module_get_func_export(0xd197e3c7,0xa497b150));
    assert(module_get_func_export(0xd197e3c7,0xec752aaf));
    assert(module_get_func_export(0xd197e3c7,0xa9c3ced6));
    assert(!SceCtrl_init());assert(!sceCtrlGetSamplingMode(&mode) && mode==SCE_CTRL_MODE_DIGITAL);
    puts("Controller polling: modes, initialized sample, count, timestamps, bounds and monotonic stick mapping passed.");
}
