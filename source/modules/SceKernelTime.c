#include <switch.h>
#include <psp2/kernel/threadmgr/thread.h>
#include <psp2/kernel/processmgr.h>

#include "module.h"
#include "modules/SceKernelTime.h"

static uint64_t process_start_ticks;

static uint64_t ticks_to_us(uint64_t ticks)
{
    /* Horizon's timer runs at 19.2 MHz: microseconds = ticks * 5 / 96.
     * Divide first to avoid overflow in libnx's ticks * 625 ns conversion. */
    return (ticks / 96) * 5 + ((ticks % 96) * 5) / 96;
}

void SceKernelTime_start_process(void)
{
    process_start_ticks = armGetSystemTick();
}

/* Vita system time is monotonic microseconds, unrelated to calendar time.
 * Horizon supplies the same kind of counter through its architectural timer. */
EXPORT(SceThreadmgr, 0xF4EE4FA9, SceInt64, sceKernelGetSystemTimeWide)
{
    return (SceInt64)ticks_to_us(armGetSystemTick());
}

EXPORT(SceLibKernel, 0xB110C123, SceUInt64, sceKernelGetProcessTimeWide)
{
    return ticks_to_us(armGetSystemTick() - process_start_ticks);
}

EXPORT(SceLibKernel, 0xE9F973B1, SceUInt32, sceKernelGetProcessTimeLow)
{
    return (SceUInt32)sceKernelGetProcessTimeWide();
}

EXPORT(SceLibKernel, 0x4C4672BF, int, sceKernelGetProcessTime, SceKernelSysClock *clock)
{
    /* The pinned reference permits a null output as a successful no-op. */
    if (clock) *clock = sceKernelGetProcessTimeWide();
    return 0;
}
