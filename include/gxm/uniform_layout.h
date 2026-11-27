#ifndef GXM_UNIFORM_LAYOUT_H
#define GXM_UNIFORM_LAYOUT_H
#include <stdbool.h>
#include <stdint.h>

/* Host slot zero is default; slots 1..14 correspond to guest buffers 0..13. */
#define GXM_UNIFORM_LAYOUT_SLOTS 15
typedef struct {
    uint32_t bytes[GXM_UNIFORM_LAYOUT_SLOTS];
    uint32_t offsets[GXM_UNIFORM_LAYOUT_SLOTS];
    uint32_t size;
} GxmUniformLayout;

#ifdef __cplusplus
extern "C" {
#endif
bool gxm_uniform_layout_build(const uint32_t float_counts[GXM_UNIFORM_LAYOUT_SLOTS],
                              GxmUniformLayout *layout);
#ifdef __cplusplus
}
#endif
#endif
