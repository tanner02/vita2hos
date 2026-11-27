#include <stdlib.h>
#include <string.h>
#include "load_segment.h"

int load_variable_relocate(const void *reloc, uint32_t size, const segment_info_t *segments,
                            int count, uint32_t symbol)
{
    if ((!reloc && size) || !segments || count < 0) return -1;
    /* A relocation may target its own reference table. Read an immutable copy
     * so earlier writes cannot change how subsequent records are interpreted. */
    unsigned char *copy = size ? malloc(size) : NULL;
    if (size && !copy) return -1;
    if (size) memcpy(copy, reloc, size);
    uint32_t position = 0;
    while (position < size) {
        uint32_t word, offset, addend;
        if (size - position < 8) goto fail;
        memcpy(&word, copy + position, 4);
        unsigned format = word & 15;
        unsigned length = format == 1 ? 8 : format == 2 ? 12 : 0;
        if (!length || size - position < length || (format == 2 && (word >> 16))) goto fail;
        memcpy(&offset, copy + position + 4, 4);
        addend = word >> 16;
        if (format == 2) memcpy(&addend, copy + position + 8, 4);
        unsigned segment = (word >> 4) & 15, code = (word >> 8) & 255;
        /* Reuse the checked ARM encoder with absolute symbol segment 15.
         * Both the ABI addend and the symbol are unsigned 32-bit quantities. */
        uint32_t canonical[3] = {(segment << 16) | (code << 8) | (15 << 4),
                                  symbol + addend, offset};
        if (load_relocate(canonical, sizeof(canonical), segments, count) < 0) goto fail;
        position += length;
    }
    free(copy);
    return 0;
fail:
    free(copy);
    return -1;
}
