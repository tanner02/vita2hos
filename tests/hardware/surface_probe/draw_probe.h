#ifndef VITA2HOS_DRAW_PROBE_H
#define VITA2HOS_DRAW_PROBE_H
#include <psp2/gxm.h>
#include <stdint.h>
typedef struct {
    void *(*allocate)(unsigned size);
    void (*record)(const char *text);
    void (*result)(const char *name, int code);
} ProbeHooks;
int probe_vertex_fetch(SceGxmContext *context, SceGxmRenderTarget *target,
    SceGxmColorSurface *surface, uint32_t *pixels, SceGxmShaderPatcher *patcher,
    SceGxmShaderPatcherId vertex_id, SceGxmFragmentProgram *fragment,
    const SceGxmProgramParameter *position, const SceGxmProgramParameter *uv,
    const SceGxmProgramParameter *matrix, const SceGxmProgramParameter *sampler,
    const ProbeHooks *hooks);
int probe_texture_mips(SceGxmContext *context, SceGxmRenderTarget *target,
    SceGxmColorSurface *surface, uint32_t *pixels, SceGxmVertexProgram *vertex,
    SceGxmFragmentProgram *fragment, const SceGxmProgramParameter *matrix,
    const SceGxmProgramParameter *sampler, float vertices[][8], uint16_t *indices,
    const ProbeHooks *hooks);
int probe_texture_alpha(SceGxmContext *context,SceGxmRenderTarget *target,
    SceGxmColorSurface *surface,uint32_t *pixels,SceGxmVertexProgram *vertex,
    SceGxmFragmentProgram *fragment,SceGxmShaderPatcher *patcher,
    SceGxmShaderPatcherId fragment_id,const SceGxmProgram *vertex_source,
    const SceGxmProgramParameter *matrix,const SceGxmProgramParameter *sampler,
    float vertices[][8],uint16_t *indices,const ProbeHooks *hooks);
int probe_draw(SceGxmContext *context, SceGxmRenderTarget *target,
               SceGxmColorSurface *surface, uint32_t *pixels, const ProbeHooks *hooks);
int probe_cube(SceGxmContext *context, SceGxmVertexProgram *vertex,
               SceGxmFragmentProgram *fragment, const SceGxmProgramParameter *matrix,
               const SceGxmProgramParameter *sampler, const ProbeHooks *hooks);
#endif
