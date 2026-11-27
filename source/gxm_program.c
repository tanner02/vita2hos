#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "gxm/program.h"
#include "module.h"

EXPORT(SceGxm, 0xED8B6C69, int, sceGxmProgramCheck, const SceGxmProgram *program)
{
    if (!program) return SCE_GXM_ERROR_INVALID_POINTER;
    return memcmp(&program->magic, "GXP", 4) ? SCE_GXM_ERROR_NULL_PROGRAM : 0;
}

EXPORT(SceGxm, 0x04BB3C59, SceGxmProgramType, sceGxmProgramGetType,
       const SceGxmProgram *program)
{
    return program && (program->program_flags & 1) ? SCE_GXM_FRAGMENT_PROGRAM
                                                 : SCE_GXM_VERTEX_PROGRAM;
}

EXPORT(SceGxm, 0xBF5E2090, unsigned int, sceGxmProgramGetSize, const SceGxmProgram *program)
{
    return program ? program->size : 0;
}

EXPORT(SceGxm, 0xD5D5FCCD, unsigned int, sceGxmProgramGetParameterCount,
       const SceGxmProgram *program)
{
    return program ? program->parameter_count : 0;
}

/* GXP offsets are relative to their own field/record. The caller must provide
 * a readable program of the declared size; malformed tables cannot escape it. */
static const SceGxmProgramParameter *program_parameters(const SceGxmProgram *program)
{
    if (!program || program->size < sizeof(*program) || sceGxmProgramCheck(program)) return NULL;
    uint64_t offset=(uint64_t)offsetof(SceGxmProgram, parameters_offset)+program->parameters_offset;
    uint64_t bytes=(uint64_t)program->parameter_count*sizeof(SceGxmProgramParameter);
    if (offset < sizeof(*program) || offset > program->size || bytes > program->size-offset ||
        offset % _Alignof(SceGxmProgramParameter) ||
        (uintptr_t)program > UINTPTR_MAX-program->size) return NULL;
    return (const SceGxmProgramParameter *)((const char *)program+(size_t)offset);
}

EXPORT(SceGxm, 0x06FF9151, const SceGxmProgramParameter *, sceGxmProgramGetParameter,
       const SceGxmProgram *program, unsigned int index)
{
    const SceGxmProgramParameter *parameters=program_parameters(program);
    return parameters && index < program->parameter_count ? &parameters[index] : NULL;
}

EXPORT(SceGxm, 0x633CAA54, const SceGxmProgramParameter *, sceGxmProgramFindParameterBySemantic,
       const SceGxmProgram *program, SceGxmParameterSemantic semantic, unsigned int index)
{
    if (semantic == SCE_GXM_PARAMETER_SEMANTIC_NONE) return NULL;
    const SceGxmProgramParameter *parameters=program_parameters(program);
    if (!parameters) return NULL;
    for (uint32_t i=0; i<program->parameter_count; ++i)
        if (parameters[i].semantic == semantic && parameters[i].semantic_index == index)
            return &parameters[i];
    return NULL;
}

EXPORT(SceGxm, 0x7FFFDD7A, const SceGxmProgramParameter *, _sceGxmProgramFindParameterBySemantic,
       const SceGxmProgram *program, SceGxmParameterSemantic semantic, unsigned int index)
{
    return sceGxmProgramFindParameterBySemantic(program,semantic,index);
}

EXPORT(SceGxm, 0x277794C4, const SceGxmProgramParameter *, sceGxmProgramFindParameterByName,
       const SceGxmProgram *program, const char *name)
{
    if (!name) return NULL;
    const SceGxmProgramParameter *parameters=program_parameters(program);
    if (!parameters) return NULL;
    for (uint32_t i=0; i<program->parameter_count; ++i) {
        int64_t offset=(const char *)&parameters[i]-(const char *)program;
        offset+=parameters[i].name_offset;
        if (offset < 0 || (uint64_t)offset >= program->size) return NULL;
        const char *candidate=(const char *)program+(size_t)offset;
        if (!memchr(candidate,0,program->size-(size_t)offset)) return NULL;
        if (!strcmp(candidate,name)) return &parameters[i];
    }
    return NULL;
}
