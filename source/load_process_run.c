#include "load_process.h"
#include "log.h"
#include <psp2/kernel/error.h>
#include <psp2common/kernel/modulemgr.h>

typedef int (*GuestEntry)(uint32_t args, void *argp);

int load_process_run(LoadProcess *process, uint32_t args, void *argp)
{
    if (!process || !process->bound || !process->images || !process->count ||
        !process->entry || process->run_started || (args && !argp))
        return (int)SCE_KERNEL_ERROR_MODULEMGR_START_FAILED;
    for (size_t i = 0; i < process->count; ++i)
        if (!process->images[i].bound)
            return (int)SCE_KERNEL_ERROR_MODULEMGR_START_FAILED;
    process->run_started = true;
    for (size_t i = 1; i < process->count; ++i) {
        GuestEntry start = (GuestEntry)process->images[i].lifecycle.start;
        if (!start) continue;
        LOG("Starting guest dependency %zu", i);
        int result = start(0, NULL);
        if (result != SCE_KERNEL_START_SUCCESS) {
            /* NO_RESIDENT cannot be accepted while imports still point into
             * the module. Dynamic unloading/rebinding is not implemented. */
            LOG("Guest dependency %zu startup returned 0x%x; main will not run", i, result);
            return (int)SCE_KERNEL_ERROR_MODULEMGR_START_FAILED;
        }
        ++process->modules_started;
    }
    return ((GuestEntry)process->entry)(args, argp);
}
