#ifndef VITA2HOS_SCE_MP4_H
#define VITA2HOS_SCE_MP4_H
#include <stdint.h>
/* The reviewed SDK caller consumes only R0 as a handle and treats zero as
 * failure. Argument roles remain unknown; the unavailable backend must not
 * dereference these opaque register words or fabricate a successful handle. */
uint32_t sceMp4OpenFile(uint32_t argument0,uint32_t argument1,uint32_t argument2,uint32_t argument3);
#endif
