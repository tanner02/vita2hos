#ifndef GUEST_TP_PLAN_H
#define GUEST_TP_PLAN_H
#include "guest_tp.h"
/* V2HOSTP1, LE version/count, LE source FNV-1a/size, then 16-byte site records.
 * Fingerprints detect changed input; this format is not an authentication scheme.
 * Positive result: number of patched sites. Negative result: no patches applied.
 * The file helper returns zero only when the optional sidecar is absent. */
int guest_tp_plan_apply(const segment_info_t *segments, int count, uint64_t source_tag,
                         uint64_t source_size, const void *plan, size_t size);
int guest_tp_plan_file(const segment_info_t *segments, int count, uint64_t source_tag,
                        uint64_t source_size, const char *filename);
uintptr_t guest_tp_exchange(uintptr_t value);
#endif
