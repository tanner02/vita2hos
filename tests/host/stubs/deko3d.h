#ifndef TEST_DEKO3D_H
#define TEST_DEKO3D_H
#include <stddef.h>
#include <stdint.h>
typedef enum {
    DkVtxAttribSize_1x32=0x12, DkVtxAttribSize_2x32=0x04,
    DkVtxAttribSize_3x32=0x02, DkVtxAttribSize_4x32=0x01,
    DkVtxAttribSize_1x16=0x1b, DkVtxAttribSize_2x16=0x0f,
    DkVtxAttribSize_3x16=0x05, DkVtxAttribSize_4x16=0x03,
    DkVtxAttribSize_1x8=0x1d, DkVtxAttribSize_2x8=0x18,
    DkVtxAttribSize_3x8=0x13, DkVtxAttribSize_4x8=0x0a
} DkVtxAttribSize;
typedef enum {
    DkVtxAttribType_None=0, DkVtxAttribType_Snorm=1, DkVtxAttribType_Unorm=2,
    DkVtxAttribType_Sint=3, DkVtxAttribType_Uint=4, DkVtxAttribType_Uscaled=5,
    DkVtxAttribType_Sscaled=6, DkVtxAttribType_Float=7
} DkVtxAttribType;

typedef struct DkVtxAttribState {
    uint32_t bufferId:5, reserved0:1, isFixed:1, offset:14, size:6, type:3, reserved1:1, isBgra:1;
} DkVtxAttribState;

typedef void *DkMemBlock;
typedef void *DkDevice;
typedef enum { DkResult_Success, DkResult_Fail } DkResult;
typedef struct { float x, y, width, height, near, far; } DkViewport;
typedef struct { uint32_t x, y, width, height; } DkScissor;
typedef enum { DkFace_None = 0, DkFace_Front = 1, DkFace_Back = 2, DkFace_FrontAndBack = 3 } DkFace;
typedef enum { DkPolygonMode_Point=0, DkPolygonMode_Line=1, DkPolygonMode_Fill=2 } DkPolygonMode;
typedef struct {
    uint32_t rasterizerEnable:1, depthClampEnable:1, fillRectangleEnable:1;
    uint32_t polygonModeFront:2, polygonModeBack:2, cullMode:2, frontFace:1, provokingVertex:1;
    uint32_t polygonSmoothEnableMask:3, depthBiasEnableMask:3;
} DkRasterizerState;

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
    DkImageFormat_RG_BC5_Unorm, DkImageFormat_RG_BC5_Snorm, DkImageFormat_ZF32_X24S8,
    DkImageFormat_RGBA8_Unorm_sRGB, DkImageFormat_RGBA_BC1_sRGB,
    DkImageFormat_RGBA_BC2_sRGB, DkImageFormat_RGBA_BC3_sRGB,
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
#define DK_SHADER_CODE_UNUSABLE_SIZE 0x400U
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
DkResult dkMemBlockFlushCpuCache(DkMemBlock block, uint32_t offset, uint32_t size);

typedef uint64_t DkGpuAddr;
#define DK_GPU_ADDR_INVALID UINT64_MAX
typedef void *DkCmdBuf;
typedef void *DkQueue;
typedef struct { unsigned unused; } DkImage;
typedef struct { const DkImage *pImage;DkImageFormat format; } DkImageView;
typedef enum { DkMsMode_1x,DkMsMode_2x,DkMsMode_4x,DkMsMode_8x } DkMsMode;
enum { DkImageFlags_UsageRender=1,DkImageFlags_Usage2DEngine=2,DkImageFlags_HwCompression=4 };
typedef enum { DkImageType_2D,DkImageType_2DMS } DkImageType;
typedef struct { DkDevice device;DkImageFormat format;uint32_t flags,dimensions[3];
    DkMsMode msMode;DkImageType type; } DkImageLayoutMaker;
typedef struct { uint64_t size;uint32_t alignment; } DkImageLayout;
static inline void dkImageLayoutMakerDefaults(DkImageLayoutMaker *maker,DkDevice device)
{ *maker=(DkImageLayoutMaker){.device=device}; }
void dkImageLayoutInitialize(DkImageLayout *,const DkImageLayoutMaker *);
uint64_t dkImageLayoutGetSize(const DkImageLayout *);
uint32_t dkImageLayoutGetAlignment(const DkImageLayout *);
void dkImageInitialize(DkImage *,const DkImageLayout *,DkMemBlock,uint32_t);
void dkImageViewDefaults(DkImageView *,const DkImage *);

typedef struct { uint32_t x,y,z,width,height,depth; } DkImageRect;
typedef struct { DkGpuAddr addr;uint32_t rowLength,imageHeight; } DkCopyBuf;
typedef enum { DkBarrier_Full=4 } DkBarrier;
enum { DkInvalidateFlags_Image=1,DkInvalidateFlags_Zcull=8,DkInvalidateFlags_L2Cache=16 };
DkGpuAddr dkMemBlockGetGpuAddr(DkMemBlock);
void dkCmdBufBarrier(DkCmdBuf,DkBarrier,uint32_t);
void dkCmdBufResolveImage(DkCmdBuf,const DkImageView *,const DkImageView *);
void dkCmdBufCopyBufferToImage(DkCmdBuf,const DkCopyBuf *,const DkImageView *,const DkImageRect *,uint32_t);
void dkCmdBufCopyImageToBuffer(DkCmdBuf,const DkImageView *,const DkImageRect *,const DkCopyBuf *,uint32_t);
void dkQueueWaitIdle(DkQueue);
void dkQueueFlush(DkQueue);
#endif
