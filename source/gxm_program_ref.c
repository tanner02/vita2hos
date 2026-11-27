#include <limits.h>
#include <psp2/gxm.h>
#include "gxm/program_ref.h"

int gxm_program_ref_add(GxmProgramRef *ref)
{
    if (!ref) return SCE_GXM_ERROR_INVALID_POINTER;
    unsigned count = atomic_load_explicit(&ref->count, memory_order_relaxed);
    do {
        if (!count || count == UINT_MAX) return SCE_GXM_ERROR_INVALID_VALUE;
    } while (!atomic_compare_exchange_weak_explicit(&ref->count, &count, count + 1,
                                                   memory_order_relaxed, memory_order_relaxed));
    return 0;
}

int gxm_program_ref_release(GxmProgramRef *ref, bool *destroy)
{
    if (!destroy) return SCE_GXM_ERROR_INVALID_POINTER;
    *destroy = false;
    if (!ref) return SCE_GXM_ERROR_INVALID_POINTER;
    unsigned count = atomic_load_explicit(&ref->count, memory_order_relaxed);
    do {
        if (!count) return SCE_GXM_ERROR_INVALID_VALUE;
    } while (!atomic_compare_exchange_weak_explicit(&ref->count, &count, count - 1,
                                                   memory_order_release, memory_order_relaxed));
    if (count == 1) {
        atomic_thread_fence(memory_order_acquire);
        *destroy = true;
    }
    return 0;
}

int gxm_program_ref_get(const GxmProgramRef *ref, unsigned int *count)
{
    if (!ref || !count) return SCE_GXM_ERROR_INVALID_POINTER;
    *count = atomic_load_explicit(&ref->count, memory_order_relaxed);
    return 0;
}
