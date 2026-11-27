#ifndef GXM_PROGRAM_REGISTRY_H
#define GXM_PROGRAM_REGISTRY_H
#include <psp2/gxm.h>
#include <stdint.h>
int gxm_program_registry_add(SceGxmShaderPatcherId **entries, uint32_t *count,
                              SceGxmShaderPatcherId program);
int gxm_program_registry_remove(SceGxmShaderPatcherId *entries, uint32_t *count,
                                 SceGxmShaderPatcherId program);
#endif
