#include "load_segment.h"

int load_fixed_layout(const segment_info_t *segments, int count)
{
    if (!segments || count <= 0 || count > 64)
        return -1;
    int executable = 0;
    for (int i = 0; i < count; ++i) {
        const segment_info_t *s = &segments[i];
        if (s->p_type != PT_LOAD || !s->p_memsz)
            continue;
        uint64_t size = ((uint64_t)s->p_memsz + 4095) & ~UINT64_C(4095);
        uint64_t end = (uint64_t)s->p_vaddr + size;
        if (!s->p_vaddr || s->p_vaddr % 4096 || end > UINT32_MAX || s->p_filesz > s->p_memsz ||
            (s->p_align && ((s->p_align & (s->p_align - 1)) || s->p_vaddr % s->p_align)) ||
            (s->p_flags & (PF_W | PF_X)) == (PF_W | PF_X))
            return -1;
        if (s->p_flags & PF_X)
            executable = 1;
        for (int j = 0; j < i; ++j) {
            const segment_info_t *p = &segments[j];
            if (p->p_type != PT_LOAD || !p->p_memsz)
                continue;
            uint64_t previous_end =
                (uint64_t)p->p_vaddr + (((uint64_t)p->p_memsz + 4095) & ~UINT64_C(4095));
            if (s->p_vaddr < previous_end && p->p_vaddr < end)
                return -1;
        }
    }
    return executable ? 0 : -1;
}
