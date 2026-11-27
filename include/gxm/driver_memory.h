#ifndef GXM_DRIVER_MEMORY_H
#define GXM_DRIVER_MEMORY_H
#include <stdbool.h>
#include <stdint.h>
#include <psp2/types.h>
typedef struct { SceUID uid; void *base; bool owned; } GxmDriverMemory;
int gxm_driver_memory_size(uint32_t required,uint32_t *size);
/* Caller retains supplied allocations until release; failures preserve output. */
int gxm_driver_memory_acquire(SceUID requested,uint32_t required,GxmDriverMemory *out);
int gxm_driver_memory_release(GxmDriverMemory *memory);
#endif
