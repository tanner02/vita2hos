#define TEST_THREAD_RUNTIME
#define TEST_FIXED_MEMORY
#include <setjmp.h>
#include <assert.h>
#include <stdio.h>

#include "../../source/guest_tls.c"
#include "../../source/thread_events.c"
static void track_thread_free(void *pointer);
#define free track_thread_free
#include "../../source/modules/SceKernelThreadMgr.c"
#undef free
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
static unsigned create_calls, close_calls, start_calls, mask_calls, query_calls;
static u64 allowed_cores=7;
static Result query_result, mask_result;
static int last_cpu, mask_cpu;
static u32 last_mask;
static VitaThreadInfo *native_info;
static void *failed_info, *failed_tls;
static unsigned failed_info_frees, failed_tls_frees;
static void track_thread_free(void *pointer)
{
    if (pointer && pointer==failed_info) ++failed_info_frees;
    if (pointer && pointer==failed_tls) ++failed_tls_frees;
    free(pointer);
}
Result svcGetInfo(u64 *value,u32 type,Handle handle,u64 sub_id)
{
    assert(type==InfoType_CoreMask && handle==CUR_PROCESS_HANDLE && !sub_id);
    ++query_calls;*value=allowed_cores;return query_result;
}
Result svcSetThreadCoreMask(uint32_t handle,int32_t cpu,uint32_t mask)
{
    assert(handle==0x1234 && native_info && !atomic_load(&native_info->started));
    assert(!vita_thread_info_dict_get(g_vita_thread_infos,native_info->uid));
    assert(cpu==last_cpu && mask && (mask&(1U<<cpu)));
    for(int core=0;core<cpu;++core) assert(!(mask&(1U<<core)));
    ++mask_calls;mask_cpu=cpu;last_mask=mask;
    if(mask_result) {failed_info=native_info;failed_tls=native_info->vita_tls;}
    return mask_result;
}
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
SceUID sceKernelGetProcessId(void) { return 77; }
void log_printf(const char *format, ...) { (void)format; }
SceUID SceSysmem_get_next_uid(void) { static SceUID next = 1; return next++; }
UEvent *SceLibKernel_get_process_exit_uevent(void) { return &exit_event; }
int SceLibKernel_get_process_exit_res(void) { return process_exit_status; }
bool SceLibKernel_process_exit_requested(void) { return false; }
Result threadCreate(Thread *thread, void (*entry)(void *), void *arg, void *stack,
                    size_t size, int priority, int cpu)
{
    (void)stack;last_cpu=cpu;native_info=arg;
    last_stack=size;last_priority=priority;++create_calls;
    thread->handle = 0x1234;
    thread->stack_mirror=(void *)(uintptr_t)0x90000000;thread->stack_sz=size;
    thread->entry = entry;
    thread->argument = arg;
    return 0;
}
Result threadStart(Thread *thread) { (void)thread; ++start_calls;return start_result; }
Result threadClose(Thread *thread) { assert(thread->handle==0x1234);++close_calls;return 0; }
int threadTlsAlloc(void (*destructor)(void *)) { (void)destructor; return 1; }
void threadTlsFree(int slot) { (void)slot; }
void threadTlsSet(int slot, void *value) { (void)slot; tls_value = value; }
void *threadTlsGet(int slot) { (void)slot; return tls_value; }
Result waitSingle(Waiter waiter, uint64_t ns)
{
    if(!wait_result && waiter.thread && ns!=0) {
        VitaThreadInfo *info=waiter.thread->argument;
        atomic_store(&info->completed,true);
    }
    return wait_result;
}
void svcSleepThread(s64 ns) { (void)ns; }
void threadExit(void) { longjmp(thread_return, 1); }

static int entry(SceSize size, void *argument)
{
    assert(size == 4 && memcmp(argument, "test", 4) == 0);
    GuestThreadSnapshot running={.size=128};
    assert(!sceKernelGetThreadInfo(0,(void *)&running) && running.status==SCE_THREAD_RUNNING);
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
    int (*vfp_get)(void)=(void *)module_get_func_export(0xcae9ace6,0xA37A6057);
    int (*vfp_change)(SceUInt32,SceUInt32)=(void *)module_get_func_export(0x859a24b1,0xCC18FBAE);
    assert(vfp_get && vfp_change && !vfp_get() && !info->vita_tls[4]);
    assert(!vfp_change(0,SCE_KERNEL_VFP_EXCP_IOC|SCE_KERNEL_VFP_EXCP_QC));
    assert(vfp_get()==(SCE_KERNEL_VFP_EXCP_IOC|SCE_KERNEL_VFP_EXCP_QC));
    assert(vfp_change(0,0)==(SCE_KERNEL_VFP_EXCP_IOC|SCE_KERNEL_VFP_EXCP_QC));
    assert(vfp_change(SCE_KERNEL_VFP_EXCP_UFC,0)==(SCE_KERNEL_VFP_EXCP_IOC|SCE_KERNEL_VFP_EXCP_QC));

    assert((uint32_t)vfp_change(SCE_KERNEL_VFP_EXCP_IOC,SCE_KERNEL_VFP_EXCP_IOC)==SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    assert((uint32_t)vfp_change(0,0x100)==SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    assert(vfp_get()==(SCE_KERNEL_VFP_EXCP_IOC|SCE_KERNEL_VFP_EXCP_QC));
    tls_value=other_info;assert(!vfp_get());
    assert(!vfp_change(0,SCE_KERNEL_VFP_EXCP_DZC) && vfp_get()==SCE_KERNEL_VFP_EXCP_DZC);
    tls_value=info;
    assert(vfp_change(SCE_KERNEL_VFP_EXCP_ALL,0)==(SCE_KERNEL_VFP_EXCP_IOC|SCE_KERNEL_VFP_EXCP_QC));
    assert(!vfp_get() && other_info->vita_tls[4]==SCE_KERNEL_VFP_EXCP_DZC);
    GuestThreadSnapshot snapshot;
    memset(&snapshot,0xcc,sizeof(snapshot));snapshot.size=128;
    int (*query)(SceUID,SceKernelThreadInfo *)=(void *)module_get_func_export(0xcae9ace6,0x8D9C5461);
    int (*raw_query)(SceUID,SceKernelThreadInfo *)=(void *)module_get_func_export(0x859a24b1,0xB373D8A1);
    assert(query && raw_query);
    assert(!query(0,(void *)&snapshot));
    assert(snapshot.process_id==77 && !strcmp(snapshot.name,"worker"));
    assert(snapshot.entry==(uint32_t)(uintptr_t)entry && snapshot.stack==0x90000000);
    assert(snapshot.stack_size==4096 && snapshot.initial_priority==160 && snapshot.current_priority==160);
    assert(snapshot.status==SCE_THREAD_DORMANT && snapshot.current_cpu==-1 && !snapshot.run_clocks);
    GuestThreadSnapshot saved_snapshot=snapshot;
    snapshot.size=127;GuestThreadSnapshot bad=snapshot;
    assert((uint32_t)raw_query(uid,(void *)&snapshot)==SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);
    assert(!memcmp(&bad,&snapshot,sizeof(snapshot)));
    assert((uint32_t)query(uid,NULL)==SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    assert((uint32_t)query(-1,(void *)&snapshot)==SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    snapshot=saved_snapshot;
    tls_value=NULL;
    assert((uint32_t)vfp_get()==SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    assert((uint32_t)vfp_change(0,0)==SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);


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
    snapshot.size=128;
    assert(!query(uid,(void *)&snapshot));
    assert(snapshot.status==SCE_THREAD_DORMANT && snapshot.exit_status==1234);
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
    assert(SceKernelThreadMgr_init(&tls,false)==0);
    uid=create_thread("signals",entry,160,4096);assert(uid>0);
    info=get_thread_info_for_uid(uid);threadTlsSet(g_vita_thread_info_tls_slot_id,info);
    assert((uint32_t)sceKernelSendSignal(-1)==SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    assert(!sceKernelSendSignal(uid));
    assert((uint32_t)sceKernelSendSignal(uid)==SCE_KERNEL_ERROR_ALREADY_SENT);
    wait_result=0;unsigned signal_timeout=100000;
    assert(!sceKernelWaitSignal(0,0,&signal_timeout));assert(!atomic_load(&info->signal_pending));
    assert(!sceKernelSendSignal(uid));assert(!_sceKernelWaitSignal(0,0,NULL));
    assert((uint32_t)sceKernelWaitSignal(1,0,NULL)==SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    wait_result=0xEA01;signal_timeout=0;
    assert((uint32_t)sceKernelWaitSignal(0,0,&signal_timeout)==SCE_KERNEL_ERROR_WAIT_TIMEOUT);
    assert(!signal_timeout && !atomic_load(&info->signal_pending));
    wait_result=0;threadTlsSet(g_vita_thread_info_tls_slot_id,NULL);
    assert((uint32_t)sceKernelWaitSignal(0,0,NULL)==SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    assert(!sceKernelDeleteThread(uid));
    assert((uint32_t)sceKernelSendSignal(uid)==SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    assert(module_get_func_export(0xcae9ace6,0xADCA94E5));
    assert(SceKernelThreadMgr_finish()==0);
    assert(SceKernelThreadMgr_init(&tls,false)==0);
    assert((uint32_t)sceKernelCreateThread("bad affinity",entry,160,4096,0,1,NULL)==SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK);
    uid=sceKernelCreateThread("affinity",entry,160,4096,0,SCE_KERNEL_CPU_MASK_USER_1,NULL);assert(uid>0);
    info=get_thread_info_for_uid(uid);assert(info->vita_tls[9]==SCE_KERNEL_CPU_MASK_USER_1);
    assert(sceKernelGetThreadCpuAffinityMask(uid)==SCE_KERNEL_CPU_MASK_USER_1);
    threadTlsSet(g_vita_thread_info_tls_slot_id,info);
    assert(sceKernelGetThreadCpuAffinityMask(0)==SCE_KERNEL_CPU_MASK_USER_1);
    GuestThreadSnapshot affinity_info={.size=128};assert(!_sceKernelGetThreadInfo(uid,(void *)&affinity_info));
    assert(affinity_info.initial_affinity==SCE_KERNEL_CPU_MASK_USER_1 && affinity_info.current_affinity==SCE_KERNEL_CPU_MASK_USER_1);
    threadTlsSet(g_vita_thread_info_tls_slot_id,NULL);assert(!sceKernelDeleteThread(uid));
    assert((uint32_t)sceKernelGetThreadCpuAffinityMask(uid)==SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    uid=create_thread("default affinity",entry,160,4096);assert(uid>0);
    info=get_thread_info_for_uid(uid);
    assert(!info->affinity_mask && !info->vita_tls[9]);
    assert(last_mask==7 && last_cpu==0 && mask_cpu==0);
    assert(_sceKernelGetThreadCpuAffinityMask(uid)==SCE_KERNEL_CPU_MASK_USER_ALL);assert(!sceKernelDeleteThread(uid));
    for(unsigned mask=1;mask<8;++mask) {
        unsigned queried=query_calls,configured=mask_calls;
        uid=sceKernelCreateThread("mask combinations",entry,160,4096,0,(int)(mask<<16),NULL);assert(uid>0);
        info=get_thread_info_for_uid(uid);
        assert(info->affinity_mask==(int)(mask<<16) && info->vita_tls[9]==mask<<16);
        assert(query_calls==queried+1 && mask_calls==configured+1 && last_mask==mask);
        assert(sceKernelGetThreadCpuAffinityMask(uid)==(int)(mask<<16));assert(!sceKernelDeleteThread(uid));
    }
    /* Defaults intersect native permissions, while explicit masks must fit in full. */
    for(unsigned allowed=1;allowed<8;++allowed) {
        allowed_cores=allowed | UINT64_C(0x100000008);
        uid=create_thread("restricted default",entry,160,4096);assert(uid>0);
        assert(last_mask==allowed);
        info=get_thread_info_for_uid(uid);assert(!info->affinity_mask && !info->vita_tls[9]);
        assert(sceKernelGetThreadCpuAffinityMask(uid)==SCE_KERNEL_CPU_MASK_USER_ALL);
        assert(!sceKernelDeleteThread(uid));
        for(unsigned mask=1;mask<8;++mask) {
            unsigned before=create_calls,configured=mask_calls;
            uid=sceKernelCreateThread("restricted explicit",entry,160,4096,0,(int)(mask<<16),NULL);
            if((mask&allowed)==mask) {
                assert(uid>0 && create_calls==before+1 && last_mask==mask);
                assert(sceKernelGetThreadCpuAffinityMask(uid)==(int)(mask<<16));
                assert(!sceKernelDeleteThread(uid));
            } else {
                assert((uint32_t)uid==SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK);
                assert(create_calls==before && mask_calls==configured);
            }
            assert(!vita_thread_info_dict_size(g_vita_thread_infos));
        }
    }
    unsigned before=create_calls,configured=mask_calls,closed=close_calls,started=start_calls;
    allowed_cores=8; /* Native system cores cannot satisfy a guest user mask. */
    assert((uint32_t)create_thread("no user cores",entry,160,4096)==SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK);
    assert((uint32_t)sceKernelCreateThread("no explicit user core",entry,160,4096,0,SCE_KERNEL_CPU_MASK_USER_1,NULL)==SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK);
    allowed_cores=7;query_result=1;
    assert((uint32_t)create_thread("query failure",entry,160,4096)==SCE_KERNEL_ERROR_THREAD_ERROR);
    query_result=0;
    assert(create_calls==before && mask_calls==configured && close_calls==closed && start_calls==started);
    mask_result=1;
    assert((uint32_t)create_thread("mask failure",entry,160,4096)==SCE_KERNEL_ERROR_THREAD_ERROR);
    assert(create_calls==before+1 && mask_calls==configured+1 && close_calls==closed+1 && start_calls==started);
    assert(failed_info_frees==1 && failed_tls_frees==1);
    assert(!vita_thread_info_dict_size(g_vita_thread_infos));
    failed_info=failed_tls=NULL;mask_result=0;
    uid=create_thread("after mask failure",entry,160,4096);assert(uid>0);assert(!sceKernelDeleteThread(uid));
    assert((uint32_t)sceKernelCreateThread("system affinity",entry,160,4096,0,0x80000,NULL)==SCE_KERNEL_ERROR_ILLEGAL_CPU_AFFINITY_MASK);
    assert(module_get_func_export(0x859a24b1,0xF1AE5654));
    assert(SceKernelThreadMgr_finish()==0);
    assert(!SceKernelThreadMgr_init(&tls,false));
    uid=create_thread("shutdown live",entry,160,4096);assert(uid>0);
    SceUID dormant=create_thread("shutdown dormant",entry,160,4096);assert(dormant>0);
    info=get_thread_info_for_uid(uid);assert(!start_thread(uid,0,NULL));
    uint32_t *saved_tls=info->vita_tls;
    assert((uint32_t)sceKernelDeleteThread(uid)==SCE_KERNEL_ERROR_NOT_DORMANT);
    assert((uint32_t)SceKernelThreadMgr_quiesce()==SCE_KERNEL_ERROR_NOT_DORMANT);
    assert((uint32_t)SceKernelThreadMgr_finish()==SCE_KERNEL_ERROR_NOT_DORMANT);
    assert(get_thread_info_for_uid(uid)==info && info->vita_tls==saved_tls);
    assert(guest_tls_template_valid(&g_main_tls) && !memcmp(&g_main_tls,&tls,sizeof(tls)));
    assert((uint32_t)create_thread("after shutdown",entry,160,4096)==SCE_KERNEL_ERROR_THREAD_ERROR);
    assert((uint32_t)start_thread(dormant,0,NULL)==SCE_KERNEL_ERROR_THREAD_ERROR);
    atomic_store(&info->completed,true);wait_result=1;
    assert((uint32_t)SceKernelThreadMgr_quiesce()==SCE_KERNEL_ERROR_NOT_DORMANT);
    wait_result=0;assert(!SceKernelThreadMgr_quiesce());
    assert(!sceKernelDeleteThread(uid));assert(!sceKernelDeleteThread(dormant));
    assert(!SceKernelThreadMgr_finish());
    assert(!SceKernelThreadMgr_init(&tls,false));
    uid=create_thread("shutdown reset",entry,160,4096);assert(uid>0);
    assert(!sceKernelDeleteThread(uid));assert(!SceKernelThreadMgr_finish());
    guest_tls_template_clear(&tls);
    puts("Thread runtime regression checks passed.");
    return 0;
}
