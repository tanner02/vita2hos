#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include <psp2/kernel/error.h>
#include <psp2/kernel/threadmgr/eventflag.h>

#include "module.h"
#include "modules/SceSysmem.h"
#include "modules/SceKernelThreadMgr.h"

typedef struct EventWaiter {
    struct EventWaiter *next;
    CondVar condition;
    uint32_t bits, mode, observed;
    int result, priority;
    bool done;
} EventWaiter;

typedef struct EventFlag {
    struct EventFlag *next;
    SceUID uid;
    char name[32];
    uint32_t attr, initial, pattern;
    unsigned int references;
    EventWaiter *waiters;
} EventFlag;

/* A single lock covers lookup, bit consumption and waiter lifetime. Waiters
 * retain the event until they have observed deletion/cancellation. */
static Mutex event_lock;
static EventFlag *events;

static EventFlag *find_event(SceUID uid)
{
    for (EventFlag *event = events; event; event = event->next)
        if (event->uid == uid)
            return event;
    return NULL;
}

static int validate_wait(uint32_t bits, uint32_t mode)
{
    if (!bits)
        return SCE_KERNEL_ERROR_ILLEGAL_PATTERN;
    if ((mode & ~7U) || ((mode & 6U) == 6U))
        return SCE_KERNEL_ERROR_ILLEGAL_MODE;
    return 0;
}

static bool consume(EventFlag *event, EventWaiter *waiter)
{
    bool matched = waiter->mode & SCE_EVENT_WAITOR
                       ? (event->pattern & waiter->bits) != 0
                       : (event->pattern & waiter->bits) == waiter->bits;
    if (!matched)
        return false;
    waiter->observed = event->pattern;
    if (waiter->mode & SCE_EVENT_WAITCLEAR)
        event->pattern = 0;
    else if (waiter->mode & SCE_EVENT_WAITCLEAR_PAT)
        event->pattern &= ~waiter->bits;
    waiter->result = 0;
    waiter->done = true;
    return true;
}

static unsigned int pending_count(EventFlag *event)
{
    unsigned int count = 0;
    for (EventWaiter *w = event->waiters; w; w = w->next)
        count += !w->done;
    return count;
}

EXPORT(SceLibKernel, 0x8516D040, SceUID, sceKernelCreateEventFlag, const char *name,
       int attr, int bits, SceKernelEventFlagOptParam *opt)
{
    if (!name)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    if ((uint32_t)attr & ~((uint32_t)(SCE_EVENT_WAITMULTIPLE | SCE_KERNEL_ATTR_THREAD_PRIO)))
        return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
    if (opt && opt->size != sizeof(*opt))
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
    EventFlag *event = calloc(1, sizeof(*event));
    if (!event)
        return SCE_KERNEL_ERROR_NO_MEMORY;
    event->uid = SceSysmem_get_next_uid();
    strncpy(event->name, name, sizeof(event->name) - 1);
    event->attr = attr;
    event->initial = event->pattern = (uint32_t)bits;
    event->references = 1;
    const SceUID uid = event->uid;
    mutexLock(&event_lock);
    event->next = events;
    events = event;
    mutexUnlock(&event_lock);
    return uid;
}

EXPORT(SceThreadmgr, 0xEC94DFF7, int, sceKernelSetEventFlag, SceUID uid, unsigned int bits)
{
    mutexLock(&event_lock);
    EventFlag *event = find_event(uid);
    if (event) {
        event->pattern |= bits;
        for (EventWaiter *w = event->waiters; w; w = w->next)
            if (!w->done && consume(event, w))
                condvarWakeAll(&w->condition);
    }
    const int result = event ? 0 : SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;
    mutexUnlock(&event_lock);
    return result;
}

EXPORT(SceThreadmgr, 0x4CB87CA7, int, sceKernelClearEventFlag, SceUID uid, unsigned int bits)
{
    mutexLock(&event_lock);
    EventFlag *event = find_event(uid);
    if (event)
        event->pattern &= bits; /* Vita's argument is a keep mask. */
    const int result = event ? 0 : SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;
    mutexUnlock(&event_lock);
    return result;
}

EXPORT(SceLibKernel, 0x1FBB0FE1, int, sceKernelPollEventFlag, int uid, unsigned int bits,
       unsigned int mode, unsigned int *out_bits)
{
    int result = validate_wait(bits, mode);
    if (result)
        return result;
    EventWaiter waiter = {.bits = bits, .mode = mode};
    mutexLock(&event_lock);
    EventFlag *event = find_event(uid);
    result = !event ? SCE_KERNEL_ERROR_UNKNOWN_EVF_ID
                    : consume(event, &waiter) ? 0 : SCE_KERNEL_ERROR_EVF_COND;
    if (event && out_bits)
        *out_bits = event->pattern;
    if (!result && out_bits)
        *out_bits = waiter.observed;
    mutexUnlock(&event_lock);
    return result;
}

EXPORT(SceLibKernel, 0x83C0E2AF, int, sceKernelWaitEventFlag, int uid, unsigned int bits,
       unsigned int mode, unsigned int *out_bits, SceUInt *timeout)
{
    int result = validate_wait(bits, mode);
    if (result)
        return result;
    EventWaiter waiter = {.bits = bits, .mode = mode};
    condvarInit(&waiter.condition);
    uint64_t start = armGetSystemTick();
    uint64_t duration = timeout ? (uint64_t)*timeout * 1000 : UINT64_MAX;
    mutexLock(&event_lock);
    EventFlag *event = find_event(uid);
    if (!event) {
        mutexUnlock(&event_lock);
        return SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;
    }
    if (!(event->attr & SCE_EVENT_WAITMULTIPLE) && pending_count(event)) {
        mutexUnlock(&event_lock);
        return SCE_KERNEL_ERROR_EVF_MULTI;
    }
    consume(event, &waiter);
    if (!waiter.done && (event->attr & SCE_KERNEL_ATTR_THREAD_PRIO)) {
        VitaThreadInfo *thread = SceKernelThreadMgr_get_thread_info();
        if (!thread) {
            mutexUnlock(&event_lock);
            return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
        }
        waiter.priority = thread->priority;
    }
    event->references++;
    EventWaiter **tail = &event->waiters;
    while (*tail && ((*tail)->done || !(event->attr & SCE_KERNEL_ATTR_THREAD_PRIO) ||
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
        Result res = condvarWaitTimeout(&waiter.condition, &event_lock,
                                         timeout ? duration - elapsed : UINT64_MAX);
        if (R_FAILED(res) && res != 0xEA01 && !waiter.done) {
            waiter.result = SCE_KERNEL_ERROR_EVF_ERROR;
            break;
        }
    }
    if (out_bits)
        *out_bits = waiter.done && waiter.result == 0 ? waiter.observed : event->pattern;
    if (timeout) {
        uint64_t elapsed = armTicksToNs(armGetSystemTick() - start);
        *timeout = elapsed >= duration ? 0 : (SceUInt)((duration - elapsed) / 1000);
    }
    EventWaiter **link = &event->waiters;
    while (*link != &waiter)
        link = &(*link)->next;
    *link = waiter.next;
    result = waiter.result;
    if (--event->references == 0)
        free(event);
    mutexUnlock(&event_lock);
    return result;
}

EXPORT(SceLibKernel, 0x2A12D9B7, int, sceKernelCancelEventFlag, SceUID uid,
       unsigned int pattern, unsigned int *num_waiters)
{
    mutexLock(&event_lock);
    EventFlag *event = find_event(uid);
    if (event) {
        if (num_waiters)
            *num_waiters = pending_count(event);
        event->pattern = pattern;
        for (EventWaiter *w = event->waiters; w; w = w->next) {
            if (w->done)
                continue;
            w->result = SCE_KERNEL_ERROR_WAIT_CANCEL;
            w->done = true;
            condvarWakeAll(&w->condition);
        }
    }
    const int result = event ? 0 : SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;
    mutexUnlock(&event_lock);
    return result;
}

EXPORT(SceThreadmgr, 0x5840162C, int, sceKernelDeleteEventFlag, int uid)
{
    mutexLock(&event_lock);
    EventFlag **link = &events;
    while (*link && (*link)->uid != uid)
        link = &(*link)->next;
    EventFlag *event = *link;
    const bool found = event != NULL;
    if (event) {
        *link = event->next;
        for (EventWaiter *w = event->waiters; w; w = w->next) {
            if (w->done)
                continue;
            w->result = SCE_KERNEL_ERROR_WAIT_DELETE;
            w->done = true;
            condvarWakeAll(&w->condition);
        }
        if (--event->references == 0)
            free(event);
    }
    mutexUnlock(&event_lock);
    return found ? 0 : SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;
}

EXPORT(SceLibKernel, 0x8BA4C0C1, int, sceKernelGetEventFlagInfo, SceUID uid,
       SceKernelEventFlagInfo *info)
{
    if (!info)
        return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    if (info->size != sizeof(*info))
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
    mutexLock(&event_lock);
    EventFlag *event = find_event(uid);
    if (event) {
        memset(info, 0, sizeof(*info));
        info->size = sizeof(*info);
        info->evfId = uid;
        memcpy(info->name, event->name, sizeof(info->name));
        info->attr = event->attr;
        info->initPattern = event->initial;
        info->currentPattern = event->pattern;
        info->numWaitThreads = pending_count(event);
    }
    const int result = event ? 0 : SCE_KERNEL_ERROR_UNKNOWN_EVF_ID;
    mutexUnlock(&event_lock);
    return result;
}
