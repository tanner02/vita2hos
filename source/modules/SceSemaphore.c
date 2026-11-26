#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include <psp2/kernel/error.h>
#include <psp2/kernel/threadmgr/semaphore.h>

#include "module.h"
#include "modules/SceSysmem.h"

typedef struct SemaWaiter {
    struct SemaWaiter *next;
    CondVar condition;
    int count, result;
    bool done;
} SemaWaiter;

typedef struct VitaSemaphore {
    struct VitaSemaphore *next;
    SceUID uid;
    char name[32];
    int initial, count, maximum;
    unsigned int references;
    SemaWaiter *waiters;
} VitaSemaphore;

/* libnx mutexes are zero-initialized. The table owns one reference; each
 * blocked wait owns another until its stack waiter has been unlinked. */
static Mutex sema_lock;
static VitaSemaphore *semaphores;

static VitaSemaphore *find_sema(SceUID uid)
{
    for (VitaSemaphore *s = semaphores; s; s = s->next)
        if (s->uid == uid) return s;
    return NULL;
}

static int sema_pending(VitaSemaphore *s)
{
    int count = 0;
    for (SemaWaiter *w = s->waiters; w; w = w->next)
        count += !w->done;
    return count;
}

static void sema_dispatch(VitaSemaphore *s)
{
    for (SemaWaiter *w = s->waiters; w; w = w->next) {
        if (w->done) continue;
        if (s->count < w->count) break; /* Strict FIFO, including large requests. */
        s->count -= w->count;
        w->result = 0;
        w->done = true;
        condvarWakeAll(&w->condition);
    }
}

EXPORT(SceLibKernel, 0x1BD67366, SceUID, sceKernelCreateSema, const char *name,
       SceUInt attr, int initial, int maximum, SceKernelSemaOptParam *option)
{
    if (!name) return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    /* Priority queues and named/openable semaphores need separate support. */
    if (attr != SCE_KERNEL_ATTR_THREAD_FIFO) return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
    if (maximum <= 0 || initial < 0 || initial > maximum)
        return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
    if (option && option->size != sizeof(*option))
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
    VitaSemaphore *s = calloc(1, sizeof(*s));
    if (!s) return SCE_KERNEL_ERROR_NO_MEMORY;
    const SceUID uid = s->uid = SceSysmem_get_next_uid();
    strncpy(s->name, name, sizeof(s->name) - 1);
    s->initial = s->count = initial;
    s->maximum = maximum;
    s->references = 1;
    mutexLock(&sema_lock);
    s->next = semaphores;
    semaphores = s;
    mutexUnlock(&sema_lock);
    return uid;
}

EXPORT(SceThreadmgr, 0xE6B761D1, int, sceKernelSignalSema, SceUID uid, int count)
{
    if (count <= 0) return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
    mutexLock(&sema_lock);
    VitaSemaphore *s = find_sema(uid);
    int result;
    if (!s) result = SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID;
    else if (count > s->maximum - s->count) result = SCE_KERNEL_ERROR_SEMA_OVF;
    else {
        s->count += count;
        sema_dispatch(s);
        result = 0;
    }
    mutexUnlock(&sema_lock);
    return result;
}

EXPORT(SceThreadmgr, 0x866EF048, int, sceKernelPollSema, SceUID uid, int count)
{
    if (count <= 0) return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
    mutexLock(&sema_lock);
    VitaSemaphore *s = find_sema(uid);
    int result;
    if (!s) result = SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID;
    else if (count > s->maximum) result = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
    else if (s->count < count) result = SCE_KERNEL_ERROR_SEMA_ZERO;
    else { s->count -= count; result = 0; }
    mutexUnlock(&sema_lock);
    return result;
}

EXPORT(SceLibKernel, 0x0C7B834B, int, sceKernelWaitSema, SceUID uid, int count,
       SceUInt *timeout)
{
    if (count <= 0) return SCE_KERNEL_ERROR_ILLEGAL_COUNT;
    SemaWaiter waiter = {.count = count};
    condvarInit(&waiter.condition);
    const uint64_t start = armGetSystemTick();
    const uint64_t duration = timeout ? (uint64_t)*timeout * 1000 : UINT64_MAX;
    mutexLock(&sema_lock);
    VitaSemaphore *s = find_sema(uid);
    if (!s || count > s->maximum) {
        int result = s ? SCE_KERNEL_ERROR_ILLEGAL_COUNT : SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID;
        mutexUnlock(&sema_lock);
        return result;
    }
    /* FIFO applies to blocked waiters. Match an immediately satisfiable
     * acquisition before joining that queue, as with sceKernelPollSema. */
    if (s->count >= count) {
        s->count -= count;
        if (timeout) {
            uint64_t elapsed = armTicksToNs(armGetSystemTick() - start);
            *timeout = elapsed >= duration ? 0 : (SceUInt)((duration - elapsed) / 1000);
        }
        mutexUnlock(&sema_lock);
        return 0;
    }
    ++s->references;
    SemaWaiter **tail = &s->waiters;
    while (*tail) tail = &(*tail)->next;
    *tail = &waiter;
    sema_dispatch(s);
    while (!waiter.done) {
        uint64_t elapsed = armTicksToNs(armGetSystemTick() - start);
        if (timeout && elapsed >= duration) {
            waiter.result = SCE_KERNEL_ERROR_WAIT_TIMEOUT;
            break;
        }
        Result result = condvarWaitTimeout(&waiter.condition, &sema_lock,
                                          timeout ? duration - elapsed : UINT64_MAX);
        if (R_FAILED(result) && result != 0xEA01 && !waiter.done) {
            waiter.result = SCE_KERNEL_ERROR_SEMA_ERROR;
            break;
        }
    }
    if (timeout) {
        uint64_t elapsed = armTicksToNs(armGetSystemTick() - start);
        *timeout = elapsed >= duration ? 0 : (SceUInt)((duration - elapsed) / 1000);
    }
    SemaWaiter **link = &s->waiters;
    while (*link != &waiter) link = &(*link)->next;
    *link = waiter.next;
    /* A timed-out head must not strand later requests behind unused tokens. */
    sema_dispatch(s);
    const int result = waiter.result;
    if (--s->references == 0) free(s);
    mutexUnlock(&sema_lock);
    return result;
}

EXPORT(SceLibKernel, 0x66D6BF05, int, sceKernelCancelSema, SceUID uid, int count,
       int *num_waiters)
{
    mutexLock(&sema_lock);
    VitaSemaphore *s = find_sema(uid);
    int result;
    if (!s) result = SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID;
    else if (count < -1 || count > s->maximum) result = SCE_KERNEL_ERROR_ILLEGAL_COUNT;
    else {
        if (num_waiters) *num_waiters = sema_pending(s);
        s->count = count == -1 ? s->initial : count;
        for (SemaWaiter *w = s->waiters; w; w = w->next) {
            if (w->done) continue;
            w->result = SCE_KERNEL_ERROR_WAIT_CANCEL;
            w->done = true;
            condvarWakeAll(&w->condition);
        }
        result = 0;
    }
    mutexUnlock(&sema_lock);
    return result;
}

EXPORT(SceThreadmgr, 0xDB32948A, int, sceKernelDeleteSema, SceUID uid)
{
    mutexLock(&sema_lock);
    VitaSemaphore **link = &semaphores;
    while (*link && (*link)->uid != uid) link = &(*link)->next;
    VitaSemaphore *s = *link;
    const bool found = s != NULL;
    if (s) {
        *link = s->next;
        for (SemaWaiter *w = s->waiters; w; w = w->next) {
            if (w->done) continue;
            w->result = SCE_KERNEL_ERROR_WAIT_DELETE;
            w->done = true;
            condvarWakeAll(&w->condition);
        }
        if (--s->references == 0) free(s);
    }
    mutexUnlock(&sema_lock);
    return found ? 0 : SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID;
}

EXPORT(SceLibKernel, 0x595D3FA6, int, sceKernelGetSemaInfo, SceUID uid,
       SceKernelSemaInfo *info)
{
    if (!info) return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    if (info->size != sizeof(*info)) return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
    mutexLock(&sema_lock);
    VitaSemaphore *s = find_sema(uid);
    if (s) {
        memset(info, 0, sizeof(*info));
        info->size = sizeof(*info);
        info->semaId = uid;
        memcpy(info->name, s->name, sizeof(info->name));
        info->initCount = s->initial;
        info->currentCount = s->count;
        info->maxCount = s->maximum;
        info->numWaitThreads = sema_pending(s);
    }
    const int result = s ? 0 : SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID;
    mutexUnlock(&sema_lock);
    return result;
}
