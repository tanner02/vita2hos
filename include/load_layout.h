#ifndef LOAD_LAYOUT_H
#define LOAD_LAYOUT_H

#include <stdint.h>

/* Compute a segment's offset within an arena whose actual base is known.
 * ELF alignments 0 and 1 impose no constraint. Output is unchanged on error. */
int load_layout_place(uintptr_t base, uint32_t used, uint32_t size, uint32_t alignment,
                       uint32_t capacity, uint32_t *offset, uint32_t *end);

#endif
