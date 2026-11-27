#include <stdlib.h>
#include <string.h>
#include <psp2/gxm.h>
#include "gxm/shader_code.h"

struct GxmShaderCode {
    GxmShaderCode *next;
    DkMemBlock block;
};

int gxm_shader_code_upload(DkDevice device, GxmShaderCode **list, const void *data,
                            uint32_t size, DkMemBlock *block)
{
    if (!block) return SCE_GXM_ERROR_INVALID_POINTER;
    *block = NULL;
    if (!device || !list || !data) return SCE_GXM_ERROR_INVALID_POINTER;
    if (!size) return SCE_GXM_ERROR_INVALID_VALUE;
    if (size > UINT32_MAX - (DK_MEMBLOCK_ALIGNMENT - 1U))
        return SCE_GXM_ERROR_OUT_OF_MEMORY;
    GxmShaderCode *code = calloc(1, sizeof(*code));
    if (!code) return SCE_GXM_ERROR_OUT_OF_MEMORY;
    uint32_t allocation = (size + DK_MEMBLOCK_ALIGNMENT - 1U) & ~(DK_MEMBLOCK_ALIGNMENT - 1U);
    DkMemBlockMaker maker;
    dkMemBlockMakerDefaults(&maker, device, allocation);
    maker.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Code;
    code->block = dkMemBlockCreate(&maker);
    if (!code->block) {
        free(code);
        return SCE_GXM_ERROR_OUT_OF_MEMORY;
    }
    void *destination = dkMemBlockGetCpuAddr(code->block);
    if (!destination) {
        dkMemBlockDestroy(code->block);
        free(code);
        return SCE_GXM_ERROR_INVALID_POINTER;
    }
    memcpy(destination, data, size);
    memset((char *)destination + size, 0, allocation - size);
    code->next = *list;
    *list = code;
    *block = code->block;
    return 0;
}

void gxm_shader_code_clear(GxmShaderCode **list)
{
    if (!list) return;
    while (*list) {
        GxmShaderCode *code = *list;
        *list = code->next;
        dkMemBlockDestroy(code->block);
        free(code);
    }
}
