#include <stdint.h>
#include "module.h"

/* Capture is inactive: no host profiling recorder is connected. The guest ABI
 * imports the pointer variable, then follows it to the 32-bit capture flag.
 * Match the pointer/flag layout in the bundled Vita3K ScePerf implementation. */
static uint32_t perf_capture_flag;
static uint32_t *perf_capture_flag_ptr = &perf_capture_flag;
EXPORT_VAR(0x447f047d, 0x936a5f31, perf_capture_flag_ptr);
