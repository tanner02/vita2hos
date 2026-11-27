#define TEST_FIXED_MEMORY
#include <assert.h>
#include <stdio.h>
#include "../../source/power_tick.c"
#include "../../source/module.c"
DECLARE_LIBRARY(SceProcessmgr,0x2dd91812);
static unsigned calls;
static Result result;
Result appletReportUserIsActive(void){++calls;return result;}
int main(void)
{
    int (*tick)(SceKernelPowerTickType)=(void *)module_get_func_export(0x2dd91812,0x2252890C);
    assert(tick);
    assert(!tick(SCE_KERNEL_POWER_TICK_DEFAULT) && calls==1);
    assert(!tick(SCE_KERNEL_POWER_TICK_DEFAULT) && calls==2);
    result=0x1234;assert((uint32_t)tick(SCE_KERNEL_POWER_TICK_DEFAULT)==SCE_KERNEL_ERROR_ERROR && calls==3);
    const SceKernelPowerTickType selective[]={SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND,SCE_KERNEL_POWER_TICK_DISABLE_OLED_OFF,SCE_KERNEL_POWER_TICK_DISABLE_OLED_DIMMING};
    for(unsigned i=0;i<3;i++)assert((uint32_t)tick(selective[i])==SCE_KERNEL_ERROR_UNSUP);
    const int invalid[]={-1,2,3,5,7,0x100};
    for(unsigned i=0;i<6;i++)assert((uint32_t)tick((SceKernelPowerTickType)invalid[i])==SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    assert(calls==3);
    puts("Power tick: NID dispatch, one-shot activity reports, native failure and selective-mode rejection passed.");
}
