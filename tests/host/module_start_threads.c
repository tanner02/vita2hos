#define _GNU_SOURCE
#define TEST_THREAD_RUNTIME
#define TEST_THREAD_RUNTIME_PTHREAD
#define TEST_THREADED_RWLOCK
#define TEST_THREADED_MUTEX
#define TEST_FIXED_MEMORY
#include <assert.h>
#include <stdio.h>
#include "../../source/guest_tls.c"
#include "../../source/thread_events.c"
#include "../../source/modules/SceKernelThreadMgr.c"
#include "../../source/load_process_run.c"

static _Thread_local void *current_tls;
static _Thread_local uintptr_t current_tp;
static atomic_int entries, exits, main_calls, joins, creates;
static atomic_int fail_create, fail_start, fail_wait;
static bool explicit_exit;
static int module_status;
static atomic_bool process_exit;
static bool exit_process;
static SceUID parent_uid;
static UEvent process_event;
SceUID sceKernelGetProcessId(void) { return 77; }
void log_printf(const char *format,...) { (void)format; }
SceUID SceSysmem_get_next_uid(void) { static atomic_int uid=1;return atomic_fetch_add(&uid,1); }
UEvent *SceLibKernel_get_process_exit_uevent(void) { return &process_event; }
int SceLibKernel_get_process_exit_res(void) { return 77; }
bool SceLibKernel_process_exit_requested(void) { return atomic_load(&process_exit); }
uintptr_t guest_tp_exchange(uintptr_t value) { uintptr_t old=current_tp;current_tp=value;return old; }
int threadTlsAlloc(void (*destructor)(void *)) { (void)destructor;return 1; }
void threadTlsFree(int slot) { (void)slot; }
void threadTlsSet(int slot,void *value) { (void)slot;current_tls=value; }
void *threadTlsGet(int slot) { (void)slot;return current_tls; }
static void *native_entry(void *arg)
{
    Thread *thread=arg;thread->entry(thread->argument);abort();
}
Result threadCreate(Thread *thread,void (*entry)(void *),void *arg,void *stack,
                    size_t size,int priority,int cpu)
{
    (void)stack;(void)cpu;
    if (atomic_exchange(&fail_create,0)) return 1;
    assert(size==256*1024 && priority==vita_priority_to_hos_priority(100));
    *thread=(Thread){.entry=entry,.argument=arg};atomic_fetch_add(&creates,1);return 0;
}
Result threadStart(Thread *thread)
{
    if (atomic_exchange(&fail_start,0)) return 1;
    int result=pthread_create(&thread->native,NULL,native_entry,thread);
    if (!result) thread->started=true;
    return result;
}
Result threadClose(Thread *thread)
{
    assert(!thread->started || thread->joined);return 0;
}
Result waitSingle(Waiter waiter,uint64_t ns)
{
    if(waiter.event) {
        uint64_t start=armGetSystemTick();
        while(!__atomic_exchange_n(waiter.event,0,__ATOMIC_SEQ_CST)) {
            if(ns!=UINT64_MAX && armTicksToNs(armGetSystemTick()-start)>=ns)return 0xEA01;
            sched_yield();
        }
        return 0;
    }
    assert(waiter.thread && (ns==UINT64_MAX || ns==0));
    if(ns==0) {
        Thread *thread=waiter.thread;
        if(thread->joined)return 0;
        int result=pthread_tryjoin_np(thread->native,NULL);
        if(!result){thread->joined=true;atomic_fetch_add(&joins,1);}
        return result ? 0xEA01 : 0;
    }
    if (atomic_exchange(&fail_wait,0)) return 1;
    Thread *thread=waiter.thread;
    if (!thread->joined) {
        assert(!pthread_join(thread->native,NULL));thread->joined=true;
        atomic_fetch_add(&joins,1);
    }
    return 0;
}
Result svcSetThreadPriority(uint32_t handle,uint32_t priority) { (void)handle;(void)priority;return 0; }
void svcSleepThread(s64 ns) { (void)ns; }
void threadExit(void) { pthread_exit(NULL); }

static int event(SceInt32 type,SceUID uid,SceInt32 arg,void *common)
{
    (void)common;VitaThreadInfo *info=SceKernelThreadMgr_get_thread_info();
    assert(info && uid==info->uid && !arg && current_tp==(uintptr_t)info->vita_user_tls);
    if (type==VITA_THREAD_EVENT_START) atomic_fetch_add(&entries,1);
    if (type==VITA_THREAD_EVENT_EXIT) atomic_fetch_add(&exits,1);
    return 0;
}
static int dependency(SceSize args,void *argp)
{
    assert(!args && !argp);
    VitaThreadInfo *info=SceKernelThreadMgr_get_thread_info();
    assert(info && info->uid!=parent_uid && info->priority==100);
    assert(info->affinity_mask==SCE_KERNEL_CPU_MASK_USER_1 && info->vita_tls[9]==SCE_KERNEL_CPU_MASK_USER_1);
    assert(sceKernelGetThreadCpuAffinityMask(0)==SCE_KERNEL_CPU_MASK_USER_1);
    assert(current_tp==(uintptr_t)info->vita_user_tls);
    assert(*(uint32_t *)info->vita_user_tls==0x12345678);
    *(uint32_t *)info->vita_user_tls=0xfeedface;
    if (exit_process) { atomic_store(&process_exit,true);sceKernelExitThread(77); }
    if (explicit_exit) sceKernelExitThread(module_status);
    return module_status;
}
static int guest_main(uint32_t args,void *argp)
{
    assert(!args && !argp);
    VitaThreadInfo *info=SceKernelThreadMgr_get_thread_info();
    assert(info->uid==parent_uid && *(uint32_t *)info->vita_user_tls==0x12345678);
    assert(current_tp==(uintptr_t)info->vita_user_tls);
    atomic_fetch_add(&main_calls,1);return 123;
}
static int parent_entry(SceSize args,void *argp)
{
    assert(args==sizeof(LoadProcess *));LoadProcess *process;
    memcpy(&process,argp,sizeof(process));
    return load_process_run(process,0,NULL);
}
static void run(bool exit_call,int status,int injected)
{
    explicit_exit=exit_call;module_status=status;atomic_store(&process_exit,false);
    atomic_store(&entries,0);atomic_store(&exits,0);atomic_store(&main_calls,0);
    atomic_store(&creates,0);atomic_store(&joins,0);
    LoadImage images[3]={{.bound=true},{.bound=true},{.bound=true}};
    images[1].lifecycle.start=dependency;images[2].lifecycle.start=dependency;
    LoadProcess process={.images=images,.count=3,.bound=true,.entry=guest_main},*ptr=&process;
    parent_uid=create_thread_with_affinity("parent",parent_entry,100,256*1024,SCE_KERNEL_CPU_MASK_USER_1);assert(parent_uid>0);
    /* The parent has already been created; inject a dependency-stage failure. */
    atomic_store(&fail_create,injected==1);atomic_store(&fail_start,injected==2);
    atomic_store(&fail_wait,injected==3);
    /* Bypass the injection for the parent's own start and join. */
    VitaThreadInfo *parent=get_thread_info_for_uid(parent_uid);
    parent->argp=malloc(sizeof(ptr));assert(parent->argp);memcpy(parent->argp,&ptr,sizeof(ptr));
    parent->arglen=sizeof(ptr);atomic_store(&parent->started,true);
    assert(!pthread_create(&parent->thread.native,NULL,native_entry,&parent->thread));
    parent->thread.started=true;
    assert(!pthread_join(parent->thread.native,NULL));parent->thread.joined=true;
    bool success=!status && injected!=1 && injected!=2 && !exit_process;
    assert(parent->return_status==(exit_process ? 77 : (success ? 123 : (int)SCE_KERNEL_ERROR_MODULEMGR_START_FAILED)));
    assert(atomic_load(&main_calls)==(success ? 1 : 0));
    assert(process.modules_started==(success ? 2u : 0u));
    int children=(injected==1 || injected==2) ? 0 : ((status || exit_process) ? 1 : 2);
    assert(atomic_load(&entries)==children+1 && atomic_load(&exits)==children+1);
    assert(atomic_load(&joins)==children);
    assert(atomic_load(&creates)==1+children+(injected==2));
    assert(!sceKernelDeleteThread(parent_uid));
    assert(vita_thread_info_dict_size(g_vita_thread_infos)==0);
}
static atomic_uint priority_successes;
static void *change_until_deleted(void *argument)
{
    SceUID uid=*(SceUID *)argument;
    for (;;) {
        int result=sceKernelChangeThreadPriority(uid,100);
        if ((uint32_t)result==SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID) break;
        assert(!result);
        result=sceKernelSendSignal(uid);
        if((uint32_t)result==SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID)break;
        assert(!result || (uint32_t)result==SCE_KERNEL_ERROR_ALREADY_SENT);
        GuestThreadSnapshot snapshot={.size=128};
        result=_sceKernelGetThreadInfo(uid,(void *)&snapshot);
        if ((uint32_t)result==SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID) break;
        assert(!result && snapshot.current_priority==100 && snapshot.initial_priority==100);
        assert(snapshot.status==SCE_THREAD_DORMANT);
        atomic_fetch_add(&priority_successes,1);
    }
    return NULL;
}
static void priority_delete_race(void)
{
    SceUID uid=create_thread("priority/delete",dependency,100,256*1024);
    assert(uid>0);
    pthread_t changer;
    assert(!pthread_create(&changer,NULL,change_until_deleted,&uid));
    while (!atomic_load(&priority_successes)) sched_yield();
    assert(!sceKernelDeleteThread(uid));
    assert(!pthread_join(changer,NULL));
    assert((uint32_t)sceKernelChangeThreadPriority(uid,100)==SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    assert(!vita_thread_info_dict_size(g_vita_thread_infos));
}
static atomic_uint signal_ready,signal_received;
static int signal_receiver(SceSize args,void *argp)
{
    (void)args;(void)argp;
    for(unsigned i=1;i<=128;++i) {
        atomic_store(&signal_ready,i);unsigned timeout=1000000;
        assert(!sceKernelWaitSignal(0,0,&timeout));
        atomic_store(&signal_received,i);
    }
    return 0;
}
static void signal_roundtrips(void)
{
    SceUID uid=create_thread("signal receiver",signal_receiver,100,256*1024);assert(uid>0);
    assert(!start_thread(uid,0,NULL));
    uint64_t start=armGetSystemTick();
    for(unsigned i=1;i<=128;++i) {
        while(atomic_load(&signal_ready)<i) {assert(armTicksToNs(armGetSystemTick()-start)<UINT64_C(5000000000));sched_yield();}
        assert(!sceKernelSendSignal(uid));
        while(atomic_load(&signal_received)<i) {assert(armTicksToNs(armGetSystemTick()-start)<UINT64_C(5000000000));sched_yield();}
    }
    assert(!sceKernelWaitThreadEnd(uid,NULL,NULL));assert(!sceKernelDeleteThread(uid));
}
static atomic_bool shutdown_entered,shutdown_release;
static int shutdown_worker(SceSize args,void *argp)
{
    (void)args;(void)argp;atomic_store(&shutdown_entered,true);
    while(!atomic_load(&shutdown_release))sched_yield();
    return 0;
}
static void shutdown_live_worker(void)
{
    SceUID uid=create_thread("shutdown worker",shutdown_worker,100,256*1024);assert(uid>0);
    SceUID dormant=create_thread("shutdown dormant",shutdown_worker,100,256*1024);assert(dormant>0);
    assert(!start_thread(uid,0,NULL));
    uint64_t start=armGetSystemTick();
    while(!atomic_load(&shutdown_entered)) {
        assert(armTicksToNs(armGetSystemTick()-start)<UINT64_C(5000000000));sched_yield();
    }
    assert((uint32_t)SceKernelThreadMgr_quiesce()==SCE_KERNEL_ERROR_NOT_DORMANT);
    assert((uint32_t)SceKernelThreadMgr_finish()==SCE_KERNEL_ERROR_NOT_DORMANT);
    assert((uint32_t)start_thread(dormant,0,NULL)==SCE_KERNEL_ERROR_THREAD_ERROR);
    assert((uint32_t)create_thread("late",shutdown_worker,100,256*1024)==SCE_KERNEL_ERROR_THREAD_ERROR);
    atomic_store(&shutdown_release,true);
    assert(!sceKernelWaitThreadEnd(uid,NULL,NULL));
    assert(!SceKernelThreadMgr_quiesce());
    assert(!sceKernelDeleteThread(uid));assert(!sceKernelDeleteThread(dormant));
}
int main(void)
{
    GuestTlsTemplate tls={0};uint32_t seed=0x12345678;
    assert(!guest_tls_template_create(&tls,&seed,4,16));
    assert(!SceKernelThreadMgr_init(&tls,true));
    assert(SceKernelThreadMgr_run_module_start(dependency)<0); /* Host is not a guest. */
    assert(thread_events_register("events",VITA_THREAD_EVENT_PROCESS_ALL,
           VITA_THREAD_EVENT_START|VITA_THREAD_EVENT_EXIT,event,NULL)>0);
    run(false,0,0);run(true,0,0);run(true,2,0);run(false,-123,0);
    run(false,0,1);run(false,0,2);run(true,0,3);
    exit_process=true;run(false,0,0);
    signal_roundtrips();
    priority_delete_race();
    shutdown_live_worker();
    assert(!SceKernelThreadMgr_finish());guest_tls_template_clear(&tls);
    puts("Module startup threads: return/exit status, main continuation, isolated TLS, events, joins and failure gating passed.");
}
