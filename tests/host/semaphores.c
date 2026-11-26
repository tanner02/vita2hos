#define TEST_THREADED_MUTEX
#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <unistd.h>

#include "../../source/modules/SceSemaphore.c"

SceUID SceSysmem_get_next_uid(void)
{
    static atomic_int next = 1;
    return atomic_fetch_add(&next, 1);
}

typedef struct { SceUID uid; int count, result; SceUInt timeout; bool forever; } Wait;
static void *wait_thread(void *argument)
{
    Wait *w = argument;
    w->result = sceKernelWaitSema(w->uid, w->count, w->forever ? NULL : &w->timeout);
    return NULL;
}
static SceKernelSemaInfo get_info(SceUID uid)
{
    SceKernelSemaInfo info = {.size = sizeof(info)};
    assert(sceKernelGetSemaInfo(uid, &info) == 0);
    return info;
}
static void wait_pending(SceUID uid, int count)
{
    uint64_t start = armGetSystemTick();
    while (get_info(uid).numWaitThreads != count) {
        assert(armTicksToNs(armGetSystemTick() - start) < 1000000000);
        sched_yield();
    }
}
static atomic_bool stop_deleting;
static void *delete_concurrently(void *unused)
{
    (void)unused;
    while (!atomic_load(&stop_deleting)) {
        mutexLock(&sema_lock);
        SceUID uid = semaphores ? semaphores->uid : -1;
        mutexUnlock(&sema_lock);
        if (uid > 0) sceKernelDeleteSema(uid);
        sched_yield();
    }
    return NULL;
}

int main(void)
{
    alarm(15);
    mutexInit(&sema_lock);
    assert((uint32_t)sceKernelCreateSema(NULL, 0, 0, 1, NULL) == SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    assert((uint32_t)sceKernelCreateSema("bad", 0, 0, 0, NULL) == SCE_KERNEL_ERROR_ILLEGAL_COUNT);
    assert((uint32_t)sceKernelCreateSema("bad", 0, -1, 1, NULL) == SCE_KERNEL_ERROR_ILLEGAL_COUNT);
    assert((uint32_t)sceKernelCreateSema("bad", 0, 2, 1, NULL) == SCE_KERNEL_ERROR_ILLEGAL_COUNT);
    assert((uint32_t)sceKernelCreateSema("bad", SCE_KERNEL_ATTR_THREAD_PRIO, 0, 1, NULL) ==
           SCE_KERNEL_ERROR_ILLEGAL_ATTR);
    SceKernelSemaOptParam option = {.size = 0};
    assert((uint32_t)sceKernelCreateSema("bad", 0, 0, 1, &option) ==
           SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);
    option.size = sizeof(option);
    SceUID uid = sceKernelCreateSema("tokens", 0, 2, 3, &option);
    assert(uid > 0);
    SceKernelSemaInfo info = get_info(uid);
    assert(info.semaId == uid && !strcmp(info.name, "tokens") && info.attr == 0);
    assert(info.initCount == 2 && info.currentCount == 2 && info.maxCount == 3);
    assert(sceKernelPollSema(uid, 2) == 0);
    assert((uint32_t)sceKernelPollSema(uid, 1) == SCE_KERNEL_ERROR_SEMA_ZERO);
    assert((uint32_t)sceKernelWaitSema(uid, 4, NULL) == SCE_KERNEL_ERROR_ILLEGAL_COUNT);
    assert((uint32_t)sceKernelPollSema(uid, 0) == SCE_KERNEL_ERROR_ILLEGAL_COUNT);
    assert((uint32_t)sceKernelSignalSema(uid, -1) == SCE_KERNEL_ERROR_ILLEGAL_COUNT);
    assert((uint32_t)sceKernelSignalSema(uid, 4) == SCE_KERNEL_ERROR_SEMA_OVF);
    assert(get_info(uid).currentCount == 0);
    assert((uint32_t)sceKernelGetSemaInfo(uid, NULL) == SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    info.size = 0;
    assert((uint32_t)sceKernelGetSemaInfo(uid, &info) == SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);
    SceUInt timeout = 0;
    assert((uint32_t)sceKernelWaitSema(uid, 1, &timeout) == SCE_KERNEL_ERROR_WAIT_TIMEOUT);
    assert(timeout == 0);
    assert(sceKernelSignalSema(uid, 1) == 0);
    assert(sceKernelWaitSema(uid, 1, &timeout) == 0);

    Wait first = {.uid = uid, .count = 2, .timeout = 2000000};
    Wait second = {.uid = uid, .count = 1, .timeout = 2000000};
    pthread_t a, b;
    assert(pthread_create(&a, NULL, wait_thread, &first) == 0);
    wait_pending(uid, 1);
    assert(pthread_create(&b, NULL, wait_thread, &second) == 0);
    wait_pending(uid, 2);
    assert(sceKernelSignalSema(uid, 1) == 0);
    assert(get_info(uid).currentCount == 1 && get_info(uid).numWaitThreads == 2);
    assert((uint32_t)sceKernelPollSema(uid, 1) == SCE_KERNEL_ERROR_SEMA_ZERO);
    assert(sceKernelSignalSema(uid, 1) == 0);
    assert(pthread_join(a, NULL) == 0 && first.result == 0);
    wait_pending(uid, 1);
    assert(first.timeout > 0 && first.timeout <= 2000000);
    assert(sceKernelSignalSema(uid, 1) == 0);
    assert(pthread_join(b, NULL) == 0 && second.result == 0);

    /* If the FIFO head times out, the smaller request must consume the token
     * already present, without requiring an additional signal. */
    first = (Wait){.uid = uid, .count = 2, .timeout = 500000};
    second = (Wait){.uid = uid, .count = 1, .timeout = 2000000};
    assert(pthread_create(&a, NULL, wait_thread, &first) == 0);
    wait_pending(uid, 1);
    assert(pthread_create(&b, NULL, wait_thread, &second) == 0);
    wait_pending(uid, 2);
    assert(sceKernelSignalSema(uid, 1) == 0);
    assert(pthread_join(a, NULL) == 0);
    assert((uint32_t)first.result == SCE_KERNEL_ERROR_WAIT_TIMEOUT && first.timeout == 0);
    assert(pthread_join(b, NULL) == 0 && second.result == 0);
    assert(get_info(uid).currentCount == 0);

    first = (Wait){.uid = uid, .count = 1, .forever = true};
    assert(pthread_create(&a, NULL, wait_thread, &first) == 0);
    wait_pending(uid, 1);
    int canceled = -1;
    assert((uint32_t)sceKernelCancelSema(uid, 4, &canceled) == SCE_KERNEL_ERROR_ILLEGAL_COUNT);
    assert(canceled == -1 && get_info(uid).numWaitThreads == 1);
    assert(sceKernelCancelSema(uid, -1, &canceled) == 0 && canceled == 1);
    assert(pthread_join(a, NULL) == 0 && (uint32_t)first.result == SCE_KERNEL_ERROR_WAIT_CANCEL);
    assert(get_info(uid).currentCount == 2 && sceKernelPollSema(uid, 2) == 0);

    first = (Wait){.uid = uid, .count = 1, .timeout = 2000000};
    second = first;
    assert(pthread_create(&a, NULL, wait_thread, &first) == 0);
    wait_pending(uid, 1);
    assert(pthread_create(&b, NULL, wait_thread, &second) == 0);
    wait_pending(uid, 2);
    assert(sceKernelDeleteSema(uid) == 0);
    assert(pthread_join(a, NULL) == 0 && (uint32_t)first.result == SCE_KERNEL_ERROR_WAIT_DELETE);
    assert(pthread_join(b, NULL) == 0 && (uint32_t)second.result == SCE_KERNEL_ERROR_WAIT_DELETE);
    assert((uint32_t)sceKernelSignalSema(uid, 1) == SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);
    assert((uint32_t)sceKernelDeleteSema(uid) == SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);

    uid = sceKernelCreateSema("large", 0, INT_MAX - 1, INT_MAX, NULL);
    assert(uid > 0);
    assert((uint32_t)sceKernelSignalSema(uid, 2) == SCE_KERNEL_ERROR_SEMA_OVF);
    assert(get_info(uid).currentCount == INT_MAX - 1);
    assert(sceKernelSignalSema(uid, 1) == 0 && sceKernelPollSema(uid, INT_MAX) == 0);
    assert(sceKernelDeleteSema(uid) == 0);
    assert(pthread_create(&a, NULL, delete_concurrently, NULL) == 0);
    for (int i = 0; i < 500; ++i) {
        uid = sceKernelCreateSema("racing", 0, 1, 1, NULL);
        assert(uid > 0);
        int result = sceKernelPollSema(uid, 1);
        assert(result == 0 || (uint32_t)result == SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);
        result = sceKernelDeleteSema(uid);
        assert(result == 0 || (uint32_t)result == SCE_KERNEL_ERROR_UNKNOWN_SEMA_ID);
    }
    atomic_store(&stop_deleting, true);
    assert(pthread_join(a, NULL) == 0 && semaphores == NULL);
    pthread_mutex_destroy(&sema_lock);
    puts("Semaphore threaded regression checks passed.");
    return 0;
}
