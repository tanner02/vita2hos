#include <string.h>
#include "load_format.h"
#include "sce-elf.h"
#include "self.h"

int load_file_range(size_t size, uint64_t offset, uint64_t length)
{
    return offset <= size && length <= size - offset;
}

LoadFormat load_detect_format(const void *data, size_t size)
{
    if (!data || size < 4)
        return LOAD_FORMAT_INVALID;
    if (memcmp(data, "\177ELF", 4) == 0)
        return size >= sizeof(Elf32_Ehdr) ? LOAD_FORMAT_ELF : LOAD_FORMAT_INVALID;
    if (memcmp(data, "SCE\0", 4) == 0)
        return size >= sizeof(SCE_header) ? LOAD_FORMAT_SELF : LOAD_FORMAT_INVALID;
    return LOAD_FORMAT_INVALID;
}
