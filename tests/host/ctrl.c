#define TEST_THREADED_MUTEX
#define TEST_THREAD_RUNTIME
#define TEST_THREAD_RUNTIME_PTHREAD
#include <assert.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sched.h>
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
static bool native_connected,trace_flag;
static unsigned trace_logs;
static char last_native_trace[512],last_delivery_trace[512];
#define CTRL_TRACE_FLAG "/codex-owned-controller-trace-test.enabled"
static FILE *trace_fopen(const char *path,const char *mode)
{
    assert(!strcmp(path,CTRL_TRACE_FLAG) && !strcmp(mode,"rb"));
    if(!trace_flag)return NULL;
    FILE *file=tmpfile();assert(file);return file;
}
static HidAnalogStickState sticks[2];
static unsigned updates;
static void padConfigureInput(int count,int style){assert(count==1 && style==1);}
static void padInitializeAny(PadState *pad){(void)pad;}
static void padUpdate(PadState *pad){(void)pad;updates++;}
static uint64_t padGetButtons(PadState *pad){(void)pad;return native_buttons;}
static bool padIsConnected(const PadState *pad){(void)pad;return native_connected;}
static HidAnalogStickState padGetStickPos(PadState *pad,int side){(void)pad;return sticks[side];}
static atomic_uint permits,event_failure,condition_waits;
static atomic_ullong sample_time;
static unsigned fail_create,fail_start,fail_wait,fail_close,create_calls,close_calls,join_calls;
static Result observed_cond_wait(CondVar *condition,Mutex *mutex,uint64_t ns);
#define condvarWaitTimeout observed_cond_wait
#define fopen trace_fopen
#include "../../source/modules/SceCtrl.c"
#undef fopen
#undef condvarWaitTimeout
#include "../../source/module.c"
static Result observed_cond_wait(CondVar *condition,Mutex *mutex,uint64_t ns)
{atomic_fetch_add(&condition_waits,1);return condvarWaitTimeout(condition,mutex,ns);}
SceUInt64 sceKernelGetProcessTimeWide(void){return atomic_fetch_add(&sample_time,1000)+1000;}
void log_printf(const char *format,...)
{
    if(strncmp(format,"CTRL_",5))return;
    ++trace_logs;
    char line[512];va_list args;va_start(args,format);vsnprintf(line,sizeof(line),format,args);va_end(args);
    if(!strncmp(format,"CTRL_NATIVE",11))snprintf(last_native_trace,sizeof(last_native_trace),"%s",line);
    if(!strncmp(format,"CTRL_DELIVERY",13))snprintf(last_delivery_trace,sizeof(last_delivery_trace),"%s",line);
}
static void *native_entry(void *argument)
{Thread *thread=argument;thread->entry(thread->argument);return NULL;}
Result threadCreate(Thread *thread,void (*entry)(void *),void *argument,void *stack,size_t size,int priority,int cpu)
{
    (void)stack;assert(size==0x4000 && priority==28 && cpu==-2);create_calls++;
    if(fail_create){fail_create--;return 1;}
    *thread=(Thread){.entry=entry,.argument=argument};return 0;
}
Result threadStart(Thread *thread)
{
    if(fail_start){fail_start--;return 1;}
    int result=pthread_create(&thread->native,NULL,native_entry,thread);
    if(!result)thread->started=true;
    return result;
}
Result threadClose(Thread *thread)
{
    assert(!thread->started || thread->joined);close_calls++;
    if(fail_close){fail_close--;return 1;}return 0;
}
Result waitSingle(Waiter waiter,uint64_t ns)
{
    if(waiter.thread) {
        assert(ns==CTRL_JOIN_TIMEOUT_NS);
        if(fail_wait){fail_wait--;return 1;}
        if(!waiter.thread->joined){assert(!pthread_join(waiter.thread->native,NULL));waiter.thread->joined=true;join_calls++;}
        return 0;
    }
    assert(waiter.event && ns==CTRL_SAMPLE_PERIOD_NS);
    for(;;) {
        if(__atomic_exchange_n(waiter.event,0,__ATOMIC_SEQ_CST))return 0;
        unsigned failure=atomic_exchange(&event_failure,0);if(failure)return failure;
        unsigned count=atomic_load(&permits);
        if(count && atomic_compare_exchange_weak(&permits,&count,count-1))return CTRL_TIMEOUT_RESULT;
        sched_yield();
    }
}
static uint64_t serial(void)
{mutexLock(&g_ctrl_lock);uint64_t result=g_ctrl_written;mutexUnlock(&g_ctrl_lock);return result;}
static void tick(void)
{
    uint64_t before=serial(),start=armGetSystemTick();atomic_fetch_add(&permits,1);
    while(serial()==before){assert(armTicksToNs(armGetSystemTick()-start)<UINT64_C(5000000000));sched_yield();}
}
typedef struct {SceCtrlData data[65];int count,result;atomic_bool done;} Reader;
static void *read_input(void *argument)
{
    Reader *reader=argument;
    reader->result=sceCtrlReadBufferPositive(0,reader->data,reader->count);
    atomic_store(&reader->done,true);return NULL;
}
static pthread_t start_reader(Reader *reader,int count)
{
    memset(reader->data,0xa5,sizeof(reader->data));reader->count=count;reader->result=-1;atomic_init(&reader->done,false);
    unsigned before=atomic_load(&condition_waits);pthread_t thread;
    assert(!pthread_create(&thread,NULL,read_input,reader));uint64_t start=armGetSystemTick();
    while(atomic_load(&condition_waits)==before){assert(armTicksToNs(armGetSystemTick()-start)<UINT64_C(5000000000));sched_yield();}
    assert(!atomic_load(&reader->done));return thread;
}
int main(void)
{
    assert(!SceCtrl_finish());assert(!SceCtrl_init());
    SceCtrlPadInputMode mode=99;assert(!sceCtrlGetSamplingMode(&mode) && mode==SCE_CTRL_MODE_DIGITAL);
    SceCtrlData samples[65];memset(samples,0xa5,sizeof(samples));SceCtrlData sentinel=samples[64];
    sticks[0]=(HidAnalogStickState){-32768,32767};sticks[1]=(HidAnalogStickState){32767,-32768};
    native_buttons=HidNpadButton_B|HidNpadButton_L;tick();
    assert(sceCtrlPeekBufferPositive(0,samples,64)==2);
    assert(samples[0].timeStamp==2000 && samples[1].timeStamp==1000);
    assert(samples[0].buttons==(SCE_CTRL_CROSS|SCE_CTRL_LTRIGGER) && !samples[1].buttons);
    assert(samples[0].lx==128 && samples[0].ly==128 && samples[0].rx==128 && samples[0].ry==128);
    assert(!memcmp(&samples[64],&sentinel,sizeof(sentinel)));
    for(unsigned i=0;i<sizeof(samples[0].reserved);i++)assert(!samples[0].reserved[i]);
    assert(!samples[0].up && !samples[0].lt && !samples[0].cross);
    assert(sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG)==SCE_CTRL_MODE_DIGITAL);tick();
    assert(sceCtrlPeekBufferPositive(0,samples,1)==1);
    assert(samples[0].lx==0 && samples[0].ly==0 && samples[0].rx==255 && samples[0].ry==255);
    assert(sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE)==SCE_CTRL_MODE_ANALOG);
    sticks[0]=sticks[1]=(HidAnalogStickState){0,0};tick();
    assert(sceCtrlPeekBufferPositive(0,samples,1)==1 && samples[0].lx==128 && samples[0].ly==128);
    assert(sceCtrlSetSamplingMode((SceCtrlPadInputMode)-1)==(int)SCE_CTRL_ERROR_INVALID_ARG);
    assert(sceCtrlSetSamplingMode((SceCtrlPadInputMode)3)==(int)SCE_CTRL_ERROR_INVALID_ARG);
    assert(!sceCtrlGetSamplingMode(&mode) && mode==SCE_CTRL_MODE_ANALOG_WIDE);
    assert(sceCtrlGetSamplingMode(NULL)==(int)SCE_CTRL_ERROR_INVALID_ARG);
    unsigned before=updates;SceCtrlData unchanged=samples[0];
    assert(sceCtrlPeekBufferPositive(1,samples,1)==(int)SCE_CTRL_ERROR_INVALID_ARG);
    assert(sceCtrlPeekBufferPositive(0,NULL,1)==(int)SCE_CTRL_ERROR_INVALID_ARG);
    assert(sceCtrlPeekBufferPositive(0,samples,0)==(int)SCE_CTRL_ERROR_INVALID_ARG);
    assert(sceCtrlReadBufferPositive(0,samples,65)==(int)SCE_CTRL_ERROR_INVALID_ARG);
    assert(updates==before && !memcmp(&samples[0],&unchanged,sizeof(unchanged)));
    assert(sceCtrlReadBufferPositive(0,samples,1)==1);
    Reader reader;pthread_t waiting=start_reader(&reader,64);
    assert(sceCtrlPeekBufferPositive(0,samples,64)==4 && !atomic_load(&reader.done));
    tick();assert(!pthread_join(waiting,NULL));assert(reader.result==1);
    assert(!memcmp(&reader.data[1],&sentinel,sizeof(sentinel)));
    for(unsigned i=0;i<100;i++){native_buttons=(i&1) ? HidNpadButton_B : 0;tick();}
    assert(sceCtrlReadBufferPositive(0,samples,64)==64);
    uint64_t newest=atomic_load(&sample_time);
    for(unsigned i=0;i<64;i++) {
        assert(samples[i].timeStamp==newest-i*1000);
        assert(samples[i].buttons==((i&1) ? 0 : SCE_CTRL_CROSS));
    }
    waiting=start_reader(&reader,1);
    assert(sceCtrlPeekBufferPositive(0,samples,64)==64 && !atomic_load(&reader.done));
    tick();assert(!pthread_join(waiting,NULL) && reader.result==1);
    Reader first,second;
    pthread_t first_thread=start_reader(&first,1),second_thread=start_reader(&second,1);
    tick();uint64_t start=armGetSystemTick();
    while(!atomic_load(&first.done) && !atomic_load(&second.done)) {
        assert(armTicksToNs(armGetSystemTick()-start)<UINT64_C(5000000000));sched_yield();
    }
    assert(!(atomic_load(&first.done) && atomic_load(&second.done)));
    tick();assert(!pthread_join(first_thread,NULL) && !pthread_join(second_thread,NULL));
    assert(first.result==1 && second.result==1 && first.data[0].timeStamp!=second.data[0].timeStamp);
    assert(sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG)==SCE_CTRL_MODE_ANALOG_WIDE);
    sticks[0]=(HidAnalogStickState){-32768,32767};tick();
    assert(sceCtrlSetSamplingMode(SCE_CTRL_MODE_DIGITAL)==SCE_CTRL_MODE_ANALOG);tick();
    assert(sceCtrlPeekBufferPositive(0,samples,2)==2 && samples[0].lx==128 && samples[1].lx==0);
    assert(sceCtrlReadBufferPositive(0,samples,1)==1);
    waiting=start_reader(&reader,1);assert(!SceCtrl_finish());assert(!SceCtrl_init());
    assert(!pthread_join(waiting,NULL) && reader.result==(int)SCE_CTRL_ERROR_FATAL);
    assert(!memcmp(&reader.data[0],&sentinel,sizeof(sentinel)));
    assert(!sceCtrlGetSamplingMode(&mode) && mode==SCE_CTRL_MODE_DIGITAL);
    assert(sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG)==SCE_CTRL_MODE_DIGITAL);
    before=create_calls;assert(!SceCtrl_init() && create_calls==before);
    assert(!sceCtrlGetSamplingMode(&mode) && mode==SCE_CTRL_MODE_ANALOG);assert(!SceCtrl_finish());
    before=close_calls;fail_create=1;assert(SceCtrl_init()==(int)SCE_CTRL_ERROR_FATAL && !g_ctrl_created && close_calls==before);
    before=join_calls;fail_start=1;assert(SceCtrl_init()==(int)SCE_CTRL_ERROR_FATAL && !g_ctrl_created && join_calls==before);
    fail_start=fail_close=1;assert(SceCtrl_init()==(int)SCE_CTRL_ERROR_FATAL && g_ctrl_created && !g_ctrl_started);
    assert(SceCtrl_init()==(int)SCE_CTRL_ERROR_FATAL);assert(!SceCtrl_finish() && join_calls==before);
    assert(!SceCtrl_init());fail_wait=1;assert(SceCtrl_finish()==(int)SCE_CTRL_ERROR_FATAL && g_ctrl_created);
    assert(SceCtrl_init()==(int)SCE_CTRL_ERROR_FATAL);assert(!SceCtrl_finish());
    assert(!SceCtrl_init());fail_close=1;assert(SceCtrl_finish()==(int)SCE_CTRL_ERROR_FATAL && g_ctrl_created);
    assert(!SceCtrl_finish());
    assert(!SceCtrl_init());assert(sceCtrlReadBufferPositive(0,samples,1)==1);
    waiting=start_reader(&reader,1);atomic_store(&event_failure,0x1234);
    assert(!pthread_join(waiting,NULL) && reader.result==(int)SCE_CTRL_ERROR_FATAL);assert(!SceCtrl_finish());
    for(int x=-32768;x<32767;x++){assert(ctrl_axis(x,false)<=ctrl_axis(x+1,false));assert(ctrl_axis(x,true)>=ctrl_axis(x+1,true));}
    assert(ctrl_axis(INT32_MIN,false)==0 && ctrl_axis(INT32_MAX,true)==0);
    assert(module_get_func_export(0xd197e3c7,0x67e7ab83));
    assert(!SceCtrl_finish());
    assert(!trace_logs);
    trace_flag=true;native_connected=false;native_buttons=0;
    assert(!SceCtrl_init() && g_ctrl_trace.enabled && trace_logs==1);
    assert(strstr(last_native_trace,"connected=0") && strstr(last_native_trace,"guest_polls=0"));
    native_connected=true;native_buttons=HidNpadButton_B;tick();
    assert(strstr(last_native_trace,"connected=1") && g_ctrl_trace.native_buttons==HidNpadButton_B);
    assert(sceCtrlPeekBufferPositive(0,samples,1)==1 && samples[0].buttons==SCE_CTRL_CROSS);
    char cross[48];snprintf(cross,sizeof(cross),"buttons=0x%08x",SCE_CTRL_CROSS);
    assert(strstr(last_delivery_trace,cross) && g_ctrl_trace.polls==1);
    native_buttons=0;tick();
    assert(sceCtrlReadBufferPositive(0,samples,1)==1 && !samples[0].buttons);
    assert(strstr(last_delivery_trace,"buttons=0x00000000"));
    before=trace_logs;atomic_fetch_add(&sample_time,CTRL_TRACE_PERIOD_US);tick();
    assert(trace_logs==before+1 && strstr(last_native_trace,"guest_polls=2"));
    for(unsigned i=0;i<CTRL_TRACE_LIMIT+10;i++) {
        native_buttons=(i&1)?HidNpadButton_B:0;tick();
        assert(sceCtrlPeekBufferPositive(0,samples,1)==1);
        assert(samples[0].buttons==((i&1)?SCE_CTRL_CROSS:0));
    }
    assert(trace_logs==CTRL_TRACE_LIMIT && g_ctrl_trace.records==CTRL_TRACE_LIMIT);
    assert(g_ctrl_trace.polls==CTRL_TRACE_LIMIT+12);
    assert(!SceCtrl_finish() && !g_ctrl_trace.enabled);
    trace_flag=false;assert(!SceCtrl_init() && !g_ctrl_trace.enabled);tick();
    assert(sceCtrlPeekBufferPositive(0,samples,1)==1 && trace_logs==CTRL_TRACE_LIMIT);
    assert(!SceCtrl_finish());
    puts("Controller diagnostics: opt-in presence/native buttons, actual guest delivery, periodic poll counts, bounded records and continued sampling passed.");
    puts("Controller buffering: real history, fresh blocking reads, overrun, mode transitions, shutdown generations and native failure ownership passed.");
}
