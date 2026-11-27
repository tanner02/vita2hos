#ifndef LOAD_SEGMENT_H
#define LOAD_SEGMENT_H
#include <stdbool.h>
#include <stdint.h>
#include <elf.h>

typedef struct {
    void *src_data;
    uintptr_t rw_addr;
    uintptr_t rx_addr; /* Only used for executable segments */
    Elf32_Word p_type;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
    Elf32_Addr p_vaddr;
    bool needs_free;
} segment_info_t;

int load_relocate(const void *reloc, uint32_t size, const segment_info_t *segments, int count);
/* Imported-variable formats 1/2 use an explicit symbol, not a symbol segment. */
int load_variable_relocate(const void *reloc, uint32_t size, const segment_info_t *segments,
                            int count, uint32_t symbol);
#endif
