#ifndef VITA2HOS_DRAW_PROBE_H
#define VITA2HOS_DRAW_PROBE_H
#include <psp2/gxm.h>
#include <stdint.h>
typedef struct {
    void *(*allocate)(unsigned size);
    void (*record)(const char *text);
    void (*result)(const char *name, int code);
} ProbeHooks;
int probe_draw(SceGxmContext *context, SceGxmRenderTarget *target,
               SceGxmColorSurface *surface, uint32_t *pixels, const ProbeHooks *hooks);
int probe_cube(SceGxmContext *context, SceGxmVertexProgram *vertex,
               SceGxmFragmentProgram *fragment, const SceGxmProgramParameter *matrix,
               const SceGxmProgramParameter *sampler, const ProbeHooks *hooks);
#endif
