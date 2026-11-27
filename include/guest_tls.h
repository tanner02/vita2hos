#ifndef GUEST_TLS_H
#define GUEST_TLS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define GUEST_KERNEL_TLS_SIZE 0x800
typedef struct {
    void *data; /* Owned immutable initialization snapshot. */
    uint32_t filesz, memsz;
} GuestTlsTemplate;
bool guest_tls_template_valid(const GuestTlsTemplate *template);
int guest_tls_template_create(GuestTlsTemplate *template, const void *data,
                               uint32_t filesz, uint32_t memsz);
void guest_tls_template_clear(GuestTlsTemplate *template);
void *guest_tls_allocate(const GuestTlsTemplate *template);
static inline void *guest_tls_user_area(void *allocation)
{
    return allocation ? (char *)allocation + GUEST_KERNEL_TLS_SIZE : NULL;
}
#endif
