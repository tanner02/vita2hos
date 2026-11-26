#ifndef TEST_DEKO3D_H
#define TEST_DEKO3D_H
#include <stddef.h>
#include <stdint.h>
typedef void *DkMemBlock;
typedef void *DkDevice;
typedef enum {
    DkImageFormat_R8_Unorm, DkImageFormat_R8_Snorm,
    DkImageFormat_R16_Unorm, DkImageFormat_R16_Snorm, DkImageFormat_R16_Float,
    DkImageFormat_R32_Float, DkImageFormat_RG8_Unorm, DkImageFormat_RG8_Snorm,
    DkImageFormat_RG16_Unorm, DkImageFormat_RG16_Snorm, DkImageFormat_RG16_Float,
    DkImageFormat_RG32_Float, DkImageFormat_RGBA8_Unorm, DkImageFormat_RGBA8_Snorm,
    DkImageFormat_RGBA16_Unorm, DkImageFormat_RGBA16_Snorm, DkImageFormat_RGBA16_Float,
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
