#ifndef GXM_RENDER_SURFACE_H
#define GXM_RENDER_SURFACE_H
#include <stdbool.h>
#include <stdint.h>
#include <deko3d.h>
typedef struct {
    uint32_t size;
    uint16_t width,height;
    DkMsMode ms_mode;
    DkMemBlock memblock;
    DkImage image;
    DkImageView view;
} dk_surface_t;
/* Empty output on allocation failure. Destroy only after GPU references retire. */
bool dk_surface_create(DkDevice device,dk_surface_t *surface,uint32_t width,uint32_t height,
                       DkImageFormat format,uint32_t flags);
bool dk_surface_create_ms(DkDevice device,dk_surface_t *surface,uint32_t width,uint32_t height,
                          DkImageFormat format,uint32_t flags,DkMsMode mode);
void dk_surface_destroy(dk_surface_t *surface);
/* Requires distinct fresh outputs. Allocation failure empties all three. */
bool dk_render_surfaces_create(DkDevice device,dk_surface_t *color,dk_surface_t *depth,
    dk_surface_t *resolve,uint32_t width,uint32_t height,DkMsMode mode);
/* After draw submission commands, return a single-sample color copy source.
 * Invalid surfaces emit no commands. The returned image remains caller-owned. */
const DkImage *dk_surface_resolve_color(DkCmdBuf cmd,const dk_surface_t *color,
    const dk_surface_t *resolve);
#endif
