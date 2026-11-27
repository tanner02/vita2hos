#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
static bool fail_realloc;
static void *checked_realloc(void *pointer, size_t size)
{ return fail_realloc ? NULL : realloc(pointer, size); }
#define realloc checked_realloc
#include "../../source/gxm_program_registry.c"
#undef realloc

int main(void)
{
    int objects[3];
    SceGxmShaderPatcherId a = (void *)&objects[0], b = (void *)&objects[1], c = (void *)&objects[2];
    SceGxmShaderPatcherId *entries = NULL;
    uint32_t count = 0;
    assert(gxm_program_registry_add(&entries, &count, a) == 0 && count == 1);
    SceGxmShaderPatcherId *original = entries;
    fail_realloc = true;
    assert((uint32_t)gxm_program_registry_add(&entries, &count, b) == SCE_GXM_ERROR_OUT_OF_MEMORY);
    assert(entries == original && count == 1 && entries[0] == a);
    fail_realloc = false;
    assert(gxm_program_registry_add(&entries, &count, b) == 0);
    assert(gxm_program_registry_add(&entries, &count, c) == 0 && count == 3);
    assert(gxm_program_registry_remove(entries, &count, b) == 0);
    assert(count == 2 && entries[0] == a && entries[1] == c && entries[2] == NULL);
    assert((uint32_t)gxm_program_registry_remove(entries, &count, b) == SCE_GXM_ERROR_INVALID_VALUE);
    assert(count == 2);
    assert(gxm_program_registry_remove(entries, &count, a) == 0 && count == 1 && entries[0] == c);
    assert(gxm_program_registry_remove(entries, &count, c) == 0 && count == 0 && entries[0] == NULL);
    assert((uint32_t)gxm_program_registry_remove(entries, &count, c) == SCE_GXM_ERROR_INVALID_VALUE);
    original = entries;
    count = UINT32_MAX;
    assert((uint32_t)gxm_program_registry_add(&entries, &count, a) == SCE_GXM_ERROR_OUT_OF_MEMORY);
    assert(count == UINT32_MAX && entries == original);
    free(entries);
    puts("Shader program registry regression checks passed.");
    return 0;
}
