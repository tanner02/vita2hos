#define TEST_THREADED_MUTEX
#define TEST_KERNEL_TIME
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <string.h>
static struct timeval native_time;
static int native_result;
static unsigned wall_reads;
static int fake_gettimeofday(struct timeval *value,void *zone)
{
    assert(!zone);++wall_reads;
    if (native_result<0) return native_result;
    *value=native_time;return 0;
}
#define gettimeofday fake_gettimeofday

static uint64_t clock_ticks;
static unsigned int tick_reads;
static uint64_t armGetSystemTick(void)
{
    ++tick_reads;
    return clock_ticks;
}
#include "../../source/modules/SceKernelTime.c"
#undef gettimeofday
#include "../../source/module.c"
DECLARE_LIBRARY(SceThreadmgr,0x859a24b1);
DECLARE_LIBRARY(SceLibKernel,0xcae9ace6);
DECLARE_LIBRARY(SceProcessmgr,0x2DD91812);
void log_printf(const char *format,...) { (void)format; }

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
    SceKernelClock (*libc_clock)(void)=(void *)module_get_func_export(0x2DD91812,0x9E45DA09);
    SceKernelTime (*libc_time)(SceKernelTime *)=(void *)module_get_func_export(0x2DD91812,0x0039BE45);
    int (*libc_gettimeofday)(SceKernelTimeval *,SceKernelTimezone *)=
        (void *)module_get_func_export(0x2DD91812,0x4B879059);
    assert(libc_clock && libc_time && libc_gettimeofday);
    clock_ticks=process_start_ticks+UINT64_C(19200000)*10000;
    assert(libc_clock()==UINT64_C(10000000000)); /* Must retain high 32 bits. */
    const int64_t epochs[]={0,1795737600,INT32_MAX,(int64_t)INT32_MAX+1,
                            UINT32_MAX,(int64_t)UINT32_MAX+1,-1};
    for (unsigned i=0;i<sizeof(epochs)/sizeof(epochs[0]);++i) {
        native_time=(struct timeval){.tv_sec=epochs[i],.tv_usec=999999};
        SceKernelTime scalar=123;
        assert(libc_time(&scalar)==(uint32_t)epochs[i] && scalar==(uint32_t)epochs[i]);
        assert(libc_time(NULL)==scalar);
        struct { uint32_t before;SceKernelTimeval value;uint32_t after; } guarded=
            {.before=0x12345678,.after=0xabcdef01};
        SceKernelTimezone zone={.value=UINT64_MAX};
        assert(!libc_gettimeofday(&guarded.value,&zone));
        uint32_t bits;memcpy(&bits,&guarded.value.sec,sizeof(bits));
        assert(bits==scalar && guarded.value.usec==999999 && !zone.value);
        assert(guarded.before==0x12345678 && guarded.after==0xabcdef01);
    }
    unsigned before_wall=wall_reads;
    SceKernelTimezone zone={.value=UINT64_MAX};
    assert(!libc_gettimeofday(NULL,&zone) && !zone.value && wall_reads==before_wall);
    assert(!libc_gettimeofday(NULL,NULL) && wall_reads==before_wall);
    native_result=-1;
    SceKernelTime scalar=123;
    assert(libc_time(&scalar)==UINT32_MAX && scalar==123);
    SceKernelTimeval unchanged={.sec=123,.usec=456},value=unchanged;
    zone.value=UINT64_MAX;
    assert(libc_gettimeofday(&value,&zone)==-1);
    assert(!memcmp(&value,&unchanged,sizeof(value)) && zone.value==UINT64_MAX);
    native_result=0;native_time=(struct timeval){.tv_sec=10,.tv_usec=0};
    assert(!libc_gettimeofday(&value,NULL) && value.sec==10 && !value.usec);
    SceKernelTime_start_process();assert(!libc_clock());
    puts("Kernel time: monotonic conversion, full-width libc clock, UTC epoch ABI, rollover and failure preservation passed.");
    return 0;
}
