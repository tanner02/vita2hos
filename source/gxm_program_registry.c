#include <stdlib.h>
#include <string.h>
#include "gxm/program_registry.h"

int gxm_program_registry_add(SceGxmShaderPatcherId **entries, uint32_t *count,
                              SceGxmShaderPatcherId program)
{
    if (!entries || !count || !program) return SCE_GXM_ERROR_INVALID_POINTER;
    if (*count == UINT32_MAX || (uint64_t)*count + 1 > SIZE_MAX / sizeof(**entries))
        return SCE_GXM_ERROR_OUT_OF_MEMORY;
    if (*count && !*entries) return SCE_GXM_ERROR_INVALID_POINTER;
    SceGxmShaderPatcherId *replacement = realloc(*entries, ((size_t)*count + 1) * sizeof(*replacement));
    if (!replacement) return SCE_GXM_ERROR_OUT_OF_MEMORY;
    replacement[*count] = program;
    *entries = replacement;
    ++*count;
    return 0;
}

int gxm_program_registry_remove(SceGxmShaderPatcherId *entries, uint32_t *count,
                                 SceGxmShaderPatcherId program)
{
    if (!count || !program || (*count && !entries)) return SCE_GXM_ERROR_INVALID_POINTER;
    for (uint32_t i = 0; i < *count; ++i) {
        if (entries[i] != program) continue;
        memmove(entries + i, entries + i + 1, ((size_t)*count - i - 1) * sizeof(*entries));
        entries[--*count] = NULL;
        return 0;
    }
    return SCE_GXM_ERROR_INVALID_VALUE;
}
