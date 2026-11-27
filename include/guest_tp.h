#ifndef GUEST_TP_H
#define GUEST_TP_H
#include "load_segment.h"
#include <stddef.h>
#define GUEST_TP_MAX_SITES 1024
enum { GUEST_TP_ARM, GUEST_TP_THUMB };
typedef struct {
    uint32_t segment, offset, expected, mode;
} GuestTpSite;
/* Select sites using independently established instruction boundaries. This
 * API never scans code: matching bytes alone do not prove an instruction site.
 * expected is the little-endian 32-bit word, including both Thumb halfwords.
 * Call only before guest execution; finalize code caches after patching.
 * All sites validate before any write. No thread registers are changed here. */
int guest_tp_patch(const segment_info_t *segments, int count,
                    const GuestTpSite *sites, size_t site_count);
#endif
