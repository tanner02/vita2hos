#ifndef GXM_RENDER_SURFACE_H
#define GXM_RENDER_SURFACE_H
#include <stdbool.h>
#include <stdint.h>
#include <deko3d.h>
typedef struct {
    uint32_t size;
    uint16_t width,height;
    DkMemBlock memblock;
    DkImage image;
    DkImageView view;
} dk_surface_t;
/* Empty output on allocation failure. Destroy only after GPU references retire. */
bool dk_surface_create(DkDevice device,dk_surface_t *surface,uint32_t width,uint32_t height,
                       DkImageFormat format,uint32_t flags);
void dk_surface_destroy(dk_surface_t *surface);
#endif
