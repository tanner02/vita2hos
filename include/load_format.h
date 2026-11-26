#ifndef LOAD_FORMAT_H
#define LOAD_FORMAT_H
#include <stddef.h>
#include <stdint.h>
typedef enum { LOAD_FORMAT_INVALID, LOAD_FORMAT_ELF, LOAD_FORMAT_SELF } LoadFormat;
int load_file_range(size_t size, uint64_t offset, uint64_t length);
LoadFormat load_detect_format(const void *data, size_t size);
#endif
