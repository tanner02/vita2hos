#include <psp2/ctrl.h>
#include <stdlib.h>
#include <string.h>
#include <psp2/kernel/processmgr.h>
#include <switch.h>

#include "log.h"
#include "module.h"
#include "util.h"

static PadState g_pad;
static Mutex g_ctrl_lock;
static SceCtrlPadInputMode g_sampling_mode=SCE_CTRL_MODE_DIGITAL;

EXPORT(SceCtrl,0xA497B150,int,sceCtrlSetSamplingMode,SceCtrlPadInputMode mode)
{
    if((unsigned)mode>(unsigned)SCE_CTRL_MODE_ANALOG_WIDE)
        return (int)SCE_CTRL_ERROR_INVALID_ARG;
    mutexLock(&g_ctrl_lock);
    int previous=g_sampling_mode;
    g_sampling_mode=mode;
    mutexUnlock(&g_ctrl_lock);
    return previous;
}
EXPORT(SceCtrl,0xEC752AAF,int,sceCtrlGetSamplingMode,SceCtrlPadInputMode *mode)
{
    if(!mode)return (int)SCE_CTRL_ERROR_INVALID_ARG;
    mutexLock(&g_ctrl_lock);
    *mode=g_sampling_mode;
    mutexUnlock(&g_ctrl_lock);
    return 0;
}
/* Normalize the native signed stick with clamped endpoints and exact center.
 * Both Vita analog modes currently use this full-range native mapping. */
static uint8_t ctrl_axis(int32_t value,bool invert)
{
    if(value < -32768)value=-32768;
    if(value > 32767)value=32767;
    if(invert) {
        value=-value;
        if(value>32767)value=32767;
    }
    return (uint8_t)(((uint32_t)(value+32768)*255+32767)/65535);
}

EXPORT(SceCtrl, 0xA9C3CED6, int, sceCtrlPeekBufferPositive, int port, SceCtrlData *pad_data,
       int count)
{
    if(port!=0 || !pad_data || count<1 || count>64)
        return (int)SCE_CTRL_ERROR_INVALID_ARG;
    SceCtrlData sample={0};
    sample.lx=sample.ly=sample.rx=sample.ry=128;
    mutexLock(&g_ctrl_lock);
    sample.timeStamp=sceKernelGetProcessTimeWide();
    uint64_t buttons;
    HidAnalogStickState analog_stick_l, analog_stick_r;
    uint32_t vita_buttons = 0;

    padUpdate(&g_pad);
    buttons = padGetButtons(&g_pad);
    analog_stick_l = padGetStickPos(&g_pad, 0);
    analog_stick_r = padGetStickPos(&g_pad, 1);

    if (buttons & HidNpadButton_Minus)
        vita_buttons |= SCE_CTRL_SELECT;
    if (buttons & HidNpadButton_StickL)
        vita_buttons |= SCE_CTRL_L3;
    if (buttons & HidNpadButton_StickR)
        vita_buttons |= SCE_CTRL_R3;
    if (buttons & HidNpadButton_Plus)
        vita_buttons |= SCE_CTRL_START;
    if (buttons & HidNpadButton_Up)
        vita_buttons |= SCE_CTRL_UP;
    if (buttons & HidNpadButton_Right)
        vita_buttons |= SCE_CTRL_RIGHT;
    if (buttons & HidNpadButton_Down)
        vita_buttons |= SCE_CTRL_DOWN;
    if (buttons & HidNpadButton_Left)
        vita_buttons |= SCE_CTRL_LEFT;
    if (buttons & HidNpadButton_L)
        vita_buttons |= SCE_CTRL_LTRIGGER;
    if (buttons & HidNpadButton_R)
        vita_buttons |= SCE_CTRL_RTRIGGER;
    if (buttons & HidNpadButton_ZL)
        vita_buttons |= SCE_CTRL_L1;
    if (buttons & HidNpadButton_ZR)
        vita_buttons |= SCE_CTRL_R1;
    if (buttons & HidNpadButton_X)
        vita_buttons |= SCE_CTRL_TRIANGLE;
    if (buttons & HidNpadButton_A)
        vita_buttons |= SCE_CTRL_CIRCLE;
    if (buttons & HidNpadButton_B)
        vita_buttons |= SCE_CTRL_CROSS;
    if (buttons & HidNpadButton_Y)
        vita_buttons |= SCE_CTRL_SQUARE;

    sample.buttons = vita_buttons;
    if(g_sampling_mode!=SCE_CTRL_MODE_DIGITAL) {
        sample.lx=ctrl_axis(analog_stick_l.x,false);
        sample.ly=ctrl_axis(analog_stick_l.y,true);
        sample.rx=ctrl_axis(analog_stick_r.x,false);
        sample.ry=ctrl_axis(analog_stick_r.y,true);
    }
    mutexUnlock(&g_ctrl_lock);
    /* This polling implementation returns one current sample, even when the
     * caller offers more space. It does not invent a historical sample ring. */
    memcpy(pad_data,&sample,sizeof(sample));
    return 1;
}

DECLARE_LIBRARY(SceCtrl, 0xD197E3C7);

int SceCtrl_init(void)
{
    mutexLock(&g_ctrl_lock);
    g_sampling_mode=SCE_CTRL_MODE_DIGITAL;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeAny(&g_pad);
    mutexUnlock(&g_ctrl_lock);

    return 0;
}
