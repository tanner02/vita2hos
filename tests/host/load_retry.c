#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../source/load_retry.c"

int main(void)
{
    const char a[] = "first guest", b[] = "second guest";
    uint64_t tag = load_source_tag(a, sizeof(a));
    assert(tag && tag == load_source_tag(a, sizeof(a)));
    assert(tag != load_source_tag(b, sizeof(b)));
    assert(load_source_tag(NULL, 3) == 0);
    LoadRetryPacket packet = {0}, next = {0};
    assert(!load_retry_next(&packet, tag, UINT64_C(0x0100152000022000), &next));
    assert(!load_retry_next(&packet, 0, LOAD_RETRY_PROGRAM_ID, &next));
    assert(load_retry_next(&packet, tag, LOAD_RETRY_PROGRAM_ID, &next));
    assert(next.attempts == 1 && next.source_tag == tag);
    packet = next;
    for (unsigned attempt = 2; attempt <= LOAD_LAYOUT_RETRY_LIMIT; ++attempt) {
        assert(load_retry_next(&packet, tag, LOAD_RETRY_PROGRAM_ID, &next));
        assert(next.attempts == attempt); packet = next;
    }
    LoadRetryPacket before = next;
    assert(!load_retry_next(&packet, tag, LOAD_RETRY_PROGRAM_ID, &next));
    assert(memcmp(&before, &next, sizeof(next)) == 0);
    assert(load_retry_next(&packet, load_source_tag(b, sizeof(b)), LOAD_RETRY_PROGRAM_ID, &next));
    assert(next.attempts == 1);
    packet.magic ^= 1;
    assert(load_retry_next(&packet, tag, LOAD_RETRY_PROGRAM_ID, &next) && next.attempts == 1);
    packet.magic = LOAD_RETRY_MAGIC; packet.version = 999;
    assert(load_retry_next(&packet, tag, LOAD_RETRY_PROGRAM_ID, &next) && next.attempts == 1);
    packet.version = 1; packet.attempts = UINT32_MAX;
    assert(!load_retry_next(&packet, tag, LOAD_RETRY_PROGRAM_ID, &next));
    assert(!load_retry_next(NULL, tag, LOAD_RETRY_PROGRAM_ID, NULL));
    puts("Bounded layout retry, guest identity and host-title protection checks passed.");
}
