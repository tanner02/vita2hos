#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
static bool fail_allocation;
static void *allocate(size_t size) { return fail_allocation ? NULL : malloc(size); }
static void *zero_allocate(size_t count, size_t size) { return fail_allocation ? NULL : calloc(count,size); }
#define malloc allocate
#define calloc zero_allocate
#include "../../source/guest_tls.c"
#undef malloc
#undef calloc
int main(void)
{
    GuestTlsTemplate template={0}; unsigned char initial[]={1,2,3,4};
    assert(guest_tls_template_create(&template, NULL, 4, 16) < 0);
    assert(guest_tls_template_create(&template, initial, 4, 3) < 0);
    assert(guest_tls_template_create(&template, initial, 4, UINT32_MAX) < 0);
    fail_allocation=true;
    assert(guest_tls_template_create(&template, initial, 4, 16) < 0 && !template.data);
    fail_allocation=false;
    assert(guest_tls_template_create(&template, initial, 4, 16) == 0);
    assert(guest_tls_template_create(&template, initial, 4, 16) < 0); /* No owner overwrite. */
    initial[0]=99;
    unsigned char *first=guest_tls_allocate(&template), *second=guest_tls_allocate(&template);
    assert(first && second && first!=second);
    for (unsigned i=0;i<GUEST_KERNEL_TLS_SIZE;++i) assert(first[i]==0);
    unsigned char *user=guest_tls_user_area(first);
    assert(user[0]==1 && user[3]==4);
    for (int i=4;i<16;++i) assert(user[i]==0);
    user[0]=55;
    assert(((unsigned char *)guest_tls_user_area(second))[0]==1);
    fail_allocation=true;
    assert(guest_tls_allocate(&template)==NULL);
    fail_allocation=false;
    guest_tls_template_clear(&template);
    assert(!template.data && !template.filesz && !template.memsz);
    free(first); free(second);
    assert(guest_tls_template_create(&template, NULL, 0, 16)==0);
    first=guest_tls_allocate(&template);assert(first);
    for (int i=0;i<16;++i) assert(((unsigned char *)guest_tls_user_area(first))[i]==0);
    free(first);guest_tls_template_clear(&template);
    assert(guest_tls_allocate(NULL)==NULL && guest_tls_user_area(NULL)==NULL);
    puts("Guest TLS snapshot, per-thread initialization, isolation and allocation checks passed.");
}
