#include "load_retry.h"

uint64_t load_source_tag(const void *data, size_t size)
{
    const unsigned char *bytes = data;
    if (!data && size) return 0;
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
    return hash;
}

unsigned load_retry_count(const LoadRetryPacket *previous, uint64_t source_tag)
{
    if (!previous || previous->magic != LOAD_RETRY_MAGIC || previous->version != 1 ||
        previous->source_tag != source_tag) return 0;
    return previous->attempts;
}

bool load_retry_next(const LoadRetryPacket *previous, uint64_t source_tag,
                      uint64_t program_id, LoadRetryPacket *next)
{
    /* Never restart a commercial host title used by an hbloader override. */
    if (!next || !source_tag || program_id != LOAD_RETRY_PROGRAM_ID) return false;
    unsigned attempts = load_retry_count(previous, source_tag);
    if (attempts >= LOAD_LAYOUT_RETRY_LIMIT) return false;
    *next = (LoadRetryPacket){LOAD_RETRY_MAGIC, source_tag, 1, attempts + 1};
    return true;
}
