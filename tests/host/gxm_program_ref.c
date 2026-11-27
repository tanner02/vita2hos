#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include "../../source/gxm_program_ref.c"

static GxmProgramRef ref;
static atomic_uint destructions;

static void *retain_cycles(void *unused)
{
    (void)unused;
    for (unsigned i = 0; i < 20000; ++i) {
        bool destroy;
        assert(!gxm_program_ref_add(&ref));
        assert(!gxm_program_ref_release(&ref, &destroy) && !destroy);
    }
    return NULL;
}

static void *drop_owner(void *unused)
{
    (void)unused;
    bool destroy;
    assert(!gxm_program_ref_release(&ref, &destroy));
    if (destroy) atomic_fetch_add(&destructions, 1);
    return NULL;
}

int main(void)
{
    atomic_init(&ref.count, 1);
    atomic_init(&destructions, 0);
    unsigned count;
    bool destroy;
    assert(!gxm_program_ref_add(&ref));
    assert(!gxm_program_ref_get(&ref, &count) && count == 2);
    assert(!gxm_program_ref_release(&ref, &destroy) && !destroy);
    pthread_t workers[4];
    for (unsigned i = 0; i < 4; ++i) assert(!pthread_create(&workers[i], NULL, retain_cycles, NULL));
    for (unsigned i = 0; i < 4; ++i) assert(!pthread_join(workers[i], NULL));
    assert(!gxm_program_ref_get(&ref, &count) && count == 1);
    for (unsigned i = 0; i < 3; ++i) assert(!gxm_program_ref_add(&ref));
    for (unsigned i = 0; i < 4; ++i) assert(!pthread_create(&workers[i], NULL, drop_owner, NULL));
    for (unsigned i = 0; i < 4; ++i) assert(!pthread_join(workers[i], NULL));
    assert(atomic_load(&destructions) == 1 && atomic_load(&ref.count) == 0);
    assert((uint32_t)gxm_program_ref_add(&ref) == SCE_GXM_ERROR_INVALID_VALUE);
    assert((uint32_t)gxm_program_ref_release(&ref, &destroy) == SCE_GXM_ERROR_INVALID_VALUE && !destroy);
    atomic_store(&ref.count, UINT_MAX);
    assert((uint32_t)gxm_program_ref_add(&ref) == SCE_GXM_ERROR_INVALID_VALUE);
    assert(!gxm_program_ref_get(&ref, &count) && count == UINT_MAX);
    assert((uint32_t)gxm_program_ref_add(NULL) == SCE_GXM_ERROR_INVALID_POINTER);
    assert((uint32_t)gxm_program_ref_release(NULL, &destroy) == SCE_GXM_ERROR_INVALID_POINTER && !destroy);
    assert((uint32_t)gxm_program_ref_release(&ref, NULL) == SCE_GXM_ERROR_INVALID_POINTER);
    assert((uint32_t)gxm_program_ref_get(&ref, NULL) == SCE_GXM_ERROR_INVALID_POINTER);
    puts("Compiled shader reference lifetime and concurrency checks passed.");
}
