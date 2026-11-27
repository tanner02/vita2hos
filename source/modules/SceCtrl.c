#include <psp2/ctrl.h>
#include <psp2/kernel/processmgr.h>
#include <switch.h>
#include "log.h"
#include "module.h"
#include "modules/SceCtrl.h"
#define CTRL_HISTORY 64
#define CTRL_SAMPLE_PERIOD_NS UINT64_C(16666667)
#define CTRL_TIMEOUT_RESULT 0xEA01u
#define CTRL_JOIN_TIMEOUT_NS UINT64_C(1000000000)
static PadState g_pad;
static Mutex g_ctrl_lock;
static CondVar g_ctrl_changed;
static UEvent g_ctrl_stop;
static Thread g_ctrl_sampler;
static bool g_ctrl_sync_ready,g_ctrl_ready,g_ctrl_created,g_ctrl_started,g_ctrl_joining;
static uint64_t g_ctrl_written,g_ctrl_read,g_ctrl_generation;
static SceCtrlData g_ctrl_history[CTRL_HISTORY];
static SceCtrlPadInputMode g_sampling_mode=SCE_CTRL_MODE_DIGITAL;
EXPORT(SceCtrl,0xA497B150,int,sceCtrlSetSamplingMode,SceCtrlPadInputMode mode)
{
    if((unsigned)mode>(unsigned)SCE_CTRL_MODE_ANALOG_WIDE)return (int)SCE_CTRL_ERROR_INVALID_ARG;
    mutexLock(&g_ctrl_lock);
    int previous=g_ctrl_ready ? (int)g_sampling_mode : (int)SCE_CTRL_ERROR_FATAL;
    if(g_ctrl_ready)g_sampling_mode=mode;
    mutexUnlock(&g_ctrl_lock);return previous;
}
EXPORT(SceCtrl,0xEC752AAF,int,sceCtrlGetSamplingMode,SceCtrlPadInputMode *mode)
{
    if(!mode)return (int)SCE_CTRL_ERROR_INVALID_ARG;
    mutexLock(&g_ctrl_lock);
    int result=g_ctrl_ready ? 0 : (int)SCE_CTRL_ERROR_FATAL;
    if(!result)*mode=g_sampling_mode;
    mutexUnlock(&g_ctrl_lock);return result;
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

static void ctrl_capture_locked(void)
{
    SceCtrlData sample={0};
    sample.lx=sample.ly=sample.rx=sample.ry=128;
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
    g_ctrl_history[g_ctrl_written%CTRL_HISTORY]=sample;
    ++g_ctrl_written;
    condvarWakeAll(&g_ctrl_changed);
}
static void ctrl_sampler(void *argument)
{
    (void)argument;
    for(;;) {
        Result result=waitSingle(waiterForUEvent(&g_ctrl_stop),CTRL_SAMPLE_PERIOD_NS);
        if(!result)return;
        mutexLock(&g_ctrl_lock);
        if(result!=CTRL_TIMEOUT_RESULT || !g_ctrl_ready) {
            g_ctrl_ready=false;++g_ctrl_generation;condvarWakeAll(&g_ctrl_changed);
            mutexUnlock(&g_ctrl_lock);
            if(result!=CTRL_TIMEOUT_RESULT)LOG("Controller sampler wait failed: 0x%x",result);
            return;
        }
        ctrl_capture_locked();mutexUnlock(&g_ctrl_lock);
    }
}
static int ctrl_buffer(int port,SceCtrlData *data,int count,bool peek)
{
    if(port!=0 || !data || count<1 || count>CTRL_HISTORY)return (int)SCE_CTRL_ERROR_INVALID_ARG;
    mutexLock(&g_ctrl_lock);
    uint64_t generation=g_ctrl_generation;
    while(!peek && g_ctrl_ready && g_ctrl_written==g_ctrl_read && generation==g_ctrl_generation) {
        if(R_FAILED(condvarWaitTimeout(&g_ctrl_changed,&g_ctrl_lock,UINT64_MAX))) {
            mutexUnlock(&g_ctrl_lock);return (int)SCE_CTRL_ERROR_FATAL;
        }
    }
    if(!g_ctrl_ready || generation!=g_ctrl_generation) {
        mutexUnlock(&g_ctrl_lock);return (int)SCE_CTRL_ERROR_FATAL;
    }
    uint64_t available=peek ? g_ctrl_written : g_ctrl_written-g_ctrl_read;
    if(available>CTRL_HISTORY)available=CTRL_HISTORY;
    if(available>(unsigned)count)available=(unsigned)count;
    /* Return newest first. A read consumes the current batch, including older
     * samples the caller did not request, so a count-one reader cannot lag. */
    for(unsigned i=0;i<(unsigned)available;i++)data[i]=g_ctrl_history[(g_ctrl_written-1-i)%CTRL_HISTORY];
    if(!peek)g_ctrl_read=g_ctrl_written;
    mutexUnlock(&g_ctrl_lock);return (int)available;
}
EXPORT(SceCtrl,0xA9C3CED6,int,sceCtrlPeekBufferPositive,int port,SceCtrlData *data,int count)
{return ctrl_buffer(port,data,count,true);}
EXPORT(SceCtrl,0x67E7AB83,int,sceCtrlReadBufferPositive,int port,SceCtrlData *data,int count)
{return ctrl_buffer(port,data,count,false);}
DECLARE_LIBRARY(SceCtrl,0xD197E3C7);
int SceCtrl_init(void)
{
    mutexLock(&g_ctrl_lock);
    if(g_ctrl_created) {
        int result=g_ctrl_ready ? 0 : (int)SCE_CTRL_ERROR_FATAL;
        mutexUnlock(&g_ctrl_lock);return result;
    }
    if(!g_ctrl_sync_ready){condvarInit(&g_ctrl_changed);g_ctrl_sync_ready=true;}
    ueventCreate(&g_ctrl_stop,true);
    ++g_ctrl_generation;g_ctrl_written=g_ctrl_read=0;g_sampling_mode=SCE_CTRL_MODE_DIGITAL;
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);padInitializeAny(&g_pad);
    g_ctrl_ready=true;g_ctrl_started=false;ctrl_capture_locked();
    Result result=threadCreate(&g_ctrl_sampler,ctrl_sampler,NULL,NULL,0x4000,28,-2);
    if(R_FAILED(result)) {
        g_ctrl_ready=false;++g_ctrl_generation;condvarWakeAll(&g_ctrl_changed);
        mutexUnlock(&g_ctrl_lock);return (int)SCE_CTRL_ERROR_FATAL;
    }
    g_ctrl_created=true;result=threadStart(&g_ctrl_sampler);
    if(R_FAILED(result)) {
        g_ctrl_ready=false;++g_ctrl_generation;condvarWakeAll(&g_ctrl_changed);
        if(!R_FAILED(threadClose(&g_ctrl_sampler)))g_ctrl_created=false;
        mutexUnlock(&g_ctrl_lock);return (int)SCE_CTRL_ERROR_FATAL;
    }
    g_ctrl_started=true;mutexUnlock(&g_ctrl_lock);return 0;
}
int SceCtrl_finish(void)
{
    mutexLock(&g_ctrl_lock);
    if(!g_ctrl_created){mutexUnlock(&g_ctrl_lock);return 0;}
    if(g_ctrl_joining){mutexUnlock(&g_ctrl_lock);return (int)SCE_CTRL_ERROR_FATAL;}
    g_ctrl_joining=true;g_ctrl_ready=false;++g_ctrl_generation;
    ueventSignal(&g_ctrl_stop);condvarWakeAll(&g_ctrl_changed);
    bool started=g_ctrl_started;mutexUnlock(&g_ctrl_lock);
    Result result=started ? waitSingle(waiterForThread(&g_ctrl_sampler),CTRL_JOIN_TIMEOUT_NS) : 0;
    if(!R_FAILED(result))result=threadClose(&g_ctrl_sampler);
    mutexLock(&g_ctrl_lock);g_ctrl_joining=false;
    if(!R_FAILED(result)){g_ctrl_created=false;g_ctrl_started=false;}
    mutexUnlock(&g_ctrl_lock);
    if(R_FAILED(result))LOG("Controller sampler cleanup failed: 0x%x",result);
    return R_FAILED(result) ? (int)SCE_CTRL_ERROR_FATAL : 0;
}
