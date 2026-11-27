#define TEST_THREAD_RUNTIME
#include <setjmp.h>
#include <assert.h>
#include <stdio.h>

#include "../../source/guest_tls.c"
#include "../../source/thread_events.c"
#include "../../source/modules/SceKernelThreadMgr.c"
#include "../../source/module.c"
DECLARE_LIBRARY(SceLibKernel,0xcae9ace6);

static jmp_buf thread_return;
static void *tls_value;
static Result start_result, wait_result, priority_result;
static uint32_t changed_handle, changed_priority;
static unsigned priority_changes;
Result svcSetThreadPriority(uint32_t handle, uint32_t priority)
{ changed_handle=handle;changed_priority=priority;++priority_changes;return priority_result; }
static UEvent exit_event;
static int process_exit_status;
static uintptr_t writable_tp;
static unsigned tp_exchanges;
static bool expect_guest_tp;
static size_t last_stack;
static int last_priority;
static unsigned create_calls;
static int event_calls[8],event_count;
static bool exit_in_start,exit_in_end;
static int event_callback(SceInt32 type,SceUID uid,SceInt32 arg,void *common)
{
    VitaThreadInfo *info=SceKernelThreadMgr_get_thread_info();
    assert(info && uid==info->uid && !arg && common==&event_count);
    assert(writable_tp==(uintptr_t)info->vita_user_tls);
    event_calls[event_count++]=type;
    if ((type==4 && exit_in_start) || (type==8 && exit_in_end)) sceKernelExitThread(77);
    return 0;
}
uintptr_t guest_tp_exchange(uintptr_t value)
{
    uintptr_t previous=writable_tp;writable_tp=value;++tp_exchanges;return previous;
}
void log_printf(const char *format, ...) { (void)format; }
SceUID SceSysmem_get_next_uid(void) { static SceUID next = 1; return next++; }
UEvent *SceLibKernel_get_process_exit_uevent(void) { return &exit_event; }
int SceLibKernel_get_process_exit_res(void) { return process_exit_status; }
bool SceLibKernel_process_exit_requested(void) { return false; }
Result threadCreate(Thread *thread, void (*entry)(void *), void *arg, void *stack,
                    size_t size, int priority, int cpu)
{
    (void)stack; (void)cpu;
    last_stack=size;last_priority=priority;++create_calls;
    thread->handle = 0x1234;
    thread->entry = entry;
    thread->argument = arg;
    return 0;
}
Result threadStart(Thread *thread) { (void)thread; return start_result; }
Result threadClose(Thread *thread) { (void)thread; return 0; }
int threadTlsAlloc(void (*destructor)(void *)) { (void)destructor; return 1; }
void threadTlsFree(int slot) { (void)slot; }
void threadTlsSet(int slot, void *value) { (void)slot; tls_value = value; }
void *threadTlsGet(int slot) { (void)slot; return tls_value; }
Result waitSingle(Waiter waiter, uint64_t ns) { (void)waiter; (void)ns; return wait_result; }
void svcSleepThread(s64 ns) { (void)ns; }
void threadExit(void) { longjmp(thread_return, 1); }

static int entry(SceSize size, void *argument)
{
    assert(size == 4 && memcmp(argument, "test", 4) == 0);
    if (expect_guest_tp)
        assert(writable_tp == (uintptr_t)SceKernelThreadMgr_get_thread_info()->vita_user_tls);
    return 1234;
}

int main(void)
{
    int (*raw_priority)(void)=(void *)module_get_func_export(0x859a24b1,0x01414F0B);
    assert(raw_priority);
    GuestTlsTemplate tls={0}; uint32_t seed=0x12345678;
    assert(guest_tls_template_create(&tls, &seed, 4, 16) == 0);
    seed=0; /* Threads use the immutable snapshot, not mutable source storage. */
    assert(SceKernelThreadMgr_init(&tls, false) == 0);
    assert((uint32_t)sceKernelGetThreadCurrentPriority() == SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    assert((uint32_t)raw_priority()==SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    assert(resolve_vita_priority(SCE_KERNEL_DEFAULT_PRIORITY) == 160);
    assert(resolve_vita_priority(SCE_KERNEL_DEFAULT_PRIORITY + 3) == 163);
    assert(vita_priority_to_hos_priority(64) == vita_priority_to_hos_priority(65));
    assert((uint32_t)resolve_vita_priority(63) == SCE_KERNEL_ERROR_ILLEGAL_PRIORITY);
    assert((uint32_t)sceKernelStartThread(-1, 0, NULL) == SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    assert((uint32_t)sceKernelDeleteThread(-1) == SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    assert(sceKernelGetThreadTLSAddr(-1, 0) == NULL);
    assert((uint32_t)create_thread(NULL, entry, 160, 4096) == SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    SceUID uid = create_thread("worker", entry, SCE_KERNEL_DEFAULT_PRIORITY, 4096);
    assert(uid > 0);
    VitaThreadInfo *info = get_thread_info_for_uid(uid);
    assert(info->priority == 160);
    for (unsigned int i = 0; i < SCE_KERNEL_TLS_SIZE; i++)
        assert(((unsigned char *)info->vita_tls)[i] == (i==32 ? 160 : 0));
    assert(info->vita_user_tls == (char *)info->vita_tls + GUEST_KERNEL_TLS_SIZE);
    assert(*(uint32_t *)info->vita_user_tls == 0x12345678);
    for (int i=4; i<16; ++i) assert(((unsigned char *)info->vita_user_tls)[i] == 0);
    assert(sceKernelGetThreadTLSAddr(uid, 0x100) == (char *)info->vita_tls + 0x400);
    *(uint32_t *)info->vita_user_tls=99;
    SceUID other=create_thread("independent TLS", entry, 160, 4096);
    assert(other > 0);
    int (*change)(SceUID,int)=(void *)module_get_func_export(0x859a24b1,0xBD0139F2);
    int (*change2)(SceUID,int)=(void *)module_get_func_export(0x859a24b1,0xC5FFCA61);
    assert(change && change2);
    assert((uint32_t)change(0,100)==SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    assert((uint32_t)change(-1,100)==SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    assert(!priority_changes);
    tls_value=info;
    assert(change2(0,64)==160);
    assert(changed_handle==0x1234 && changed_priority==28);
    assert(info->priority==64 && info->vita_tls[8]==64 && raw_priority()==64);
    assert(change2(uid,65)==64 && changed_priority==28 && raw_priority()==65);
    assert(change(other,0)==0);
    VitaThreadInfo *other_info=get_thread_info_for_uid(other);
    assert(other_info->priority==65 && other_info->vita_tls[8]==65);
    unsigned calls=priority_changes;
    assert((uint32_t)change(uid,63)==SCE_KERNEL_ERROR_ILLEGAL_PRIORITY);
    assert((uint32_t)change(uid,192)==SCE_KERNEL_ERROR_ILLEGAL_PRIORITY);
    assert(priority_changes==calls && info->priority==65);
    priority_result=1;
    assert((uint32_t)change(uid,100)==SCE_KERNEL_ERROR_THREAD_ERROR);
    assert(info->priority==65 && info->vita_tls[8]==65);
    priority_result=0;
    assert(change2(uid,SCE_KERNEL_DEFAULT_PRIORITY+3)==65 && raw_priority()==163);
    assert(!change(uid,160));
    tls_value=NULL;

    assert(*(uint32_t *)get_thread_info_for_uid(other)->vita_user_tls == 0x12345678);
    assert(sceKernelDeleteThread(other) == 0);
    assert((uint32_t)start_thread(uid, 4, NULL) == SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    start_result = 1;
    assert((uint32_t)start_thread(uid, 4, "test") == SCE_KERNEL_ERROR_THREAD_ERROR);
    assert(info->argp == NULL && info->arglen == 0);
    start_result = 0;
    assert(start_thread(uid, 4, "test") == 0);
    void *saved_arguments = info->argp;
    assert((uint32_t)start_thread(uid, 4, "oops") == SCE_KERNEL_ERROR_NOT_DORMANT);
    assert(info->argp == saved_arguments && memcmp(info->argp, "test", 4) == 0);
    if (setjmp(thread_return) == 0)
        info->thread.entry(info->thread.argument);
    assert(info->return_status == 1234);
    assert(tp_exchanges == 0);
    assert(sceKernelGetThreadCurrentPriority() == 160 && raw_priority()==160);
    int status = -1;
    assert(sceKernelWaitThreadEnd(uid, &status, NULL) == 0 && status == 1234);
    wait_result = 0xEA01;
    SceUInt timeout = 1;
    assert((uint32_t)sceKernelWaitThreadEnd(uid, NULL, &timeout) == SCE_KERNEL_ERROR_WAIT_TIMEOUT);
    wait_result = 0;
    assert(sceKernelDeleteThread(uid) == 0);
    assert((uint32_t)sceKernelWaitThreadEnd(uid, NULL, NULL) == SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    /* The event wait and thread join succeeding do not mean the guest passed.
     * Preserve the status set by sceKernelExitProcess, including negative values. */
    const int exits[] = {0, 1, -7};
    for (unsigned i = 0; i < sizeof(exits) / sizeof(exits[0]); ++i) {
        process_exit_status = exits[i];
        assert(SceKernelThreadMgr_main_entry(entry, 0, NULL) == exits[i]);
    }
    assert(last_stack==256*1024 && last_priority==vita_priority_to_hos_priority(160));
    LoadProcessParameters settings={.has_main_priority=true,.main_priority=100,
        .has_main_stack_size=true,.main_stack_size=65536};
    process_exit_status=17;
    assert(SceKernelThreadMgr_main_entry_configured(entry,0,NULL,&settings)==17);
    assert(last_stack==65536 && last_priority==vita_priority_to_hos_priority(100));
    settings.main_priority=0x10000103;
    assert(SceKernelThreadMgr_main_entry_configured(entry,0,NULL,&settings)==17);
    assert(last_priority==vita_priority_to_hos_priority(0x10000103));
    unsigned created=create_calls;
    settings.main_stack_size=0;
    assert((uint32_t)SceKernelThreadMgr_main_entry_configured(entry,0,NULL,&settings)==SCE_KERNEL_ERROR_ILLEGAL_SIZE);
    settings.main_stack_size=UINT32_MAX;
    assert((uint32_t)SceKernelThreadMgr_main_entry_configured(entry,0,NULL,&settings)==SCE_KERNEL_ERROR_ILLEGAL_SIZE);
    settings.main_stack_size=65536;settings.main_priority=63;
    assert((uint32_t)SceKernelThreadMgr_main_entry_configured(entry,0,NULL,&settings)==SCE_KERNEL_ERROR_ILLEGAL_PRIORITY);
    assert(create_calls==created);
    start_result = 1;
    assert((uint32_t)SceKernelThreadMgr_main_entry(entry, 0, NULL) == SCE_KERNEL_ERROR_THREAD_ERROR);
    assert(SceKernelThreadMgr_finish() == 0);
    assert(SceKernelThreadMgr_init(&tls, true) == 0);
    start_result=0;writable_tp=0x12345678;expect_guest_tp=true;
    uid=create_thread("retargeted TLS", entry, 160, 4096);assert(uid > 0);
    info=get_thread_info_for_uid(uid);
    assert(start_thread(uid, 4, "test") == 0);
    if (setjmp(thread_return) == 0) info->thread.entry(info->thread.argument);
    assert(tp_exchanges == 2 && writable_tp == 0x12345678);
    assert(sceKernelDeleteThread(uid) == 0);
    assert(SceKernelThreadMgr_finish() == 0);
    assert(SceKernelThreadMgr_init(&tls,true)==0);
    SceUID handler=sceKernelRegisterThreadEventHandler("events",VITA_THREAD_EVENT_PROCESS_ALL,12,
                                                       event_callback,&event_count);
    assert(handler>0);
    for (volatile int mode=0;mode<3;++mode) {
        event_count=0;exit_in_start=mode==1;exit_in_end=mode==2;
        uid=create_thread("callback TLS",entry,160,4096);assert(uid>0);
        info=get_thread_info_for_uid(uid);assert(start_thread(uid,4,"test")==0);
        if (!setjmp(thread_return)) info->thread.entry(info->thread.argument);
        assert(event_count==2 && event_calls[0]==4 && event_calls[1]==8);
        assert(info->exit_notified && !info->event_frames && info->return_status==(mode ? 77 : 1234));
        assert(sceKernelDeleteThread(uid)==0);
    }
    assert(sceKernelUnregisterThreadEventHandler(handler)==0);
    VitaThreadEventOptions options={.handler=event_callback,.common=&event_count};
    handler=_sceKernelRegisterThreadEventHandler("raw",VITA_THREAD_EVENT_PROCESS_ALL,8,&options);
    assert(handler>0 && sceKernelUnregisterThreadEventHandler(handler)==0);
    assert((uint32_t)_sceKernelRegisterThreadEventHandler("raw",VITA_THREAD_EVENT_PROCESS_ALL,8,NULL)==SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    assert((uint32_t)sceKernelUnregisterThreadEventHandler(handler)==SCE_KERNEL_ERROR_UNKNOWN_THREAD_EVENT_ID);
    assert((uint32_t)sceKernelRegisterThreadEventHandler("bad target",-1,4,event_callback,NULL)==SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    assert(SceKernelThreadMgr_finish()==0);
    guest_tls_template_clear(&tls);
    puts("Thread runtime regression checks passed.");
    return 0;
}
