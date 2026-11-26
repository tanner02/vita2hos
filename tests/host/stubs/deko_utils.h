#ifndef TEST_DEKO_UTILS_H
#define TEST_DEKO_UTILS_H
#include <deko3d.h>
#include <stdint.h>
DkMemBlock dk_map_memblock(DkDevice device, void *addr, uint32_t size, uint32_t flags);
#endif
