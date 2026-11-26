#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../source/load_format.c"

int main(void)
{
    unsigned char data[sizeof(SCE_header)] = {0};
    assert(load_detect_format(NULL, 100) == LOAD_FORMAT_INVALID);
    for (size_t size = 0; size < sizeof(data); size++) {
        unsigned char *short_data = malloc(size ? size : 1);
        memset(short_data, 0, size);
        memcpy(data, "\177ELF", 4);
        memcpy(short_data, data, size < 4 ? size : 4);
        assert(load_detect_format(short_data, size) ==
               (size >= sizeof(Elf32_Ehdr) ? LOAD_FORMAT_ELF : LOAD_FORMAT_INVALID));
        memcpy(data, "SCE\0", 4);
        memcpy(short_data, data, size < 4 ? size : 4);
        assert(load_detect_format(short_data, size) == LOAD_FORMAT_INVALID);
        free(short_data);
    }
    memcpy(data, "SCE\0", 4);
    assert(load_detect_format(data, sizeof(data)) == LOAD_FORMAT_SELF);
    memcpy(data, "\177BAD", 4);
    assert(load_detect_format(data, sizeof(data)) == LOAD_FORMAT_INVALID);
    memcpy(data, "SBAD", 4);
    assert(load_detect_format(data, sizeof(data)) == LOAD_FORMAT_INVALID);
    assert(load_file_range(100, 100, 0));
    assert(load_file_range(100, 99, 1));
    assert(!load_file_range(100, 99, 2));
    assert(!load_file_range(100, UINT64_MAX, 1));
    assert(!load_file_range(100, 1, UINT64_MAX));
    puts("Executable detection and range checks passed.");
    return 0;
}
