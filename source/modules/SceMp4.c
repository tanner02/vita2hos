#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include "module.h"
#include "media_probe.h"
#include "modules/SceMp4.h"
/* No MP4 demux backend yet. A generic signed error is a non-NULL pointer and
 * the SDK would pass it into later handle APIs. Reject the open with NULL. */
EXPORT(SceMp4,0x05474AF0,uint32_t,sceMp4OpenFile,
    uint32_t argument0,uint32_t argument1,uint32_t argument2,uint32_t argument3)
{
    media_probe_record("MP4_OPEN_UNAVAILABLE result=0x00000000 caller=0x%08" PRIxPTR
        " r0=0x%08" PRIx32 " r1=0x%08" PRIx32 " r2=0x%08" PRIx32 " r3=0x%08" PRIx32,
        (uintptr_t)__builtin_extract_return_addr(__builtin_return_address(0)),
        argument0,argument1,argument2,argument3);
    return 0;
}
DECLARE_LIBRARY(SceMp4,0x8FEACABD);
