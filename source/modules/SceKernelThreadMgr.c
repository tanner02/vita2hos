#include <m-dict.h>
#include <psp2/kernel/error.h>
#include <psp2/kernel/threadmgr.h>
#include <stdlib.h>
#include <switch.h>

#include "log.h"
#include "module.h"
#include "util.h"
#include "guest_tp_plan.h"

#include "modules/SceKernelThreadMgr.h"
#include "modules/SceLibKernel.h"
#include "modules/SceSysmem.h"

#define SCE_KERNEL_HIGHEST_PRIORITY_USER               64
#define SCE_KERNEL_LOWEST_PRIORITY_USER                191
#define SCE_KERNEL_DEFAULT_PRIORITY                    ((SceInt32)0x10000100)
#define SCE_KERNEL_DEFAULT_PRIORITY_GAME_APP           160
#define SCE_KERNEL_DEFAULT_PRIORITY_USER               SCE_KERNEL_DEFAULT_PRIORITY
#define SCE_KERNEL_THREAD_STACK_SIZE_DEFAULT_USER_MAIN (256 * 1024)
#define SCE_KERNEL_TLS_SIZE                            GUEST_KERNEL_TLS_SIZE

#define HOS_HIGHEST_PRIORITY 28
#define HOS_LOWEST_PRIORITY  59

static s32 g_vita_thread_info_tls_slot_id;
static GuestTlsTemplate g_main_tls; /* Borrowed from the live process image. */
static bool g_retarget_thread_pointer;

DICT_DEF2(vita_thread_info_dict, SceUID, M_DEFAULT_OPLIST, VitaThreadInfo *, M_POD_OPLIST)
static vita_thread_info_dict_t g_vita_thread_infos;
static RwLock g_vita_thread_infos_lock;

static inline int resolve_vita_priority(int priority)
{
    if ((priority & SCE_KERNEL_DEFAULT_PRIORITY) == SCE_KERNEL_DEFAULT_PRIORITY)
        priority = SCE_KERNEL_DEFAULT_PRIORITY_GAME_APP + (priority & ~SCE_KERNEL_DEFAULT_PRIORITY);

    if ((priority < SCE_KERNEL_HIGHEST_PRIORITY_USER) ||
        (priority > SCE_KERNEL_LOWEST_PRIORITY_USER))
        return SCE_KERNEL_ERROR_ILLEGAL_PRIORITY;

    return priority;
}

static inline int vita_priority_to_hos_priority(int priority)
{
    priority = resolve_vita_priority(priority);
    if (priority < 0) return priority;
    return HOS_HIGHEST_PRIORITY +
           ((priority - SCE_KERNEL_HIGHEST_PRIORITY_USER) *
            (HOS_LOWEST_PRIORITY - HOS_HIGHEST_PRIORITY)) /
               (SCE_KERNEL_LOWEST_PRIORITY_USER - SCE_KERNEL_HIGHEST_PRIORITY_USER);
}

static VitaThreadInfo *get_thread_info_for_uid(SceUID thid)
{
    VitaThreadInfo *ti;
    VitaThreadInfo **entry;

    rwlockReadLock(&g_vita_thread_infos_lock);
    entry = vita_thread_info_dict_get(g_vita_thread_infos, thid);
    ti = entry ? *entry : NULL;
    rwlockReadUnlock(&g_vita_thread_infos_lock);

    return ti;
}

static void NORETURN thread_entry_wrapper(void *arg)
{
    VitaThreadInfo *ti = arg;
    int ret;

    threadTlsSet(g_vita_thread_info_tls_slot_id, ti);

    uintptr_t previous=0;
    bool retarget=g_retarget_thread_pointer;
    if (retarget) previous=guest_tp_exchange((uintptr_t)ti->vita_user_tls);
    thread_events_dispatch(VITA_THREAD_EVENT_START,ti->uid,&ti->event_frames);
    ret = ti->entry(ti->arglen, ti->argp);
    SceKernelThreadMgr_prepare_current_exit(ret);
    if (retarget) guest_tp_exchange(previous);

    LOG("Thread 0x%x returned with: 0x%x", ti->uid, ret);

    threadExit();
}

static SceUID create_thread(const char *name, SceKernelThreadEntry entry, int initPriority,
                            SceSize stackSize)
{
    VitaThreadInfo *ti;
    Result res;
    int priority = vita_priority_to_hos_priority(initPriority);

    if (priority < 0)
        return priority;
    if (!name || !entry)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;

    ti = malloc(sizeof(*ti));
    if (!ti) {
        LOG("Could not allocate thread info for thread \"%s\"", name);
        return SCE_KERNEL_ERROR_NO_MEMORY;
    }

    memset(ti, 0, sizeof(*ti));
    atomic_init(&ti->started, false);
    ti->uid = SceSysmem_get_next_uid();
    strncpy(ti->name, name, sizeof(ti->name) - 1);
    ti->entry = entry;
    atomic_init(&ti->priority, resolve_vita_priority(initPriority));
    ti->vita_tls = guest_tls_allocate(&g_main_tls);
    if (!ti->vita_tls) {
        free(ti);
        return SCE_KERNEL_ERROR_NO_MEMORY;
    }
    ti->vita_user_tls = guest_tls_user_area(ti->vita_tls);
    ti->vita_tls[8] = atomic_load(&ti->priority); /* Pinned kernel TLS priority slot. */

    res = threadCreate(&ti->thread, thread_entry_wrapper, ti, NULL, stackSize, priority, -2);
    if (R_FAILED(res)) {
        LOG("Error creating thread: 0x%" PRIx32, res);
        free(ti->vita_tls);
        free(ti);
        return SCE_KERNEL_ERROR_THREAD_ERROR;
    }

    LOG("Created thread \"%s\", thid: 0x%x", name, ti->uid);

    rwlockWriteLock(&g_vita_thread_infos_lock);
    vita_thread_info_dict_set_at(g_vita_thread_infos, ti->uid, ti);
    rwlockWriteUnlock(&g_vita_thread_infos_lock);

    return ti->uid;
}

static int start_thread(SceUID thid, SceSize arglen, void *argp)
{
    VitaThreadInfo *ti = get_thread_info_for_uid(thid);
    Result res;

    if (!ti)
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;

    if (arglen && !argp)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    bool expected = false;
    if (!atomic_compare_exchange_strong(&ti->started, &expected, true))
        return SCE_KERNEL_ERROR_NOT_DORMANT;

    if (arglen && argp) {
        ti->argp = malloc(arglen);
        if (!ti->argp) {
            atomic_store(&ti->started, false);
            return SCE_KERNEL_ERROR_NO_MEMORY;
        }

        memcpy(ti->argp, argp, arglen);
        ti->arglen = arglen;
    }

    res = threadStart(&ti->thread);
    if (R_FAILED(res)) {
        LOG("Error starting thread 0x%x: 0x%" PRIx32, thid, res);
        free(ti->argp);
        ti->argp = NULL;
        ti->arglen = 0;
        atomic_store(&ti->started, false);
        return SCE_KERNEL_ERROR_THREAD_ERROR;
    }

    return 0;
}

EXPORT(SceLibKernel, 0xC5C11EE7, SceUID, sceKernelCreateThread, const char *name,
       SceKernelThreadEntry entry, int initPriority, SceSize stackSize, SceUInt attr,
       int cpuAffinityMask, const SceKernelThreadOptParam *option)
{
    return create_thread(name, entry, initPriority, stackSize);
}

EXPORT(SceThreadmgr, 0x1BBDE3D9, int, sceKernelDeleteThread, SceUID thid)
{
    /* Serialize native-handle close and record reclamation with priority
     * updates. threadClose does not wait for a running thread to terminate. */
    rwlockWriteLock(&g_vita_thread_infos_lock);
    VitaThreadInfo **entry = vita_thread_info_dict_get(g_vita_thread_infos, thid);
    VitaThreadInfo *ti = entry ? *entry : NULL;
    if (!ti) {
        rwlockWriteUnlock(&g_vita_thread_infos_lock);
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    }
    Result res = threadClose(&ti->thread);
    if (R_FAILED(res)) {
        rwlockWriteUnlock(&g_vita_thread_infos_lock);
        LOG("Error closing thread 0x%x exit: 0x%" PRIx32, thid, res);
        return SCE_KERNEL_ERROR_THREAD_ERROR;
    }
    vita_thread_info_dict_erase(g_vita_thread_infos, thid);
    free(ti->argp);
    free(ti->vita_tls);
    free(ti);
    rwlockWriteUnlock(&g_vita_thread_infos_lock);
    LOG("Deleted thread 0x%x", thid);

    return 0;
}

EXPORT(SceLibKernel, 0xF08DE149, int, sceKernelStartThread, SceUID thid, SceSize arglen, void *argp)
{
    return start_thread(thid, arglen, argp);
}

EXPORT(SceThreadmgrCoredumpTime, 0x0C8A38E1, int NORETURN, sceKernelExitThread, int status)
{
    SceKernelThreadMgr_prepare_current_exit(status);
    threadExit();
}

void SceKernelThreadMgr_prepare_current_exit(int status)
{
    VitaThreadInfo *ti=SceKernelThreadMgr_get_thread_info();
    if (!ti) return;
    ti->return_status=status;
    if (!ti->exit_notified) {
        ti->exit_notified=true;
        thread_events_dispatch(VITA_THREAD_EVENT_EXIT,ti->uid,&ti->event_frames);
    }
    thread_events_abandon(&ti->event_frames);
}

EXPORT(SceLibKernel, 0x6D8C0F13, SceUID, sceKernelRegisterThreadEventHandler,
       const char *name,SceUID target,SceInt32 mask,VitaThreadEventHandler handler,void *common)
{
    if (target!=VITA_THREAD_EVENT_PROCESS_ALL && !get_thread_info_for_uid(target))
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    return thread_events_register(name,target,mask,handler,common);
}

EXPORT(SceThreadmgr, 0x2C8ED6F0, SceUID, sceKernelUnregisterThreadEventHandler,SceUID uid)
{ return thread_events_unregister(uid); }

EXPORT(SceThreadmgr, 0xCE6B49D8, SceUID, _sceKernelRegisterThreadEventHandler,
       const char *name,SceUID target,SceUInt32 mask,const VitaThreadEventOptions *option)
{
    if (!option) return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    return sceKernelRegisterThreadEventHandler(name,target,(SceInt32)mask,option->handler,option->common);
}

EXPORT(SceThreadmgr, 0x1D17DECF, int NORETURN, sceKernelExitDeleteThread, int status)
{
    sceKernelExitThread(status);
    // TODO: Delete
}

EXPORT(SceLibKernel, 0xDDB395A9, int, sceKernelWaitThreadEnd, SceUID thid, int *stat,
       SceUInt *timeout)
{
    VitaThreadInfo *ti = get_thread_info_for_uid(thid);
    uint64_t ns;
    Result res;

    if (!ti)
        return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;

    if (!timeout)
        ns = UINT64_MAX;
    else
        ns = *timeout * 1000ull;

    res = waitSingle(waiterForThread(&ti->thread), ns);
    if (R_FAILED(res)) {
        LOG("Error waiting for thread 0x%x exit: 0x%" PRIx32, thid, res);
        return res == 0xEA01 ? SCE_KERNEL_ERROR_WAIT_TIMEOUT : SCE_KERNEL_ERROR_THREAD_ERROR;
    }

    if (stat)
        *stat = ti->return_status;

    return 0;
}

int SceKernelThreadMgr_run_module_start(SceKernelThreadEntry entry)
{
    VitaThreadInfo *parent=SceKernelThreadMgr_get_thread_info();
    if (!parent) return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    SceUID uid=create_thread("<module_start>",entry,parent->priority,
                             SCE_KERNEL_THREAD_STACK_SIZE_DEFAULT_USER_MAIN);
    if (uid<0) return uid;
    int result=start_thread(uid,0,NULL);
    if (result<0) {
        sceKernelDeleteThread(uid);
        return result;
    }
    int status;
    while ((result=sceKernelWaitThreadEnd(uid,&status,NULL))<0) {
        LOG("Module-start thread 0x%x join failed: 0x%x; retaining live process",uid,result);
        svcSleepThread(1000000);
    }
    result=sceKernelDeleteThread(uid);
    /* ExitProcess from a dependency has already awakened the host waiter.
     * End the parent startup thread too, rather than entering another module
     * or main while the host is waiting to reclaim this process. */
    if (SceLibKernel_process_exit_requested())
        sceKernelExitThread(SceLibKernel_get_process_exit_res());
    return result<0 ? result : status;
}

int SceKernelThreadMgr_main_entry(SceKernelThreadEntry entry, int args, void *argp)
{
    return SceKernelThreadMgr_main_entry_configured(entry,args,argp,NULL);
}

int SceKernelThreadMgr_main_entry_configured(SceKernelThreadEntry entry, int args, void *argp,
                                             const LoadProcessParameters *parameters)
{
    SceUID thid;
    UEvent *process_exit_event;
    int ret;
    Result res;

    int priority=parameters && parameters->has_main_priority ? parameters->main_priority :
        SCE_KERNEL_DEFAULT_PRIORITY_USER;
    uint32_t stack=parameters && parameters->has_main_stack_size ? parameters->main_stack_size :
        SCE_KERNEL_THREAD_STACK_SIZE_DEFAULT_USER_MAIN;
    if (!stack || stack > UINT32_MAX-0xfff) return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
    LOG("Main guest thread priority=0x%x stack=0x%x",priority,stack);
    thid = create_thread("<main>", entry, priority, stack);
    if (thid < 0)
        return thid;

    ret = start_thread(thid, args, argp);
    if (ret < 0) {
        sceKernelDeleteThread(thid);
        return SCE_KERNEL_ERROR_THREAD_ERROR;
    }

    process_exit_event = SceLibKernel_get_process_exit_uevent();

    res = waitSingle(waiterForUEvent(process_exit_event), -1);
    if (R_FAILED(res)) {
        LOG("Error waiting for the process to finish: 0x%" PRIx32, res);
        return -1;
    }

    // TODO: Also wait for all threads to finish?
    sceKernelWaitThreadEnd(thid, NULL, NULL);
    sceKernelDeleteThread(thid);

    return SceLibKernel_get_process_exit_res();
}

EXPORT(SceThreadmgr, 0x4B675D05, int, sceKernelDelayThread, SceUInt delay)
{
    svcSleepThread((s64)delay * 1000);
    return 0;
}

EXPORT(SceLibKernel, 0xC53A9848, int, sceKernelGetThreadCurrentPriority)
{
    VitaThreadInfo *thread = SceKernelThreadMgr_get_thread_info();
    return thread ? thread->priority : (int)SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
}

/* Priority 0 selects the caller's current priority, even for another target.
 * Keep the Vita value: adjacent Vita priorities may map to one Horizon value. */
EXPORT(SceThreadmgr, 0xC5FFCA61, int, sceKernelChangeThreadPriority2, SceUID thid, int priority)
{
    VitaThreadInfo *caller = SceKernelThreadMgr_get_thread_info();
    if (!thid) {
        if (!caller) return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
        thid = caller->uid;
    }
    rwlockWriteLock(&g_vita_thread_infos_lock);
    VitaThreadInfo **entry = vita_thread_info_dict_get(g_vita_thread_infos, thid);
    VitaThreadInfo *target = entry ? *entry : NULL;
    int result;
    if (!target) {
        result = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    } else if (!priority && !caller) {
        result = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    } else {
        int resolved = resolve_vita_priority(priority ? priority : atomic_load(&caller->priority));
        if (resolved < 0) {
            result = resolved;
        } else {
            Result native = svcSetThreadPriority(target->thread.handle,
                                                  vita_priority_to_hos_priority(resolved));
            if (R_FAILED(native)) {
                result = SCE_KERNEL_ERROR_THREAD_ERROR;
            } else {
                result = atomic_exchange(&target->priority, resolved);
                target->vita_tls[8] = resolved;
            }
        }
    }
    rwlockWriteUnlock(&g_vita_thread_infos_lock);
    return result;
}

EXPORT(SceThreadmgr, 0xBD0139F2, int, sceKernelChangeThreadPriority, SceUID thid, int priority)
{
    int result = sceKernelChangeThreadPriority2(thid, priority);
    return result < 0 ? result : 0;
}

/* Bundled FIOS imports the syscall NID through SceThreadmgr directly. */
EXPORT(SceThreadmgr,0x01414F0B,int,_sceKernelGetThreadCurrentPriority,void)
{
    return sceKernelGetThreadCurrentPriority();
}

EXPORT(SceThreadmgr, 0xBACA6891, void *, sceKernelGetThreadTLSAddr, SceUID thid, int key)
{
    VitaThreadInfo *ti = get_thread_info_for_uid(thid);

    if (!ti)
        return NULL;

    if (key >= 0 && key <= 0x100)
        return &ti->vita_tls[key];

    return NULL;
}

/* SceLibKernel declared in SceLibKernel.c */
DECLARE_LIBRARY(SceThreadmgr, 0x859a24b1);
DECLARE_LIBRARY(SceThreadmgrCoredumpTime, 0x5E8D0E22);

int SceKernelThreadMgr_init(const GuestTlsTemplate *main_tls, bool retarget_thread_pointer)
{
    thread_events_init();
    g_main_tls = main_tls ? *main_tls : (GuestTlsTemplate){0};
    if (!guest_tls_template_valid(&g_main_tls)) return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    g_retarget_thread_pointer=retarget_thread_pointer;
    g_vita_thread_info_tls_slot_id = threadTlsAlloc(NULL);
    vita_thread_info_dict_init(g_vita_thread_infos);
    rwlockInit(&g_vita_thread_infos_lock);

    return 0;
}

int SceKernelThreadMgr_finish(void)
{
    thread_events_finish();
    g_main_tls = (GuestTlsTemplate){0};
    g_retarget_thread_pointer=false;
    threadTlsFree(g_vita_thread_info_tls_slot_id);
    vita_thread_info_dict_clear(g_vita_thread_infos);

    return 0;
}

VitaThreadInfo *SceKernelThreadMgr_get_thread_info(void)
{
    return threadTlsGet(g_vita_thread_info_tls_slot_id);
}
