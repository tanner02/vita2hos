#ifndef GXM_DEPTH_TRANSFER_H
#define GXM_DEPTH_TRANSFER_H
#include <stddef.h>
#include <stdint.h>
#include <psp2/gxm.h>

/* CPU staging representation, not a declaration of NVIDIA image storage.
 * The float depth bit pattern is retained, including NaNs and signed zero. */
typedef struct { uint32_t depth_bits, stencil; } GxmDepthSample;
typedef struct {
    uint32_t width, height, guest_stride, sample_stride;
    size_t depth_bytes, stencil_bytes, sample_count;
    unsigned depth_bytes_per_pixel;
    int has_stencil;
} GxmDepthTransfer;

/* Linear DF32, S8 and separate DF32_S8 planes. Unsupported layouts/formats
 * fail without changing output. sample_stride permits GPU staging row padding.
 * Guest stride is 32-aligned and at most 8192 pixels; dimensions are 1..4096.
 * Span sizes cover active pixels, excluding unused trailing row padding, and
 * fit the ARM32 address space. */
int gxm_depth_transfer_plan(GxmDepthTransfer *plan,SceGxmDepthStencilFormat format,
    SceGxmDepthStencilSurfaceType type,uint32_t width,uint32_t height,
    uint32_t guest_stride,uint32_t sample_stride);
/* NULL/zero-length planes are unbacked: read uses the supplied background and
 * write leaves them absent. Planes unused by the format are ignored. Other
 * spans and every destination must be large enough. Failure leaves outputs
 * unchanged. Destinations must not overlap source spans, plan metadata, or
 * another destination.
 * Stencil values use only their low eight bits. Row padding is untouched. */
int gxm_depth_transfer_read(const GxmDepthTransfer *plan,
    const void *depth,size_t depth_size,const void *stencil,size_t stencil_size,
    uint32_t background_depth_bits,uint8_t background_stencil,
    GxmDepthSample *samples,size_t sample_count);
int gxm_depth_transfer_write(const GxmDepthTransfer *plan,
    const GxmDepthSample *samples,size_t sample_count,
    void *depth,size_t depth_size,void *stencil,size_t stencil_size);
#endif
