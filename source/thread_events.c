#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include <psp2/kernel/error.h>
#include "thread_events.h"
#include "modules/SceSysmem.h"
#include "log.h"

typedef struct ThreadEvent {
    struct ThreadEvent *next;
    SceUID uid,target;
    uint64_t sequence;
    unsigned int references;
    SceInt32 mask;
    VitaThreadEventHandler handler;
    void *common;
} ThreadEvent;
struct VitaThreadEventFrame { struct VitaThreadEventFrame *previous;ThreadEvent *event; };
static ThreadEvent *events;
static uint64_t event_sequence;
static Mutex event_lock;
static bool event_lock_initialized;

void thread_events_init(void)
{
    if (!event_lock_initialized) { mutexInit(&event_lock);event_lock_initialized=true; }
}

static void release_event(ThreadEvent *event)
{ if (--event->references==0) free(event); }

void thread_events_finish(void)
{
    if (!event_lock_initialized) return;
    mutexLock(&event_lock);
    while (events) {
        ThreadEvent *event=events;events=event->next;release_event(event);
    }
    mutexUnlock(&event_lock);
}

SceUID thread_events_register(const char *name,SceUID target,SceInt32 mask,
                              VitaThreadEventHandler handler,void *common)
{
    if (!name || !handler) return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    if (strnlen(name,32)>=32) return SCE_KERNEL_ERROR_UID_NAME_TOO_LONG;
    if (target<=0) return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    if (!mask || (mask & ~(VITA_THREAD_EVENT_START|VITA_THREAD_EVENT_EXIT)))
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    ThreadEvent *event=calloc(1,sizeof(*event));
    if (!event) return SCE_KERNEL_ERROR_NO_MEMORY;
    event->target=target;event->mask=mask;event->handler=handler;event->common=common;
    event->uid=SceSysmem_get_next_uid();event->references=1;
    SceUID uid=event->uid;
    mutexLock(&event_lock);
    if (event_sequence==UINT64_MAX) {
        mutexUnlock(&event_lock);free(event);return SCE_KERNEL_ERROR_THREAD_EVENT_ERROR;
    }
    event->sequence=++event_sequence;
    ThreadEvent **tail=&events;while (*tail) tail=&(*tail)->next;*tail=event;
    mutexUnlock(&event_lock);
    return uid;
}

int thread_events_unregister(SceUID uid)
{
    mutexLock(&event_lock);
    ThreadEvent **link=&events;
    while (*link && (*link)->uid!=uid) link=&(*link)->next;
    ThreadEvent *event=*link;
    int result=0;
    if (!event) result=SCE_KERNEL_ERROR_UNKNOWN_THREAD_EVENT_ID;
    else { *link=event->next;release_event(event); }
    mutexUnlock(&event_lock);
    return result;
}

void thread_events_dispatch(SceInt32 type,SceUID thread,VitaThreadEventFrame **frames)
{
    if (!frames) return;
    mutexLock(&event_lock);
    uint64_t limit=event_sequence;
    mutexUnlock(&event_lock);
    uint64_t last=0;
    for (;;) {
        mutexLock(&event_lock);
        ThreadEvent *event=events;
        while (event && (event->sequence<=last || event->sequence>limit || !(event->mask & type) ||
               (event->target!=VITA_THREAD_EVENT_PROCESS_ALL && event->target!=thread))) event=event->next;
        if (!event) { mutexUnlock(&event_lock);break; }
        ++event->references;last=event->sequence;
        mutexUnlock(&event_lock);
        VitaThreadEventFrame frame={.previous=*frames,.event=event};*frames=&frame;
        int result=event->handler(type,thread,0,event->common);
        if (result) LOG("Guest thread-event handler 0x%x returned 0x%x",event->uid,result);
        *frames=frame.previous;
        mutexLock(&event_lock);release_event(event);mutexUnlock(&event_lock);
    }
}

void thread_events_abandon(VitaThreadEventFrame **frames)
{
    if (!frames) return;
    mutexLock(&event_lock);
    while (*frames) {
        VitaThreadEventFrame *frame=*frames;*frames=frame->previous;release_event(frame->event);
    }
    mutexUnlock(&event_lock);
}
