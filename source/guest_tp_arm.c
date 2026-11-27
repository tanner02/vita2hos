#include "guest_tp_plan.h"
#if !defined(__arm__)
#error This register implementation requires ARM32.
#endif
uintptr_t guest_tp_exchange(uintptr_t value)
{
    uintptr_t previous;
    __asm__ volatile("mrc p15, 0, %0, c13, c0, 2\n\tmcr p15, 0, %1, c13, c0, 2"
                      : "=&r"(previous) : "r"(value) : "memory");
    return previous;
}
