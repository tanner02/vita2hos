#ifndef SCE_KERNEL_THREAD_MGR_H
#define SCE_KERNEL_THREAD_MGR_H

#include <psp2/kernel/threadmgr.h>
#include <stdatomic.h>
#include "guest_tls.h"
#include "load_process_params.h"
#include "thread_events.h"

typedef struct {
    uint32_t index;
    Thread thread;
    SceUID uid;
    char name[32];
    SceKernelThreadEntry entry;
    SceSize arglen;
    void *argp;
    int return_status;
    int priority; /* Resolved Vita priority, before mapping to Horizon. */
    uint32_t *vita_tls;
    void *vita_user_tls;
    atomic_bool started;
    bool exit_notified;
    VitaThreadEventFrame *event_frames;
} VitaThreadInfo;

/* The immutable template is borrowed until all guest threads have stopped. */
int SceKernelThreadMgr_init(const GuestTlsTemplate *main_tls, bool retarget_thread_pointer);
int SceKernelThreadMgr_finish(void);
/* Called on the current guest thread immediately before its nonreturning exit. */
void SceKernelThreadMgr_prepare_current_exit(int status);
VitaThreadInfo *SceKernelThreadMgr_get_thread_info(void);
int SceKernelThreadMgr_main_entry(SceKernelThreadEntry entry, int args, void *argp);
int SceKernelThreadMgr_main_entry_configured(SceKernelThreadEntry entry, int args, void *argp,
                                             const LoadProcessParameters *parameters);

#endif
