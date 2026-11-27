#ifndef GXM_NOTIFICATION_H
#define GXM_NOTIFICATION_H
#include <stddef.h>
#include <stdint.h>
#include <psp2/gxm.h>

int gxm_notification_offset(const SceGxmNotification *notification, uintptr_t base,
                             size_t size, uint32_t *offset);
#endif
