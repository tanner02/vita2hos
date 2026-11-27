#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include <psp2/kernel/error.h>
#include <psp2/kernel/threadmgr/lw_mutex.h>
#include <psp2/kernel/threadmgr/lw_cond.h>

#include "module.h"
#include "modules/SceKernelThreadMgr.h"
#include "modules/SceSysmem.h"

/* Work-area offsets from the pinned Vita3K kernel types. Native synchronization
 * objects are kept separately; they do not share this layout or owner IDs. */
typedef struct {
    uint32_t owner, unknown;
    int32_t count;
    uint32_t attr;
    SceUID uid;
    uint32_t reserved[3];
} LwWorkLayout;
_Static_assert(sizeof(LwWorkLayout) == sizeof(SceKernelLwMutexWork), "LwMutex work ABI");

typedef struct LwWaiter {
    struct LwWaiter *next;
    CondVar condition;
    SceUID owner;
    int count, priority, result;
    bool done;
} LwWaiter;

typedef struct LwMutex {
    struct LwMutex *next;
    SceKernelLwMutexWork *work;
    SceUID uid, owner;
    unsigned int attr, references;
    int count;
    LwWaiter *waiters;
    bool deleted;
} LwMutex;

static Mutex lw_lock;
static LwMutex *lw_mutexes;

static int validate_work(SceKernelLwMutexWork *work)
{
    if (!work) return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    if ((uintptr_t)work % _Alignof(SceKernelLwMutexWork))
        return SCE_KERNEL_ERROR_ILLEGAL_ALIGNMENT;
    return 0;
}

static LwMutex *find_lwmutex(SceKernelLwMutexWork *work)
{
    for (LwMutex *m = lw_mutexes; m; m = m->next)
        if (m->work == work) return m;
    return NULL;
}

static void publish_lwmutex(LwMutex *m, bool deleted)
{
    const uint32_t owner = deleted ? 0 : m->owner;
    const int32_t count = deleted ? 0 : m->count;
    const SceUID uid = deleted ? -1 : m->uid;
    memcpy((char *)m->work + offsetof(LwWorkLayout, owner), &owner, sizeof(owner));
    memcpy((char *)m->work + offsetof(LwWorkLayout, count), &count, sizeof(count));
    memcpy((char *)m->work + offsetof(LwWorkLayout, attr), &m->attr, sizeof(m->attr));
    memcpy((char *)m->work + offsetof(LwWorkLayout, uid), &uid, sizeof(uid));
}

static void handoff_lwmutex(LwMutex *m)
{
    if (m->count) return;
    m->owner = 0;
    for (LwWaiter *w = m->waiters; w; w = w->next) {
        if (w->done) continue;
        m->owner = w->owner;
        m->count = w->count;
        w->result = 0;
        w->done = true;
        condvarWakeAll(&w->condition);
        break;
    }
}

EXPORT(SceLibKernel, 0xDA6EC8EF, int, sceKernelCreateLwMutex, SceKernelLwMutexWork *work,
       const char *name, unsigned int attr, int count, const SceKernelLwMutexOptParam *option)
{
    int result = validate_work(work);
    if (result) return result;
    if (!name) return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    if (attr & ~(SCE_KERNEL_MUTEX_ATTR_RECURSIVE | SCE_KERNEL_ATTR_THREAD_PRIO))
        return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
    if (count < 0 || (!(attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE) && count > 1))
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
    if (option && option->size != sizeof(*option))
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
    VitaThreadInfo *thread = SceKernelThreadMgr_get_thread_info();
    if (count && (!thread || thread->uid <= 0)) return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    LwMutex *m = calloc(1, sizeof(*m));
    if (!m) return SCE_KERNEL_ERROR_NO_MEMORY;
    m->work = work;
    m->uid = SceSysmem_get_next_uid();
    m->owner = count ? thread->uid : 0;
    m->attr = attr;
    m->count = count;
    m->references = 1;
    mutexLock(&lw_lock);
    if (find_lwmutex(work)) {
        mutexUnlock(&lw_lock);
        free(m);
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }
    memset(work, 0, sizeof(*work));
    publish_lwmutex(m, false);
    m->next = lw_mutexes;
    lw_mutexes = m;
    mutexUnlock(&lw_lock);
    return 0;
}

/* Caller holds lw_lock and owns a reference until this function can enqueue
 * its own waiter. The condition path uses the original object, never a reused
 * work-area address. The deadline covers both condition and mutex phases. */
static int acquire_lwmutex_locked(LwMutex *m, VitaThreadInfo *thread, int count,
                                  bool only_try, uint64_t start, uint64_t duration,
                                  bool timed)
{
    if (m->deleted) return SCE_KERNEL_ERROR_WAIT_DELETE;
    if (!(m->attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE) && count != 1)
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
    if (!m->count || m->owner == thread->uid) {
        if (m->count && !(m->attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE))
            return SCE_KERNEL_ERROR_LW_MUTEX_RECURSIVE;
        if (count > INT_MAX - m->count) return SCE_KERNEL_ERROR_LW_MUTEX_LOCK_OVF;
        m->owner = thread->uid;
        m->count += count;
        publish_lwmutex(m, false);
        return 0;
    }
    if (only_try) return SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN;
    LwWaiter waiter = {.owner = thread->uid, .count = count, .priority = thread->priority};
    condvarInit(&waiter.condition);
    ++m->references;
    LwWaiter **tail = &m->waiters;
    while (*tail && ((*tail)->done || !(m->attr & SCE_KERNEL_ATTR_THREAD_PRIO) ||
                     (*tail)->priority <= waiter.priority)) tail = &(*tail)->next;
    waiter.next = *tail;
    *tail = &waiter;
    while (!waiter.done) {
        uint64_t elapsed = armTicksToNs(armGetSystemTick() - start);
        if (timed && elapsed >= duration) {
            waiter.result = SCE_KERNEL_ERROR_WAIT_TIMEOUT;
            break;
        }
        Result res = condvarWaitTimeout(&waiter.condition, &lw_lock,
                                       timed ? duration - elapsed : UINT64_MAX);
        if (R_FAILED(res) && res != 0xEA01 && !waiter.done) {
            waiter.result = SCE_KERNEL_ERROR_LW_MUTEX_ERROR;
            break;
        }
    }
    LwWaiter **link = &m->waiters;
    while (*link != &waiter) link = &(*link)->next;
    *link = waiter.next;
    int result = waiter.result;
    if (m->deleted) result = SCE_KERNEL_ERROR_WAIT_DELETE;
    if (--m->references == 0) free(m);
    return result;
}

static int lock_lwmutex(SceKernelLwMutexWork *work, int count, unsigned int *timeout,
                        bool only_try)
{
    int result = validate_work(work);
    if (result) return result;
    if (count <= 0) return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
    VitaThreadInfo *thread = SceKernelThreadMgr_get_thread_info();
    if (!thread || thread->uid <= 0) return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    const uint64_t start = armGetSystemTick();
    const uint64_t duration = timeout ? (uint64_t)*timeout * 1000 : UINT64_MAX;
    mutexLock(&lw_lock);
    LwMutex *m = find_lwmutex(work);
    if (m) result=acquire_lwmutex_locked(m,thread,count,only_try,start,duration,timeout != NULL);
    else result=SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID;
    if (timeout) {
        uint64_t elapsed = armTicksToNs(armGetSystemTick() - start);
        *timeout = elapsed >= duration ? 0 : (unsigned int)((duration - elapsed) / 1000);
    }
    mutexUnlock(&lw_lock);
    return result;
}

EXPORT(SceLibKernel, 0x46E7BE7B, int, sceKernelLockLwMutex, SceKernelLwMutexWork *work,
       int count, unsigned int *timeout)
{
    return lock_lwmutex(work, count, timeout, false);
}

EXPORT(SceLibKernel, 0xA6A2C915, int, sceKernelTryLockLwMutex, SceKernelLwMutexWork *work,
       int count)
{
    return lock_lwmutex(work, count, NULL, true);
}

EXPORT(SceLibKernel, 0x91FA6614, int, sceKernelUnlockLwMutex, SceKernelLwMutexWork *work,
       int count)
{
    int result = validate_work(work);
    if (result) return result;
    if (count <= 0) return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
    VitaThreadInfo *thread = SceKernelThreadMgr_get_thread_info();
    if (!thread || thread->uid <= 0) return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    mutexLock(&lw_lock);
    LwMutex *m = find_lwmutex(work);
    if (!m) result = SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID;
    else if (!m->count || m->owner != thread->uid) result = SCE_KERNEL_ERROR_LW_MUTEX_NOT_OWNED;
    else if (count > m->count) result = SCE_KERNEL_ERROR_LW_MUTEX_UNLOCK_UDF;
    else {
        m->count -= count;
        handoff_lwmutex(m);
        publish_lwmutex(m, false);
        result = 0;
    }
    mutexUnlock(&lw_lock);
    return result;
}

EXPORT(SceLibKernel, 0x120AFC8C, int, sceKernelUnlockLwMutex2, SceKernelLwMutexWork *work,
       int count)
{
    return sceKernelUnlockLwMutex(work, count);
}

static void delete_lwmutex_conditions(LwMutex *m);

EXPORT(SceLibKernel, 0x244E76D2, int, sceKernelDeleteLwMutex, SceKernelLwMutexWork *work)
{
    int result = validate_work(work);
    if (result) return result;
    mutexLock(&lw_lock);
    LwMutex **link = &lw_mutexes;
    while (*link && (*link)->work != work) link = &(*link)->next;
    LwMutex *m = *link;
    const bool found = m != NULL;
    if (m) {
        *link = m->next;
        m->deleted = true;
        publish_lwmutex(m, true);
        delete_lwmutex_conditions(m);
        for (LwWaiter *w = m->waiters; w; w = w->next) {
            if (w->done) continue;
            w->result = SCE_KERNEL_ERROR_WAIT_DELETE;
            w->done = true;
            condvarWakeAll(&w->condition);
        }
        if (--m->references == 0) free(m);
    }
    mutexUnlock(&lw_lock);
    return found ? 0 : SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID;
}

/* Share lw_lock with mutex ownership: publishing the waiter and dropping one
 * mutex count must be atomic with respect to both unlock and signal. */
typedef struct LwCond {
    struct LwCond *next;
    SceKernelLwCondWork *work;
    LwMutex *mutex;
    SceUID uid;
    unsigned int attr, references;
    LwWaiter *waiters;
} LwCond;
static LwCond *lw_conditions;

static int validate_cond_work(SceKernelLwCondWork *work)
{
    if (!work) return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    return (uintptr_t)work % _Alignof(SceKernelLwCondWork)
        ? SCE_KERNEL_ERROR_ILLEGAL_ALIGNMENT : 0;
}

static LwCond *find_lwcond(SceKernelLwCondWork *work)
{
    for (LwCond *c=lw_conditions;c;c=c->next) if (c->work==work) return c;
    return NULL;
}

static void release_lwcond_locked(LwCond *c)
{
    if (--c->references) return;
    if (--c->mutex->references==0) free(c->mutex);
    free(c);
}

static void finish_cond_waiter(LwWaiter *waiter, int result)
{
    waiter->result=result;
    waiter->done=true;
    condvarWakeAll(&waiter->condition);
}

static void delete_lwmutex_conditions(LwMutex *m)
{
    for (LwCond *c=lw_conditions;c;c=c->next) if (c->mutex==m)
        for (LwWaiter *w=c->waiters;w;w=w->next) if (!w->done)
            finish_cond_waiter(w,SCE_KERNEL_ERROR_WAIT_DELETE_LW_MUTEX);
}

EXPORT(SceLibKernel, 0x48C7EAE6, int, sceKernelCreateLwCond, SceKernelLwCondWork *work,
       const char *name, unsigned int attr, SceKernelLwMutexWork *mutex_work,
       const SceKernelLwCondOptParam *option)
{
    int result=validate_cond_work(work);
    if (result) return result;
    if (!name) return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    result=validate_work(mutex_work);
    if (result) return result;
    if (attr & ~SCE_KERNEL_ATTR_THREAD_PRIO) return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
    if (option && option->size!=sizeof(*option)) return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
    LwCond *c=calloc(1,sizeof(*c));
    if (!c) return SCE_KERNEL_ERROR_NO_MEMORY;
    mutexLock(&lw_lock);
    LwMutex *m=find_lwmutex(mutex_work);
    if (!m) result=SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID;
    else if (find_lwcond(work)) result=SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    else {
        c->work=work;c->mutex=m;c->uid=SceSysmem_get_next_uid();
        c->attr=attr;c->references=1;
        ++m->references;
        memset(work,0,sizeof(*work));
        /* VitaSDK defines 16 bytes; only the first word is the kernel UID. */
        memcpy(work,&c->uid,sizeof(c->uid));
        c->next=lw_conditions;lw_conditions=c;
        result=0;
    }
    mutexUnlock(&lw_lock);
    if (result) free(c);
    return result;
}

EXPORT(SceLibKernel, 0x721F6CB3, int, sceKernelDeleteLwCond, SceKernelLwCondWork *work)
{
    int result=validate_cond_work(work);
    if (result) return result;
    mutexLock(&lw_lock);
    LwCond **link=&lw_conditions;
    while (*link && (*link)->work!=work) link=&(*link)->next;
    LwCond *c=*link;
    result=c ? 0 : SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID;
    if (c) {
        *link=c->next;
        SceUID deleted=-1;memcpy(work,&deleted,sizeof(deleted));
        for (LwWaiter *w=c->waiters;w;w=w->next) if (!w->done)
            finish_cond_waiter(w,SCE_KERNEL_ERROR_WAIT_DELETE_LW_COND);
        release_lwcond_locked(c);
    }
    mutexUnlock(&lw_lock);
    return result;
}

static int signal_lwcond(SceKernelLwCondWork *work, bool all, SceUID target)
{
    int result=validate_cond_work(work);
    if (result) return result;
    mutexLock(&lw_lock);
    LwCond *c=find_lwcond(work);
    result=c ? 0 : SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID;
    if (c) for (LwWaiter *w=c->waiters;w;w=w->next) {
        if (w->done || (target && w->owner!=target)) continue;
        finish_cond_waiter(w,0);
        if (!all) break;
    }
    mutexUnlock(&lw_lock);
    return result;
}

EXPORT(SceLibKernel, 0x3AC63B9A, int, sceKernelSignalLwCond, SceKernelLwCondWork *work)
{ return signal_lwcond(work,false,0); }
EXPORT(SceLibKernel, 0xE5241A0C, int, sceKernelSignalLwCondAll, SceKernelLwCondWork *work)
{ return signal_lwcond(work,true,0); }
EXPORT(SceLibKernel, 0xFC1A48EB, int, sceKernelSignalLwCondTo, SceKernelLwCondWork *work,
       SceUID target)
{
    if (target<=0) return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
    return signal_lwcond(work,false,target);
}

EXPORT(SceLibKernel, 0xE1878282, int, sceKernelWaitLwCond, SceKernelLwCondWork *work,
       unsigned int *timeout)
{
    int result=validate_cond_work(work);
    if (result) return result;
    VitaThreadInfo *thread=SceKernelThreadMgr_get_thread_info();
    if (!thread || thread->uid<=0) return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    uint64_t start=armGetSystemTick();
    uint64_t duration=timeout ? (uint64_t)*timeout*1000 : UINT64_MAX;
    mutexLock(&lw_lock);
    LwCond *c=find_lwcond(work);
    if (!c) { result=SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID;goto done; }
    LwMutex *m=c->mutex;
    if (m->deleted) { result=SCE_KERNEL_ERROR_WAIT_DELETE_LW_MUTEX;goto done; }
    if (!m->count || m->owner!=thread->uid) {
        result=SCE_KERNEL_ERROR_LW_MUTEX_NOT_OWNED;goto done;
    }
    LwWaiter waiter={.owner=thread->uid,.priority=thread->priority};
    condvarInit(&waiter.condition);
    ++c->references;
    LwWaiter **tail=&c->waiters;
    while (*tail && ((*tail)->done || !(c->attr & SCE_KERNEL_ATTR_THREAD_PRIO) ||
                     (*tail)->priority<=waiter.priority)) tail=&(*tail)->next;
    waiter.next=*tail;*tail=&waiter;
    --m->count;
    handoff_lwmutex(m);
    publish_lwmutex(m,false);
    while (!waiter.done) {
        uint64_t elapsed=armTicksToNs(armGetSystemTick()-start);
        if (timeout && elapsed>=duration) { waiter.result=SCE_KERNEL_ERROR_WAIT_TIMEOUT;break; }
        Result res=condvarWaitTimeout(&waiter.condition,&lw_lock,
                                      timeout ? duration-elapsed : UINT64_MAX);
        if (R_FAILED(res) && res!=0xEA01 && !waiter.done) {
            waiter.result=SCE_KERNEL_ERROR_LW_COND_ERROR;break;
        }
    }
    LwWaiter **link=&c->waiters;
    while (*link!=&waiter) link=&(*link)->next;
    *link=waiter.next;
    result=waiter.result;
    /* As in the pinned Vita3K reference, errors return without reacquisition;
     * a signal reacquires one count using the remaining original timeout. */
    if (!result) {
        result=acquire_lwmutex_locked(m,thread,1,false,start,duration,timeout!=NULL);
        if ((uint32_t)result==SCE_KERNEL_ERROR_WAIT_DELETE)
            result=SCE_KERNEL_ERROR_WAIT_DELETE_LW_MUTEX;
    }
    release_lwcond_locked(c);
done:
    if (timeout) {
        uint64_t elapsed=armTicksToNs(armGetSystemTick()-start);
        *timeout=elapsed>=duration ? 0 : (unsigned int)((duration-elapsed)/1000);
    }
    mutexUnlock(&lw_lock);
    return result;
}
