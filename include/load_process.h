#ifndef LOAD_PROCESS_H
#define LOAD_PROCESS_H

#include "load.h"

#define LOAD_PROCESS_MAX_MODULES 32
typedef struct {
    LoadImage *images; /* Main image first; dependency order follows the input. */
    size_t count;
    ModuleGuestExports exports;
    uint64_t source_tag;
    void *entry;
    bool bound;
    bool run_started;
    size_t modules_started;
    bool guest_tp_retargeted;
} LoadProcess;

/* Initialize to zero and keep at a stable address. Paths are consumed during
 * loading. All images are prepared/published before any import binding. No
 * lifecycle code is invoked: startup must run on a guest thread after binding.
 * Module order is explicit; this API does not infer initialization dependencies.
 * Stop every guest thread before close. Failed cleanup retains mapping owners
 * for a subsequent close attempt. */
int load_process_load(LoadProcess *process, const char *executable,
                       const char *const *modules, size_t module_count);
Result load_process_close(LoadProcess *process);
struct LoadModulePaths;
/* A present list, including an empty one, overrides bundled preloads.
 * Otherwise modern process parameters select existing libc/FIOS2 in order.
 * No firmware lookup or dynamic module loading is performed. */
int load_process_load_app(LoadProcess *process, const char *executable,
                          const char *app_root, const struct LoadModulePaths *modules);
/* Call once on an initialized guest thread. Startup entries run in explicit
 * dependency-list order with zero arguments. Only resident success (0) is
 * supported. Failure prevents entry into main. This does not run stop callbacks
 * or stop threads created by guest modules. */
int load_process_run(LoadProcess *process, uint32_t args, void *argp);

#endif
