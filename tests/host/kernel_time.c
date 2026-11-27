#define TEST_THREADED_MUTEX
#define TEST_KERNEL_TIME
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static uint64_t clock_ticks;
static unsigned int tick_reads;
static uint64_t armGetSystemTick(void)
{
    ++tick_reads;
    return clock_ticks;
}
#include "../../source/modules/SceKernelTime.c"

int main(void)
{
    clock_ticks = 0;
    assert(sceKernelGetSystemTimeWide() == 0);
    clock_ticks = 19200000;
    assert(sceKernelGetSystemTimeWide() == 1000000);
    clock_ticks = 19;
    assert(sceKernelGetSystemTimeWide() == 0);
    clock_ticks = 20;
    assert(sceKernelGetSystemTimeWide() == 1);
    clock_ticks = (uint64_t)19200000 * 10000;
    const SceInt64 wide = sceKernelGetSystemTimeWide();
    assert(wide == INT64_C(10000000000) && wide > UINT32_MAX);
    clock_ticks += 19200;
    assert(sceKernelGetSystemTimeWide() == wide + 1000);
    assert(tick_reads == 6);
    clock_ticks = UINT64_MAX;
    assert(sceKernelGetSystemTimeWide() ==
           (SceInt64)(((__uint128_t)UINT64_MAX * 5) / 96));
    uint64_t random = 1;
    for (int i = 0; i < 10000; ++i) {
        random = random * UINT64_C(6364136223846793005) + 1;
        clock_ticks = random;
        assert(sceKernelGetSystemTimeWide() == (SceInt64)(((__uint128_t)random * 5) / 96));
    }
    clock_ticks = UINT64_C(19200000) * 30000;
    SceKernelTime_start_process();
    assert(sceKernelGetProcessTimeWide() == 0);
    clock_ticks += 19200000;
    assert(sceKernelGetProcessTimeWide() == 1000000);
    assert(sceKernelGetProcessTimeLow() == 1000000);
    SceKernelSysClock process_time = UINT64_MAX;
    assert(sceKernelGetProcessTime(&process_time) == 0 && process_time == 1000000);
    const unsigned int reads_before_null = tick_reads;
    assert(sceKernelGetProcessTime(NULL) == 0 && tick_reads == reads_before_null);
    /* Crossing 32-bit microseconds wraps only the Low API. */
    clock_ticks = process_start_ticks + UINT64_C(19200000) * 10000;
    assert(sceKernelGetProcessTimeWide() == UINT64_C(10000000000));
    assert(sceKernelGetProcessTimeLow() == (uint32_t)UINT64_C(10000000000));
    assert(sceKernelGetProcessTime(&process_time) == 0 && process_time == UINT64_C(10000000000));
    /* Restarting an application establishes a fresh epoch. */
    SceKernelTime_start_process();
    assert(sceKernelGetProcessTimeWide() == 0);
    clock_ticks += 20;
    assert(sceKernelGetProcessTimeWide() == 1);
    puts("Kernel monotonic time regression checks passed.");
    return 0;
}
