#ifndef GXM_BLEND_H
#define GXM_BLEND_H
#include <stdbool.h>
#include <psp2/gxm.h>
#include <deko3d.h>
typedef struct { DkBlendState state; bool enabled; } GxmBlendState;
int gxm_translate_blend(const SceGxmBlendInfo *blend, GxmBlendState *translated);
#endif
