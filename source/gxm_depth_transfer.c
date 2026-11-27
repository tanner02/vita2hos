#include <stdbool.h>
#include <string.h>
#include "gxm/depth_transfer.h"

_Static_assert(sizeof(GxmDepthSample)==8,"Depth sample staging stride");
_Static_assert(offsetof(GxmDepthSample,stencil)==4,"Depth sample stencil offset");

int gxm_depth_transfer_plan(GxmDepthTransfer *plan,SceGxmDepthStencilFormat format,
    SceGxmDepthStencilSurfaceType type,uint32_t width,uint32_t height,
    uint32_t guest_stride,uint32_t sample_stride)
{
    if (!plan) return SCE_GXM_ERROR_INVALID_POINTER;
    if (type!=SCE_GXM_DEPTH_STENCIL_SURFACE_LINEAR) return SCE_GXM_ERROR_UNSUPPORTED;
    unsigned depth_bytes_per_pixel;int has_stencil;
    switch (format) {
    case SCE_GXM_DEPTH_STENCIL_FORMAT_DF32: depth_bytes_per_pixel=4;has_stencil=0;break;
    case SCE_GXM_DEPTH_STENCIL_FORMAT_S8: depth_bytes_per_pixel=0;has_stencil=1;break;
    case SCE_GXM_DEPTH_STENCIL_FORMAT_DF32_S8: depth_bytes_per_pixel=4;has_stencil=1;break;
    default:return SCE_GXM_ERROR_UNSUPPORTED;
    }
    if (!width || width>4096 || !height || height>4096 || guest_stride<width ||
        guest_stride>8192 || guest_stride%32 || sample_stride<width)
        return SCE_GXM_ERROR_INVALID_VALUE;
    uint64_t guest_pixels=(uint64_t)(height-1)*guest_stride+width;
    uint64_t samples=(uint64_t)(height-1)*sample_stride+width;
    uint64_t depth_bytes=guest_pixels*depth_bytes_per_pixel;
    uint64_t stencil_bytes=has_stencil ? guest_pixels : 0;
    /* Native transfer spans must fit the ARM32 address space even in host tests. */
    if (depth_bytes>UINT32_MAX || stencil_bytes>UINT32_MAX ||
        samples>UINT32_MAX/sizeof(GxmDepthSample)) return SCE_GXM_ERROR_INVALID_VALUE;
    *plan=(GxmDepthTransfer){.width=width,.height=height,.guest_stride=guest_stride,
        .sample_stride=sample_stride,.depth_bytes=(size_t)depth_bytes,
        .stencil_bytes=(size_t)stencil_bytes,.sample_count=(size_t)samples,
        .depth_bytes_per_pixel=depth_bytes_per_pixel,.has_stencil=has_stencil};
    return 0;
}

/* A copied/corrupted plan cannot bypass span validation. Rebuild all derived
 * sizes from its shape before any source read or destination write. */
int gxm_depth_transfer_check_plan(const GxmDepthTransfer *plan)
{
    if (!plan) return SCE_GXM_ERROR_INVALID_POINTER;
    SceGxmDepthStencilFormat format;
    if (plan->depth_bytes_per_pixel==4 && plan->has_stencil==0) format=SCE_GXM_DEPTH_STENCIL_FORMAT_DF32;
    else if (plan->depth_bytes_per_pixel==4 && plan->has_stencil==1) format=SCE_GXM_DEPTH_STENCIL_FORMAT_DF32_S8;
    else if (!plan->depth_bytes_per_pixel && plan->has_stencil==1) format=SCE_GXM_DEPTH_STENCIL_FORMAT_S8;
    else return SCE_GXM_ERROR_INVALID_VALUE;
    GxmDepthTransfer expected;
    int result=gxm_depth_transfer_plan(&expected,format,SCE_GXM_DEPTH_STENCIL_SURFACE_LINEAR,
        plan->width,plan->height,plan->guest_stride,plan->sample_stride);
    if (result) return result;
    if (plan->depth_bytes!=expected.depth_bytes || plan->stencil_bytes!=expected.stencil_bytes ||
        plan->sample_count!=expected.sample_count) return SCE_GXM_ERROR_INVALID_VALUE;
    return 0;
}

static int span(const void *data,size_t size,size_t required,bool allow_absent)
{
    if (!required) return 0; /* A plane unused by this format is ignored. */
    if (!data) return allow_absent && !size ? 0 : SCE_GXM_ERROR_INVALID_POINTER;
    if (size<required || (uintptr_t)data>UINTPTR_MAX-required) return SCE_GXM_ERROR_INVALID_VALUE;
    return 0;
}
static bool overlap(const void *a,size_t a_size,const void *b,size_t b_size)
{
    if (!a || !b || !a_size || !b_size) return false;
    uintptr_t left=(uintptr_t)a,right=(uintptr_t)b;
    return left<=right ? right-left<a_size : left-right<b_size;
}
static int validate_buffers(const GxmDepthTransfer *plan,const void *depth,size_t depth_size,
    const void *stencil,size_t stencil_size,const GxmDepthSample *samples,size_t count,bool writing)
{
    int result=gxm_depth_transfer_check_plan(plan);
    if (result) return result;
    if (count<plan->sample_count) return SCE_GXM_ERROR_INVALID_VALUE;
    if ((uintptr_t)samples%_Alignof(GxmDepthSample)) return SCE_GXM_ERROR_INVALID_ALIGNMENT;
    size_t sample_bytes=plan->sample_count*sizeof(*samples);
    result=span(samples,sample_bytes,sample_bytes,false);
    if (result) return result;
    result=span(depth,depth_size,plan->depth_bytes,true);
    if (result) return result;
    result=span(stencil,stencil_size,plan->stencil_bytes,true);
    if (result) return result;
    if (overlap(plan,sizeof(*plan),samples,sample_bytes) ||
        (writing && (overlap(plan,sizeof(*plan),depth,plan->depth_bytes) ||
                     overlap(plan,sizeof(*plan),stencil,plan->stencil_bytes))) ||
        overlap(samples,sample_bytes,depth,plan->depth_bytes) ||
        overlap(samples,sample_bytes,stencil,plan->stencil_bytes) ||
        (writing && overlap(depth,plan->depth_bytes,stencil,plan->stencil_bytes)))
        return SCE_GXM_ERROR_INVALID_VALUE;
    return 0;
}

int gxm_depth_transfer_check_write(const GxmDepthTransfer *plan,
    const GxmDepthSample *samples,size_t sample_count,
    void *depth,size_t depth_size,void *stencil,size_t stencil_size)
{
    return validate_buffers(plan,depth,depth_size,stencil,stencil_size,samples,sample_count,true);
}

int gxm_depth_transfer_read(const GxmDepthTransfer *plan,
    const void *depth,size_t depth_size,const void *stencil,size_t stencil_size,
    uint32_t background_depth_bits,uint8_t background_stencil,
    GxmDepthSample *samples,size_t sample_count)
{
    int result=validate_buffers(plan,depth,depth_size,stencil,stencil_size,samples,sample_count,false);
    if (result) return result;
    for (uint32_t y=0;y<plan->height;++y) for (uint32_t x=0;x<plan->width;++x) {
        size_t guest=(size_t)y*plan->guest_stride+x;
        GxmDepthSample value={.depth_bits=background_depth_bits,.stencil=background_stencil};
        if (plan->depth_bytes_per_pixel && depth) memcpy(&value.depth_bits,(const char *)depth+guest*4,4);
        if (plan->has_stencil && stencil) value.stencil=((const unsigned char *)stencil)[guest];
        samples[(size_t)y*plan->sample_stride+x]=value;
    }
    return 0;
}

int gxm_depth_transfer_write(const GxmDepthTransfer *plan,
    const GxmDepthSample *samples,size_t sample_count,
    void *depth,size_t depth_size,void *stencil,size_t stencil_size)
{
    int result=validate_buffers(plan,depth,depth_size,stencil,stencil_size,samples,sample_count,true);
    if (result) return result;
    for (uint32_t y=0;y<plan->height;++y) for (uint32_t x=0;x<plan->width;++x) {
        size_t guest=(size_t)y*plan->guest_stride+x;
        GxmDepthSample value=samples[(size_t)y*plan->sample_stride+x];
        if (plan->depth_bytes_per_pixel && depth) memcpy((char *)depth+guest*4,&value.depth_bits,4);
        if (plan->has_stencil && stencil) ((unsigned char *)stencil)[guest]=(uint8_t)value.stencil;
    }
    return 0;
}
