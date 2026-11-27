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
    puts("Kernel monotonic time regression checks passed.");
    return 0;
}
