#include <psp2/kernel/threadmgr.h>
#include <stdint.h>
volatile uint32_t dependency_state[4];
uint32_t dependency_value(uint32_t value)
{
    return dependency_state[0]^dependency_state[3]^value;
}
int module_start(SceSize args,void *argp)
{
    if (args || argp) return -1;
    dependency_state[0]=0x51a7c0de;
    dependency_state[1]=(uintptr_t)sceKernelGetTLSAddr(0);
    ++dependency_state[2];
    if (!dependency_state[1]) return -2;
    /* Matches the real bundled-libc startup's successful nonreturning path. */
    sceKernelExitThread(0);
    return -3;
}
