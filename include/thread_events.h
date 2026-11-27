#ifndef THREAD_EVENTS_H
#define THREAD_EVENTS_H
#include <stdint.h>
#include <psp2/types.h>
/* Local ABI names keep the pinned older SDK usable. Values/signature match
 * VitaSDK threadmgr definitions and the pinned Vita3K callback ABI. */
enum { VITA_THREAD_EVENT_START=4,VITA_THREAD_EVENT_EXIT=8,VITA_THREAD_EVENT_PROCESS_ALL=0x10027 };
typedef int (*VitaThreadEventHandler)(SceInt32 type,SceUID thread,SceInt32 arg,void *common);
typedef struct { VitaThreadEventHandler handler;void *common;uint32_t unused[2]; } VitaThreadEventOptions;
#if UINTPTR_MAX==UINT32_MAX
_Static_assert(sizeof(VitaThreadEventOptions)==16,"Thread-event option ABI");
#endif
typedef struct VitaThreadEventFrame VitaThreadEventFrame;
void thread_events_init(void);
/* Stop every guest thread before clearing the process registry. */
void thread_events_finish(void);
SceUID thread_events_register(const char *name,SceUID target,SceInt32 mask,
                              VitaThreadEventHandler handler,void *common);
/* Removing a registration prevents future selection. Already-selected calls
 * may run; the caller keeps handler code/common alive through those calls. */
int thread_events_unregister(SceUID uid);
/* Target-thread context, after installing its TLS. The per-thread frame chain
 * supports an explicit nonreturning exit from inside a callback. */
void thread_events_dispatch(SceInt32 event,SceUID thread,VitaThreadEventFrame **frames);
/* Immediately before nonreturning thread exit; the abandoned callbacks must
 * never return into their old native stack frames. */
void thread_events_abandon(VitaThreadEventFrame **frames);
#endif
