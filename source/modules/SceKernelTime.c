#include <switch.h>
#include <sys/time.h>
#include <string.h>
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

_Static_assert(sizeof(SceKernelClock)==8,"Guest libc clock is 64-bit");
_Static_assert(sizeof(SceKernelTime)==4,"Guest epoch seconds are 32-bit");
_Static_assert(sizeof(SceKernelTimeval)==8,"Guest timeval is two 32-bit fields");
_Static_assert(sizeof(SceKernelTimezone)==8,"Guest timezone occupies eight bytes");

/* The SceProcessmgr descriptor is declared in SceProcessmgr.c. Keep the full
 * process-relative microsecond counter, including its high return register. */
EXPORT(SceProcessmgr,0x9E45DA09,SceKernelClock,sceKernelLibcClock,void)
{
    return sceKernelGetProcessTimeWide();
}

EXPORT(SceProcessmgr,0x0039BE45,SceKernelTime,sceKernelLibcTime,SceKernelTime *output)
{
    struct timeval native;
    if (gettimeofday(&native,NULL)<0) return (SceKernelTime)-1;
    /* Retain the guest ABI's low 32 bits, independently of host time_t width. */
    SceKernelTime seconds=(SceKernelTime)native.tv_sec;
    if (output) *output=seconds;
    return seconds;
}

EXPORT(SceProcessmgr,0x4B879059,int,sceKernelLibcGettimeofday,
       SceKernelTimeval *output,SceKernelTimezone *zone)
{
    SceKernelTimeval value;
    if (output) {
        struct timeval native;
        if (gettimeofday(&native,NULL)<0) return -1;
        /* Copy the low 32 bits explicitly. Host time_t/long may be wider;
         * memcpy preserves the signed field's bit pattern after 2038 without
         * an implementation-defined out-of-range signed conversion. */
        uint32_t seconds=(uint32_t)native.tv_sec;
        memcpy(&value.sec,&seconds,sizeof(seconds));
        value.usec=(SceInt32)native.tv_usec;
        *output=value;
    }
    /* UTC is the current guest timezone policy. Never copy the host's opaque
     * timezone layout or inherit its process-global timezone/DST settings. */
    if (zone) zone->value=0;
    return 0;
}
