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

#endif
