#define TEST_THREADED_MUTEX
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <unistd.h>

#include "../../source/modules/SceEventFlag.c"

SceUID SceSysmem_get_next_uid(void) { static SceUID next = 1; return next++; }

typedef struct { SceUID uid; uint32_t mode, bits, observed; int result; } Wait;
static void *wait_thread(void *arg)
{
    Wait *w = arg;
    SceUInt timeout = 2000000;
    w->result = sceKernelWaitEventFlag(w->uid, w->bits, w->mode, &w->observed, &timeout);
    return NULL;
}
static void wait_pending(SceUID uid, unsigned int count)
{
    uint64_t start = armGetSystemTick();
    for (;;) {
        SceKernelEventFlagInfo info = {.size = sizeof(info)};
        assert(sceKernelGetEventFlagInfo(uid, &info) == 0);
        if ((unsigned int)info.numWaitThreads == count)
            return;
        assert(armGetSystemTick() - start < 1000000000);
        sched_yield();
    }
}

int main(void)
{
    mutexInit(&event_lock);
    SceUID uid = sceKernelCreateEventFlag("event", SCE_EVENT_WAITMULTIPLE, 3, NULL);
    assert(uid > 0);
    uint32_t observed;
    assert(sceKernelPollEventFlag(uid, 3, SCE_EVENT_WAITAND | SCE_EVENT_WAITCLEAR_PAT, &observed) == 0);
    assert(observed == 3);
    assert((uint32_t)sceKernelPollEventFlag(uid, 1, 0, NULL) == SCE_KERNEL_ERROR_EVF_COND);
    assert(sceKernelSetEventFlag(uid, 7) == 0);
    assert(sceKernelClearEventFlag(uid, 5) == 0);
    assert(sceKernelPollEventFlag(uid, 4, SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR, &observed) == 0);
    assert(observed == 5);
    assert((uint32_t)sceKernelPollEventFlag(uid, 0, 0, NULL) == SCE_KERNEL_ERROR_ILLEGAL_PATTERN);
    assert((uint32_t)sceKernelPollEventFlag(uid, 1, 6, NULL) == SCE_KERNEL_ERROR_ILLEGAL_MODE);
    SceUInt timeout = 1000;
    assert((uint32_t)sceKernelWaitEventFlag(uid, 1, 0, NULL, &timeout) == SCE_KERNEL_ERROR_WAIT_TIMEOUT);
    assert(timeout == 0);

    Wait first = {.uid = uid, .bits = 1, .mode = SCE_EVENT_WAITCLEAR_PAT};
    Wait second = first;
    pthread_t a, b;
    assert(pthread_create(&a, NULL, wait_thread, &first) == 0);
    wait_pending(uid, 1);
    assert(pthread_create(&b, NULL, wait_thread, &second) == 0);
    wait_pending(uid, 2);
    assert(sceKernelSetEventFlag(uid, 1) == 0);
    assert(pthread_join(a, NULL) == 0);
    assert(first.result == 0 && first.observed == 1);
    wait_pending(uid, 1);
    unsigned int canceled;
    assert(sceKernelCancelEventFlag(uid, 2, &canceled) == 0 && canceled == 1);
    assert(pthread_join(b, NULL) == 0);
    assert((uint32_t)second.result == SCE_KERNEL_ERROR_WAIT_CANCEL);
    assert(sceKernelDeleteEventFlag(uid) == 0);
    assert((uint32_t)sceKernelSetEventFlag(uid, 1) == SCE_KERNEL_ERROR_UNKNOWN_EVF_ID);

    uid = sceKernelCreateEventFlag("single", 0, 0, NULL);
    first = (Wait){.uid = uid, .bits = 1};
    assert(pthread_create(&a, NULL, wait_thread, &first) == 0);
    wait_pending(uid, 1);
    timeout = 0;
    assert((uint32_t)sceKernelWaitEventFlag(uid, 1, 0, NULL, &timeout) == SCE_KERNEL_ERROR_EVF_MULTI);
    assert(sceKernelDeleteEventFlag(uid) == 0);
    assert(pthread_join(a, NULL) == 0);
    assert((uint32_t)first.result == SCE_KERNEL_ERROR_WAIT_DELETE);
    assert(events == NULL);
    assert((uint32_t)sceKernelCreateEventFlag("priority", SCE_KERNEL_ATTR_THREAD_PRIO, 0, NULL) ==
           SCE_KERNEL_ERROR_ILLEGAL_ATTR);
    puts("Event-flag threaded regression checks passed.");
    return 0;
}
