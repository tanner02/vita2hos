#ifndef GXM_SHADER_CODE_H
#define GXM_SHADER_CODE_H
#include <deko3d.h>
#include <stdint.h>

typedef struct GxmShaderCode GxmShaderCode;
/* Caller serializes list access and idles the GPU before clearing the list. */
int gxm_shader_code_upload(DkDevice device, GxmShaderCode **list, const void *data,
                            uint32_t size, DkMemBlock *block);
void gxm_shader_code_clear(GxmShaderCode **list);
#endif
