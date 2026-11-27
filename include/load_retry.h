#ifndef LOAD_RETRY_H
#define LOAD_RETRY_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LOAD_LAYOUT_RETRY_LIMIT 64
#define LOAD_RETRY_PROGRAM_ID UINT64_C(0x0100f27432680000)
#define LOAD_RETRY_MAGIC UINT64_C(0x5654483252545259)
typedef struct {
    uint64_t magic, source_tag;
    uint32_t version, attempts;
} LoadRetryPacket;
uint64_t load_source_tag(const void *data, size_t size);
unsigned load_retry_count(const LoadRetryPacket *previous, uint64_t source_tag);
bool load_retry_next(const LoadRetryPacket *previous, uint64_t source_tag,
                      uint64_t program_id, LoadRetryPacket *next);
#endif
