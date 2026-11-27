#include "gxm/notification.h"

int gxm_notification_offset(const SceGxmNotification *notification, uintptr_t base,
                             size_t size, uint32_t *offset)
{
    if (!notification || !offset || !base || !notification->address)
        return SCE_GXM_ERROR_INVALID_POINTER;
    uintptr_t address = (uintptr_t)notification->address;
    if (address % sizeof(uint32_t)) return SCE_GXM_ERROR_INVALID_ALIGNMENT;
    if (size < sizeof(uint32_t) || size - 1 > UINTPTR_MAX - base || address < base)
        return SCE_GXM_ERROR_INVALID_POINTER;
    uintptr_t relative = address - base;
    if (relative > size - sizeof(uint32_t) || relative > UINT32_MAX)
        return SCE_GXM_ERROR_INVALID_POINTER;
    *offset = (uint32_t)relative;
    return 0;
}
