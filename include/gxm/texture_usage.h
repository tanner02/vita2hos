#ifndef GXM_TEXTURE_USAGE_H
#define GXM_TEXTURE_USAGE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* available is an independently established readable span, not the GXP's own
 * size claim. Failure leaves all units enabled, preserving legacy uploads. */
bool gxm_program_texture_mask(const void *bytes, size_t available, uint16_t *mask);
/* Retain the other stage's slots. A changed mask requires fresh descriptors,
 * including when a shader activates a previously bound texture. */
static inline bool gxm_texture_usage_select(uint32_t *combined, bool vertex, uint16_t mask)
{
    unsigned shift=vertex ? 16 : 0;
    uint32_t next=(*combined & ~(UINT32_C(0xffff)<<shift)) | ((uint32_t)mask<<shift);
    bool changed=next!=*combined;
    *combined=next;
    return changed;
}
static inline bool gxm_texture_usage_active(uint32_t combined, unsigned slot)
{
    return slot<32 && (combined & (UINT32_C(1)<<slot));
}
#ifdef __cplusplus
}
#endif
#endif
