#ifndef TEST_DEKO3D_H
#define TEST_DEKO3D_H
#include <stddef.h>
#include <stdint.h>
typedef void *DkMemBlock;
typedef void *DkDevice;
typedef struct { float x, y, width, height, near, far; } DkViewport;
typedef struct { uint32_t x, y, width, height; } DkScissor;
typedef enum { DkFace_None = 0, DkFace_Front = 1, DkFace_Back = 2, DkFace_FrontAndBack = 3 } DkFace;
typedef enum { DkBlendOp_Add = 1, DkBlendOp_Sub, DkBlendOp_RevSub, DkBlendOp_Min, DkBlendOp_Max } DkBlendOp;
typedef enum {
    DkBlendFactor_Zero = 1, DkBlendFactor_One, DkBlendFactor_SrcColor, DkBlendFactor_InvSrcColor,
    DkBlendFactor_SrcAlpha, DkBlendFactor_InvSrcAlpha, DkBlendFactor_DstAlpha,
    DkBlendFactor_InvDstAlpha, DkBlendFactor_DstColor, DkBlendFactor_InvDstColor,
    DkBlendFactor_SrcAlphaSaturate,
} DkBlendFactor;
typedef struct {
    uint32_t colorBlendOp : 3, srcColorBlendFactor : 6, dstColorBlendFactor : 6;
    uint32_t alphaBlendOp : 3, srcAlphaBlendFactor : 6, dstAlphaBlendFactor : 6;
} DkBlendState;
typedef enum {
    DkPrimitive_Points = 0, DkPrimitive_Lines = 1, DkPrimitive_Triangles = 4,
    DkPrimitive_TriangleStrip = 5, DkPrimitive_TriangleFan = 6,
} DkPrimitive;
typedef enum {
    DkImageFormat_R8_Unorm, DkImageFormat_R8_Snorm,
    DkImageFormat_R16_Unorm, DkImageFormat_R16_Snorm, DkImageFormat_R16_Float,
    DkImageFormat_R32_Float, DkImageFormat_RG8_Unorm, DkImageFormat_RG8_Snorm,
    DkImageFormat_RG16_Unorm, DkImageFormat_RG16_Snorm, DkImageFormat_RG16_Float,
    DkImageFormat_RG32_Float, DkImageFormat_RGBA8_Unorm, DkImageFormat_RGBA8_Snorm,
    DkImageFormat_RGBA16_Unorm, DkImageFormat_RGBA16_Snorm, DkImageFormat_RGBA16_Float,
    DkImageFormat_RGBA_BC1, DkImageFormat_RGBA_BC2, DkImageFormat_RGBA_BC3,
    DkImageFormat_R_BC4_Unorm, DkImageFormat_R_BC4_Snorm,
    DkImageFormat_RG_BC5_Unorm, DkImageFormat_RG_BC5_Snorm,
} DkImageFormat;
typedef enum {
    DkImageSwizzle_Zero, DkImageSwizzle_One, DkImageSwizzle_Red,
    DkImageSwizzle_Green, DkImageSwizzle_Blue, DkImageSwizzle_Alpha,
} DkImageSwizzle;
enum {
    DkMemBlockFlags_CpuCached = 1,
    DkMemBlockFlags_GpuCached = 2,
    DkMemBlockFlags_CpuUncached = 4,
    DkMemBlockFlags_Image = 8,
    DkMemBlockFlags_Code = 16,
};
void dkMemBlockDestroy(DkMemBlock block);
#define DK_MEMBLOCK_ALIGNMENT 4096U
typedef struct {
    DkDevice device;
    uint32_t size, flags;
    void *storage;
} DkMemBlockMaker;
static inline void dkMemBlockMakerDefaults(DkMemBlockMaker *maker, DkDevice device, uint32_t size)
{
    maker->device = device;
    maker->size = size;
    maker->flags = 0;
    maker->storage = NULL;
}
DkMemBlock dkMemBlockCreate(const DkMemBlockMaker *maker);
void *dkMemBlockGetCpuAddr(DkMemBlock block);
#endif
