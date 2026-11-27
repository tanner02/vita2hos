#include "guest_tp.h"
#include <stdlib.h>
#include <string.h>

static int retarget(uint32_t word, uint32_t mode, uint32_t *patched)
{
    unsigned reg;
    if (mode == GUEST_TP_ARM) {
        if ((word & UINT32_C(0x0fff0fff)) != UINT32_C(0x0e1d0f70) || word >> 28 == 15)
            return -1;
        reg = (word >> 12) & 15;
        *patched = word ^ UINT32_C(0x20);
    } else if (mode == GUEST_TP_THUMB) {
        if ((word & UINT32_C(0x0fffffff)) != UINT32_C(0x0f70ee1d)) return -1;
        reg = word >> 28;
        *patched = word ^ UINT32_C(0x00200000);
    } else return -1;
    /* SP/PC destinations are outside the supported thread-pointer read ABI. */
    return reg == 13 || reg == 15 ? -1 : 0;
}

int guest_tp_patch(const segment_info_t *segments, int count,
                    const GuestTpSite *sites, size_t site_count)
{
    if (!segments || count < 0 || (site_count && !sites) || site_count > GUEST_TP_MAX_SITES)
        return -1;
    if (!site_count) return 0;
    GuestTpSite *copy = malloc(site_count * sizeof(*copy));
    if (!copy) return -1;
    memcpy(copy, sites, site_count * sizeof(*copy));
    for (size_t i=0; i<site_count; ++i) {
        const GuestTpSite *site = &copy[i];
        if (site->segment >= (unsigned)count) goto fail;
        const segment_info_t *segment = &segments[site->segment];
        if (segment->p_type != PT_LOAD || !(segment->p_flags & PF_X) || !segment->rw_addr ||
            site->offset > segment->p_filesz || 4 > segment->p_filesz - site->offset ||
            site->offset > segment->p_memsz || 4 > segment->p_memsz - site->offset ||
            segment->rw_addr > UINTPTR_MAX - site->offset ||
            segment->rw_addr + site->offset > UINTPTR_MAX - 4 ||
            segment->rx_addr > UINT32_MAX - (uint64_t)site->offset ||
            segment->rx_addr + site->offset > UINT32_MAX - 3) goto fail;
        unsigned alignment = site->mode == GUEST_TP_THUMB ? 2 : 4;
        if ((segment->rx_addr + site->offset) % alignment) goto fail;
        uint32_t word, patched;
        memcpy(&word, (const void *)(segment->rw_addr + site->offset), 4);
        if (word != site->expected || retarget(word, site->mode, &patched) < 0) goto fail;
        uintptr_t address = segment->rw_addr + site->offset;
        for (size_t j=0; j<i; ++j) {
            uintptr_t previous = segments[copy[j].segment].rw_addr + copy[j].offset;
            if (address < previous + 4 && previous < address + 4) goto fail;
        }
        copy[i].expected = patched;
    }
    for (size_t i=0; i<site_count; ++i) {
        memcpy((void *)(segments[copy[i].segment].rw_addr + copy[i].offset), &copy[i].expected, 4);
    }
    free(copy);
    return 0;
fail:
    free(copy);
    return -1;
}
