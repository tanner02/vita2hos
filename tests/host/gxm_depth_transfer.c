#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../source/gxm_depth_transfer.c"

static void roundtrip(SceGxmDepthStencilFormat format)
{
    GxmDepthTransfer p;
    assert(!gxm_depth_transfer_plan(&p,format,SCE_GXM_DEPTH_STENCIL_SURFACE_LINEAR,3,2,32,8));
    assert(p.sample_count==11 && p.depth_bytes==(p.depth_bytes_per_pixel ? 140u : 0u));
    assert(p.stencil_bytes==(p.has_stencil ? 35u : 0u));
    unsigned char depth[256],stencil[64],saved_depth[256],saved_stencil[64];
    memset(depth,0xa5,sizeof(depth));memset(stencil,0x5a,sizeof(stencil));
    const uint32_t values[]={0,0x80000000,0x7fa12345,0x7fc12345,0x7f800000,0xff800000};
    for (unsigned y=0;y<2;++y) for (unsigned x=0;x<3;++x) {
        memcpy(depth+(y*32+x)*4,&values[y*3+x],4);stencil[y*32+x]=(unsigned char)(y*3+x+10);
    }
    memcpy(saved_depth,depth,sizeof(depth));memcpy(saved_stencil,stencil,sizeof(stencil));
    GxmDepthSample samples[16],unchanged[16];memset(samples,0xcc,sizeof(samples));
    assert(!gxm_depth_transfer_read(&p,depth,p.depth_bytes,stencil,p.stencil_bytes,
        0x3f000000,77,samples,11));
    for (unsigned y=0;y<2;++y) for (unsigned x=0;x<3;++x) {
        assert(samples[y*8+x].depth_bits==(p.depth_bytes_per_pixel ? values[y*3+x] : 0x3f000000));
        assert(samples[y*8+x].stencil==(p.has_stencil ? y*3+x+10 : 77));
    }
    for (unsigned i=3;i<8;++i) assert(samples[i].depth_bits==0xcccccccc && samples[i].stencil==0xcccccccc);
    assert(!gxm_depth_transfer_write(&p,samples,11,depth,p.depth_bytes,stencil,p.stencil_bytes));
    assert(!memcmp(saved_depth,depth,sizeof(depth)) && !memcmp(saved_stencil,stencil,sizeof(stencil)));
    if (p.has_stencil) {
        samples[0].stencil=0xabcdef12;
        assert(!gxm_depth_transfer_write(&p,samples,11,depth,p.depth_bytes,stencil,p.stencil_bytes));
        assert(stencil[0]==0x12);memcpy(saved_stencil,stencil,sizeof(stencil));
    }
    memcpy(unchanged,samples,sizeof(samples));
    assert(gxm_depth_transfer_read(&p,depth,p.depth_bytes,stencil,p.stencil_bytes,0,0,samples,10)<0);
    assert(!memcmp(samples,unchanged,sizeof(samples)));
    if (p.depth_bytes_per_pixel) {
        assert(gxm_depth_transfer_read(&p,depth,p.depth_bytes-1,stencil,p.stencil_bytes,0,0,samples,11)<0);
        assert(!memcmp(samples,unchanged,sizeof(samples)));
        samples[0].depth_bits=0xdeadbeef;
        assert(gxm_depth_transfer_write(&p,samples,11,depth,p.depth_bytes-1,stencil,p.stencil_bytes)<0);
        assert(!memcmp(depth,saved_depth,sizeof(depth)));
    }
    if (p.has_stencil) {
        samples[0].depth_bits=0xdeadbeef;samples[0].stencil=0x77;
        assert(gxm_depth_transfer_write(&p,samples,11,depth,p.depth_bytes,stencil,p.stencil_bytes-1)<0);
        assert(!memcmp(depth,saved_depth,sizeof(depth)) && !memcmp(stencil,saved_stencil,sizeof(stencil)));
    }
    assert(!gxm_depth_transfer_read(&p,NULL,0,NULL,0,0x7fc0abcd,201,samples,11));
    for (unsigned y=0;y<2;++y) for (unsigned x=0;x<3;++x)
        assert(samples[y*8+x].depth_bits==0x7fc0abcd && samples[y*8+x].stencil==201);
    assert(!gxm_depth_transfer_write(&p,samples,11,NULL,0,NULL,0));
    GxmDepthTransfer bad=p;bad.depth_bytes++;
    assert(gxm_depth_transfer_read(&bad,depth,sizeof(depth),stencil,sizeof(stencil),0,0,samples,16)<0);
    assert((uint32_t)gxm_depth_transfer_read(&p,depth,p.depth_bytes,stencil,p.stencil_bytes,
        0,0,NULL,11)==SCE_GXM_ERROR_INVALID_POINTER);
}
int main(void)
{
    roundtrip(SCE_GXM_DEPTH_STENCIL_FORMAT_DF32);
    roundtrip(SCE_GXM_DEPTH_STENCIL_FORMAT_S8);
    roundtrip(SCE_GXM_DEPTH_STENCIL_FORMAT_DF32_S8);
    GxmDepthTransfer p,saved;memset(&p,0xa5,sizeof(p));saved=p;
    assert(gxm_depth_transfer_plan(&p,SCE_GXM_DEPTH_STENCIL_FORMAT_D16,0,1,1,32,1)<0);
    assert(!memcmp(&p,&saved,sizeof(p)));
    assert(gxm_depth_transfer_plan(&p,SCE_GXM_DEPTH_STENCIL_FORMAT_DF32,
        SCE_GXM_DEPTH_STENCIL_SURFACE_TILED,1,1,32,1)<0);
    const uint32_t bad[][4]={{0,1,32,1},{4097,1,4128,4097},{1,0,32,1},{1,4097,32,1},
        {33,1,32,33},{1,1,33,1},{1,1,8224,1},{3,1,32,2},{1,2,32,UINT32_MAX}};
    for (unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i) {
        assert(gxm_depth_transfer_plan(&p,SCE_GXM_DEPTH_STENCIL_FORMAT_DF32,0,
            bad[i][0],bad[i][1],bad[i][2],bad[i][3])<0);
        assert(!memcmp(&p,&saved,sizeof(p)));
    }
    assert(!gxm_depth_transfer_plan(&p,SCE_GXM_DEPTH_STENCIL_FORMAT_DF32_S8,0,1,1,32,1));
    GxmDepthSample sample={0x3f800000,12},copy=sample;
    unsigned char buffer[64];memset(buffer,0xa5,sizeof(buffer));unsigned char original[64];memcpy(original,buffer,64);
    assert(gxm_depth_transfer_read(&p,&sample,8,NULL,0,0,0,&sample,1)<0);
    assert(!memcmp(&sample,&copy,sizeof(sample)));
    assert(gxm_depth_transfer_write(&p,&sample,1,buffer,4,buffer,1)<0);
    assert(!memcmp(buffer,original,64));
    assert(gxm_depth_transfer_read(&p,buffer,4,buffer,1,0,0,(void *)(buffer+1),1)<0);
    assert(!memcmp(buffer,original,64));
    assert(gxm_depth_transfer_write(&p,&sample,1,(void *)(UINTPTR_MAX-1),4,NULL,0)<0);
    GxmDepthTransfer before=p;
    assert(gxm_depth_transfer_read(&p,buffer,4,NULL,0,0,0,(void *)&p,1)<0);
    assert(!memcmp(&p,&before,sizeof(p)));
    assert(gxm_depth_transfer_write(&p,&sample,1,&p,4,NULL,0)<0);
    assert(!memcmp(&p,&before,sizeof(p)));
    assert(gxm_depth_transfer_read(NULL,NULL,0,NULL,0,0,0,&sample,1)<0);
    puts("Depth transfer CPU groundwork: exact float bits, separate stencil, padding, backgrounds, checked spans and failure preservation passed.");
}
