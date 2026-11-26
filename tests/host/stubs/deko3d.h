#ifndef TEST_DEKO3D_H
#define TEST_DEKO3D_H
typedef void *DkMemBlock;
typedef void *DkDevice;
enum {
    DkMemBlockFlags_CpuCached = 1,
    DkMemBlockFlags_GpuCached = 2,
    DkMemBlockFlags_CpuUncached = 4,
    DkMemBlockFlags_Image = 8,
};
void dkMemBlockDestroy(DkMemBlock block);
#endif
