#include <assert.h>
#include <stdio.h>
#include "../../source/gxm_notification.c"

int main(void)
{
    const uintptr_t base = 0x10000;
    SceGxmNotification notification = {0};
    uint32_t offset;
    for (unsigned byte = 0; byte < 2052; ++byte) {
        notification.address = (volatile unsigned int *)(base + byte);
        offset = 0xfeed;
        int ret = gxm_notification_offset(&notification, base, 2048, &offset);
        if (byte < 2048 && byte % 4 == 0) {
            assert(ret == 0 && offset == byte);
        } else {
            assert(ret != 0 && offset == 0xfeed);
        }
    }
    const uintptr_t bad_addresses[] = {0, base - 4, UINTPTR_MAX & ~(uintptr_t)3};
    for (unsigned i = 0; i < sizeof(bad_addresses) / sizeof(bad_addresses[0]); ++i) {
        notification.address = (volatile unsigned int *)bad_addresses[i];
        assert(gxm_notification_offset(&notification, base, 2048, &offset) != 0);
    }
    notification.address = (volatile unsigned int *)base;
    assert(gxm_notification_offset(NULL, base, 2048, &offset) != 0);
    assert(gxm_notification_offset(&notification, 0, 2048, &offset) != 0);
    assert(gxm_notification_offset(&notification, base, 2048, NULL) != 0);
    for (unsigned size = 0; size < 4; ++size)
        assert(gxm_notification_offset(&notification, base, size, &offset) != 0);
    assert(gxm_notification_offset(&notification, UINTPTR_MAX - 3, 8, &offset) != 0);
    puts("Notification alignment, bounds and overflow checks passed.");
    return 0;
}
