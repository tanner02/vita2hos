#include <switch.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/error.h>
#include "module.h"

EXPORT(SceProcessmgr,0x2252890C,int,sceKernelPowerTick,SceKernelPowerTickType type)
{
    switch(type) {
    case SCE_KERNEL_POWER_TICK_DEFAULT:
        /* A one-shot activity report resets Horizon idle detection. Do not
         * turn a Vita tick into a persistent disable-auto-sleep setting. */
        return R_FAILED(appletReportUserIsActive()) ? (int)SCE_KERNEL_ERROR_ERROR : 0;
    case SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND:
    case SCE_KERNEL_POWER_TICK_DISABLE_OLED_OFF:
    case SCE_KERNEL_POWER_TICK_DISABLE_OLED_DIMMING:
        /* The activity report resets all timers, not an individual timer. */
        return SCE_KERNEL_ERROR_UNSUP;
    default:
        return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    }
}
