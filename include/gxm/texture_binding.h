#ifndef GXM_TEXTURE_BINDING_H
#define GXM_TEXTURE_BINDING_H
#include <stdbool.h>

#define GXM_TEXTURE_STAGE_UNITS 16
#define GXM_TEXTURE_BINDING_COUNT (2 * GXM_TEXTURE_STAGE_UNITS)

/* Matches the pinned translator's GLSL sampler bindings. Vertex bindings
 * retain their offset even though deko3d also selects a shader stage. The
 * caller validates unit < GXM_TEXTURE_STAGE_UNITS before using this index. */
static inline unsigned gxm_texture_binding_index(bool vertex, unsigned unit)
{
    return unit + (vertex ? GXM_TEXTURE_STAGE_UNITS : 0);
}
#endif
