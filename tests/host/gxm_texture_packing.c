#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../source/gxm_texture.c"
#include "../../source/gxm_texture_layout.c"

/* Independent bit-by-bit Morton oracle: low Y bits occupy even positions,
 * low X bits odd positions; the longer dimension continues above the square. */
static uint32_t oracle_index(uint32_t x,uint32_t y,uint32_t sw,uint32_t sh)
{
    uint32_t result=0,bit=0,minimum=sw<sh?sw:sh;
    while((1U<<bit)<minimum) {
        result|=((y>>bit)&1U)<<(2*bit);
        result|=((x>>bit)&1U)<<(2*bit+1);
        ++bit;
    }
    return result|((x>>bit)<<(2*bit))|((y>>bit)<<(2*bit));
}
static void check(unsigned width,unsigned height,SceGxmTextureFormat format,bool arbitrary)
{
    SceGxmTexture texture;
    int ret=arbitrary?sceGxmTextureInitSwizzledArbitrary(&texture,NULL,format,width,height,1):
        sceGxmTextureInitSwizzled(&texture,NULL,format,width,height,1);
    assert(!ret);
    GxmTextureLayout l;assert(!gxm_texture_get_linear_layout((const SceGxmTextureInner*)&texture,&l));
    unsigned bytes=l.block_bytes?l.block_bytes:l.bytes_per_pixel;
    unsigned columns=l.block_bytes?(width+3)/4:width,rows=l.block_bytes?(height+3)/4:height;
    unsigned char *source=malloc(l.source_size+2),*output=malloc(l.size+2),*expected=malloc(l.size);
    assert(source&&output&&expected);
    for(unsigned i=0;i<l.source_size+2;++i)source[i]=(unsigned char)((i*37U+(i>>8)*19U)^(i>>3));
    memset(output,0xa5,l.size+2);memset(expected,0,l.size);
    for(unsigned y=0;y<rows;++y)for(unsigned x=0;x<columns;++x) {
        unsigned index=oracle_index(x,y,l.source_width,l.source_height);
        assert((uint64_t)(index+1)*bytes<=l.source_size);
        memcpy(expected+y*l.pitch+x*bytes,source+1+index*bytes,bytes);
    }
    assert(!gxm_texture_pack_linear(&l,source+1,l.source_size,output+1,l.size));
    assert(!memcmp(output+1,expected,l.size)&&output[0]==0xa5&&output[l.size+1]==0xa5);
    free(expected);free(output);free(source);
}
int main(void)
{
    const SceGxmTextureFormat formats[]={SCE_GXM_TEXTURE_FORMAT_U8_R111,
        SCE_GXM_TEXTURE_FORMAT_U16_R111,SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR,
        SCE_GXM_TEXTURE_FORMAT_F16F16F16F16_ABGR,SCE_GXM_TEXTURE_FORMAT_UBC1_ABGR,
        SCE_GXM_TEXTURE_FORMAT_UBC2_ABGR,SCE_GXM_TEXTURE_FORMAT_UBC3_ABGR};
    const unsigned dimensions[]={1,2,3,4,5,7,8,9,15,16,17,31,32,33,63,64,65,127};
    unsigned cases=0;
    for(unsigned f=0;f<sizeof(formats)/sizeof(*formats);++f)
        for(unsigned w=0;w<sizeof(dimensions)/sizeof(*dimensions);++w)
            for(unsigned h=0;h<sizeof(dimensions)/sizeof(*dimensions);++h) {
                check(dimensions[w],dimensions[h],formats[f],true);++cases;
            }
    for(unsigned f=0;f<sizeof(formats)/sizeof(*formats);++f)
        for(unsigned w=1;w<=1024;w*=2)for(unsigned h=1;h<=1024;h*=2) {
            check(w,h,formats[f],false);++cases;
        }
    printf("Morton packing: %u independent byte/row-padding/unaligned/guard cases passed.\n",cases);
    return 0;
}
