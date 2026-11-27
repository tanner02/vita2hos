#include "guest_tls.h"
#include <stdlib.h>
#include <string.h>

bool guest_tls_template_valid(const GuestTlsTemplate *template)
{
    return template && template->filesz <= template->memsz &&
        (!template->filesz || template->data) &&
        template->memsz <= UINT32_MAX - GUEST_KERNEL_TLS_SIZE;
}
int guest_tls_template_create(GuestTlsTemplate *template, const void *data,
                               uint32_t filesz, uint32_t memsz)
{
    if (!template || template->data || template->filesz || template->memsz) return -1;
    GuestTlsTemplate next = {(void *)data, filesz, memsz};
    if (!guest_tls_template_valid(&next)) return -1;
    next.data = filesz ? malloc(filesz) : NULL;
    if (filesz && !next.data) return -1;
    if (filesz) memcpy(next.data, data, filesz);
    *template = next;
    return 0;
}
void guest_tls_template_clear(GuestTlsTemplate *template)
{
    if (!template) return;
    free(template->data);
    *template = (GuestTlsTemplate){0};
}
void *guest_tls_allocate(const GuestTlsTemplate *template)
{
    if (!guest_tls_template_valid(template)) return NULL;
    void *allocation = calloc(1, GUEST_KERNEL_TLS_SIZE + (size_t)template->memsz);
    if (allocation && template->filesz)
        memcpy(guest_tls_user_area(allocation), template->data, template->filesz);
    return allocation;
}
