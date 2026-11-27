#ifndef LOAD_PROCESS_PARAMS_H
#define LOAD_PROCESS_PARAMS_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint32_t libc_address, main_stack_size, preload_disabled;
    uint32_t version; /* Zero when absent or legacy. */
    int32_t main_priority;
    bool has_main_priority, has_main_stack_size, has_preload_disabled;
} LoadProcessParameters;
#endif
