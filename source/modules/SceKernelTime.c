#include <switch.h>
#include <psp2/kernel/threadmgr/thread.h>

#include "module.h"

/* Vita system time is monotonic microseconds, unrelated to calendar time.
 * Horizon supplies the same kind of counter through its architectural timer. */
EXPORT(SceThreadmgr, 0xF4EE4FA9, SceInt64, sceKernelGetSystemTimeWide)
{
    const uint64_t ticks = armGetSystemTick();
    /* Horizon's timer runs at 19.2 MHz: microseconds = ticks * 5 / 96.
     * Divide first to avoid overflow in libnx's ticks * 625 ns conversion. */
    return (SceInt64)((ticks / 96) * 5 + ((ticks % 96) * 5) / 96);
}
