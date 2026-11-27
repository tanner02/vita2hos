#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include <psp2/kernel/error.h>
#include <psp2/kernel/threadmgr/lw_mutex.h>

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
    if (!m) { result = SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID; goto done; }
    if (!(m->attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE) && count != 1) {
        result = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
        goto done;
    }
    if (!m->count || m->owner == thread->uid) {
        if (m->count && !(m->attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE))
            result = SCE_KERNEL_ERROR_LW_MUTEX_RECURSIVE;
        else if (count > INT_MAX - m->count)
            result = SCE_KERNEL_ERROR_LW_MUTEX_LOCK_OVF;
        else {
            m->owner = thread->uid;
            m->count += count;
            publish_lwmutex(m, false);
            result = 0;
        }
        goto done;
    }
    if (only_try) { result = SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN; goto done; }
    LwWaiter waiter = {.owner = thread->uid, .count = count, .priority = thread->priority};
    condvarInit(&waiter.condition);
    ++m->references;
    LwWaiter **tail = &m->waiters;
    while (*tail && ((*tail)->done || !(m->attr & SCE_KERNEL_ATTR_THREAD_PRIO) ||
                     (*tail)->priority <= waiter.priority))
        tail = &(*tail)->next;
    waiter.next = *tail;
    *tail = &waiter;
    while (!waiter.done) {
        uint64_t elapsed = armTicksToNs(armGetSystemTick() - start);
        if (timeout && elapsed >= duration) {
            waiter.result = SCE_KERNEL_ERROR_WAIT_TIMEOUT;
            break;
        }
        Result res = condvarWaitTimeout(&waiter.condition, &lw_lock,
                                       timeout ? duration - elapsed : UINT64_MAX);
        if (R_FAILED(res) && res != 0xEA01 && !waiter.done) {
            waiter.result = SCE_KERNEL_ERROR_LW_MUTEX_ERROR;
            break;
        }
    }
    LwWaiter **link = &m->waiters;
    while (*link != &waiter) link = &(*link)->next;
    *link = waiter.next;
    result = waiter.result;
    if (--m->references == 0) free(m);
done:
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
        publish_lwmutex(m, true);
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
