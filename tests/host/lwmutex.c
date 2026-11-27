#define TEST_THREADED_MUTEX
#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <unistd.h>

#include "../../source/modules/SceLwMutex.c"

SceUID SceSysmem_get_next_uid(void)
{
    static atomic_int next = 1;
    return atomic_fetch_add(&next, 1);
}
static _Thread_local VitaThreadInfo current_thread;
VitaThreadInfo *SceKernelThreadMgr_get_thread_info(void) { return &current_thread; }
static LwWorkLayout mirror(SceKernelLwMutexWork *work)
{
    LwWorkLayout state;
    memcpy(&state, work, sizeof(state));
    return state;
}
typedef struct {
    SceKernelLwMutexWork *work;
    int priority, count, result, unlock_result, order;
    bool probe, forever;
    unsigned int timeout;
} Worker;
static atomic_int sequence;
static void *worker(void *argument)
{
    Worker *w = argument;
    current_thread.uid = SceSysmem_get_next_uid();
    current_thread.priority = w->priority;
    if (w->probe) {
        w->result = sceKernelTryLockLwMutex(w->work, w->count);
        w->unlock_result = sceKernelUnlockLwMutex(w->work, w->count);
        return NULL;
    }
    w->result = sceKernelLockLwMutex(w->work, w->count, w->forever ? NULL : &w->timeout);
    if (!w->result) {
        LwWorkLayout state = mirror(w->work);
        assert(state.owner == (uint32_t)current_thread.uid && state.count == w->count);
        w->order = atomic_fetch_add(&sequence, 1);
        w->unlock_result = sceKernelUnlockLwMutex(w->work, w->count);
        assert(w->unlock_result == 0);
    }
    return NULL;
}
static void wait_pending(SceKernelLwMutexWork *work, int expected)
{
    uint64_t start = armGetSystemTick();
    for (;;) {
        mutexLock(&lw_lock);
        LwMutex *m = find_lwmutex(work);
        assert(m);
        int count = 0;
        for (LwWaiter *w = m->waiters; w; w = w->next) count += !w->done;
        mutexUnlock(&lw_lock);
        if (count == expected) return;
        assert(armTicksToNs(armGetSystemTick() - start) < 1000000000);
        sched_yield();
    }
}

static void check_queue(unsigned int attr)
{
    SceKernelLwMutexWork work;
    assert(sceKernelCreateLwMutex(&work, "queue", attr, 1, NULL) == 0);
    const int count = attr & SCE_KERNEL_MUTEX_ATTR_RECURSIVE ? 2 : 1;
    Worker low = {.work = &work, .priority = 65, .count = count, .timeout = 2000000};
    Worker high = {.work = &work, .priority = 64, .count = count, .timeout = 2000000};
    Worker tie = high;
    pthread_t a, b, c;
    atomic_store(&sequence, 0);
    assert(pthread_create(&a, NULL, worker, &low) == 0);
    wait_pending(&work, 1);
    assert(pthread_create(&b, NULL, worker, &high) == 0);
    wait_pending(&work, 2);
    assert(pthread_create(&c, NULL, worker, &tie) == 0);
    wait_pending(&work, 3);
    assert(sceKernelUnlockLwMutex(&work, 1) == 0);
    assert(pthread_join(a, NULL) == 0 && low.result == 0);
    assert(pthread_join(b, NULL) == 0 && high.result == 0);
    assert(pthread_join(c, NULL) == 0 && tie.result == 0);
    if (attr & SCE_KERNEL_ATTR_THREAD_PRIO)
        assert(high.order == 0 && tie.order == 1 && low.order == 2);
    else
        assert(low.order == 0 && high.order == 1 && tie.order == 2);
    assert(mirror(&work).owner == 0 && mirror(&work).count == 0);
    assert(sceKernelDeleteLwMutex(&work) == 0);
}

int main(void)
{
    alarm(15);
    mutexInit(&lw_lock);
    current_thread.uid = SceSysmem_get_next_uid();
    current_thread.priority = 160;
    SceKernelLwMutexWork work;
    assert((uint32_t)sceKernelCreateLwMutex(NULL, "bad", 0, 0, NULL) == SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    _Alignas(8) unsigned char unaligned[sizeof(work) + 8];
    assert((uint32_t)sceKernelCreateLwMutex((void *)(unaligned + 1), "bad", 0, 0, NULL) ==
           SCE_KERNEL_ERROR_ILLEGAL_ALIGNMENT);
    SceKernelLwMutexOptParam option = {.size = 0};
    assert((uint32_t)sceKernelCreateLwMutex(&work, "bad", 0, 0, &option) ==
           SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);
    assert((uint32_t)sceKernelCreateLwMutex(&work, "bad", 0, 2, NULL) == SCE_KERNEL_ERROR_ILLEGAL_COUNT);
    assert((uint32_t)sceKernelCreateLwMutex(&work, "bad", SCE_KERNEL_MUTEX_ATTR_CEILING, 0, NULL) ==
           SCE_KERNEL_ERROR_ILLEGAL_ATTR);
    assert(sceKernelCreateLwMutex(&work, "recursive", SCE_KERNEL_MUTEX_ATTR_RECURSIVE, 2, NULL) == 0);
    assert(mirror(&work).uid > 0 && mirror(&work).count == 2);
    assert(mirror(&work).owner == (uint32_t)current_thread.uid);
    assert(mirror(&work).attr == SCE_KERNEL_MUTEX_ATTR_RECURSIVE);
    assert((uint32_t)sceKernelCreateLwMutex(&work, "duplicate", 0, 0, NULL) ==
           SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    assert(sceKernelTryLockLwMutex(&work, 3) == 0 && mirror(&work).count == 5);
    assert((uint32_t)sceKernelTryLockLwMutex(&work, 0) == SCE_KERNEL_ERROR_ILLEGAL_COUNT);
    assert(sceKernelUnlockLwMutex(&work, 4) == 0 && mirror(&work).count == 1);
    assert((uint32_t)sceKernelUnlockLwMutex(&work, 2) == SCE_KERNEL_ERROR_LW_MUTEX_UNLOCK_UDF);
    assert(mirror(&work).count == 1);
    Worker probe = {.work = &work, .priority = 160, .count = 1, .probe = true};
    pthread_t thread;
    assert(pthread_create(&thread, NULL, worker, &probe) == 0);
    assert(pthread_join(thread, NULL) == 0);
    assert((uint32_t)probe.result == SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN);
    assert((uint32_t)probe.unlock_result == SCE_KERNEL_ERROR_LW_MUTEX_NOT_OWNED);
    assert(mirror(&work).count == 1);
    Worker timed = {.work = &work, .priority = 160, .count = 1, .timeout = 200000};
    assert(pthread_create(&thread, NULL, worker, &timed) == 0);
    wait_pending(&work, 1);
    assert(pthread_join(thread, NULL) == 0);
    assert((uint32_t)timed.result == SCE_KERNEL_ERROR_WAIT_TIMEOUT && timed.timeout == 0);
    assert(mirror(&work).count == 1 && mirror(&work).owner == (uint32_t)current_thread.uid);
    assert(sceKernelUnlockLwMutex2(&work, 1) == 0);
    assert((uint32_t)sceKernelUnlockLwMutex(&work, 1) == SCE_KERNEL_ERROR_LW_MUTEX_NOT_OWNED);
    assert(sceKernelDeleteLwMutex(&work) == 0 && mirror(&work).uid == -1);
    assert((uint32_t)sceKernelTryLockLwMutex(&work, 1) == SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID);

    assert(sceKernelCreateLwMutex(&work, "overflow", SCE_KERNEL_MUTEX_ATTR_RECURSIVE, INT_MAX, NULL) == 0);
    assert((uint32_t)sceKernelTryLockLwMutex(&work, 1) == SCE_KERNEL_ERROR_LW_MUTEX_LOCK_OVF);
    assert(mirror(&work).count == INT_MAX);
    assert(sceKernelUnlockLwMutex(&work, INT_MAX) == 0);
    assert(sceKernelDeleteLwMutex(&work) == 0);
    assert(sceKernelCreateLwMutex(&work, "nonrecursive", 0, 1, NULL) == 0);
    assert((uint32_t)sceKernelTryLockLwMutex(&work, 1) == SCE_KERNEL_ERROR_LW_MUTEX_RECURSIVE);
    Worker deleted = {.work = &work, .priority = 160, .count = 1, .forever = true};
    assert(pthread_create(&thread, NULL, worker, &deleted) == 0);
    wait_pending(&work, 1);
    assert(sceKernelDeleteLwMutex(&work) == 0);
    assert(sceKernelCreateLwMutex(&work, "replacement", 0, 0, NULL) == 0);
    SceUID replacement = mirror(&work).uid;
    assert(pthread_join(thread, NULL) == 0 && (uint32_t)deleted.result == SCE_KERNEL_ERROR_WAIT_DELETE);
    assert(mirror(&work).uid == replacement && mirror(&work).count == 0);
    assert(sceKernelDeleteLwMutex(&work) == 0);
    check_queue(0);
    check_queue(SCE_KERNEL_ATTR_THREAD_PRIO);
    check_queue(SCE_KERNEL_ATTR_THREAD_PRIO | SCE_KERNEL_MUTEX_ATTR_RECURSIVE);
    assert(lw_mutexes == NULL);
    pthread_mutex_destroy(&lw_lock);
    puts("Lightweight mutex threaded regression checks passed.");
    return 0;
}
