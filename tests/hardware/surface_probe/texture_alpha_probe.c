/* Owned uniform alpha blocks; uses existing tracked sampling programs. */
#include "draw_probe.h"
#include <string.h>

typedef struct {
    SceGxmTextureFormat format;
    unsigned kind, alpha, selector;
    const char *tag;
} AlphaCase;
/* kind: RGBA8=0, BC1=1, BC2=2, BC3 eight-alpha=3, BC3 six-alpha=4.
 * Expected alphas are explicit codec arithmetic, independent of rendered data. */
static const AlphaCase cases[]={
    {SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR,0,0,0,"rgba8_zero"},
    {SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR,0,128,0,"rgba8_partial"},
    {SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR,0,255,0,"rgba8_full"},
    {SCE_GXM_TEXTURE_FORMAT_UBC1_ABGR,1,255,0,"bc1_opaque"},
    {SCE_GXM_TEXTURE_FORMAT_UBC1_ABGR,1,0,3,"bc1_transparent"},
    {SCE_GXM_TEXTURE_FORMAT_UBC1_1BGR,1,255,3,"bc1_1bgr_opaque"},
    {SCE_GXM_TEXTURE_FORMAT_UBC2_ABGR,2,0,0,"bc2_zero"},
    {SCE_GXM_TEXTURE_FORMAT_UBC2_ABGR,2,136,0,"bc2_partial"},
    {SCE_GXM_TEXTURE_FORMAT_UBC2_ABGR,2,255,0,"bc2_full"},
    {SCE_GXM_TEXTURE_FORMAT_UBC3_ABGR,3,176,3,"bc3_eight_interpolated"},
    {SCE_GXM_TEXTURE_FORMAT_UBC3_ABGR,4,153,4,"bc3_six_interpolated"},
    {SCE_GXM_TEXTURE_FORMAT_UBC3_ABGR,4,0,6,"bc3_six_zero"},
    {SCE_GXM_TEXTURE_FORMAT_UBC3_ABGR,4,255,7,"bc3_six_full"}
};
static void fill(unsigned char *data,const AlphaCase *test)
{
    memset(data,0,4096);
    if(!test->kind) {
        uint32_t value=(test->alpha<<24)|255;
        for(unsigned i=0;i<16*16;++i) memcpy(data+i*4,&value,4);
        return;
    }
    unsigned bytes=test->kind==1 ? 8 : 16;
    for(unsigned i=0;i<16;++i) {
        unsigned char *block=data+i*bytes;
        unsigned color=bytes==8 ? 0 : 8;
        if(test->kind==1 && test->selector==3) {
            block[color+2]=0x00;block[color+3]=0xf8;
            memset(block+color+4,0xff,4); /* color0<=color1, selector3. */
        } else { block[color]=0x00;block[color+1]=0xf8; }
        if(test->kind==2) memset(block,(test->alpha/17)*17,8);
        if(test->kind>=3) {
            block[0]=test->kind==3 ? 240 : 0;
            block[1]=test->kind==3 ? 16 : 255;
            for(unsigned pixel=0;pixel<16;++pixel) {
                unsigned bit=pixel*3,shift=bit&7,index=2+bit/8;
                unsigned bits=test->selector<<shift;
                block[index]|=(unsigned char)bits;
                if(shift>5) block[index+1]|=(unsigned char)(bits>>8);
            }
        }
    }
}
static uint32_t expected_pixel(const AlphaCase *test,int blend)
{
    unsigned a=test->alpha;
    unsigned r=test->kind==1 && test->selector==3 ? 0 : 255;
    if(!blend) return (a<<24)|r;
    /* Straight RGB: Cs*As + Cd*(1-As); alpha: As + Ad*(1-As).
     * Background ABGR=0x80604020; nearest integer UNORM8 conversion. */
    unsigned red=(r*a+32*(255-a)+127)/255;
    unsigned green=(64*(255-a)+127)/255,blue=(96*(255-a)+127)/255;
    unsigned alpha=a+(128*(255-a)+127)/255;
    return (alpha<<24)|(blue<<16)|(green<<8)|red;
}
int probe_texture_alpha(SceGxmContext *context,SceGxmRenderTarget *target,
    SceGxmColorSurface *surface,uint32_t *pixels,SceGxmVertexProgram *vertex,
    SceGxmFragmentProgram *fragment,SceGxmShaderPatcher *patcher,
    SceGxmShaderPatcherId fragment_id,const SceGxmProgram *vertex_source,
    const SceGxmProgramParameter *matrix,const SceGxmProgramParameter *sampler,
    float vertices[][8],uint16_t *indices,const ProbeHooks *hooks)
{
    unsigned char *data=hooks->allocate(4096);
    if(!data) { hooks->record("FAIL texture_alpha_allocation\n");return -1; }
    SceGxmFragmentProgram *straight=NULL;
    SceGxmBlendInfo blend={0};
    blend.colorMask=SCE_GXM_COLOR_MASK_ALL;
    blend.colorFunc=blend.alphaFunc=SCE_GXM_BLEND_FUNC_ADD;
    blend.colorSrc=SCE_GXM_BLEND_FACTOR_SRC_ALPHA;
    blend.alphaSrc=SCE_GXM_BLEND_FACTOR_ONE;
    blend.colorDst=blend.alphaDst=SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    int ret=-1,failures=0,scene_active=0;
#define CHECK(call) do { int error=(call); if(error) { hooks->record("FAIL texture_alpha_api\n"); \
    hooks->result(#call,error);goto done; } } while(0)
    CHECK(sceGxmShaderPatcherCreateFragmentProgram(patcher,fragment_id,
        SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,SCE_GXM_MULTISAMPLE_NONE,&blend,vertex_source,&straight));
    sceGxmSetVertexProgram(context,vertex);
    sceGxmSetCullMode(context,SCE_GXM_CULL_NONE);
    sceGxmSetTwoSidedEnable(context,SCE_GXM_TWO_SIDED_DISABLED);
    sceGxmSetFrontFragmentProgramEnable(context,SCE_GXM_FRAGMENT_PROGRAM_ENABLED);
    for(unsigned c=0;c<sizeof(cases)/sizeof(cases[0]);++c) {
        const AlphaCase *test=&cases[c];fill(data,test);
        SceGxmTexture texture;
        CHECK(sceGxmTextureInitLinear(&texture,data,test->format,16,16,1));
        CHECK(sceGxmTextureSetMinFilter(&texture,SCE_GXM_TEXTURE_FILTER_POINT));
        CHECK(sceGxmTextureSetMagFilter(&texture,SCE_GXM_TEXTURE_FILTER_POINT));
        CHECK(sceGxmTextureSetMipFilter(&texture,SCE_GXM_TEXTURE_MIP_FILTER_DISABLED));
        CHECK(sceGxmTextureSetUAddrMode(&texture,SCE_GXM_TEXTURE_ADDR_CLAMP));
        CHECK(sceGxmTextureSetVAddrMode(&texture,SCE_GXM_TEXTURE_ADDR_CLAMP));
        for(unsigned pass=0;pass<2;++pass) {
            hooks->record("RUN texture_alpha_");hooks->record(test->tag);
            hooks->record(pass ? "_blend\n" : "_raw\n");
            for(unsigned i=0;i<64*64;++i) pixels[i]=UINT32_C(0x80604020);
            sceGxmSetFragmentProgram(context,pass ? straight : fragment);
            CHECK(sceGxmBeginScene(context,0,target,NULL,NULL,NULL,surface,NULL));
            scene_active=1;
            sceGxmSetViewport(context,32,32,32,-32,0.5f,0.5f);
            sceGxmSetRegionClip(context,SCE_GXM_REGION_CLIP_NONE,0,0,63,63);
            CHECK(sceGxmSetVertexStream(context,0,vertices));
            CHECK(sceGxmSetFragmentTexture(context,sceGxmProgramParameterGetResourceIndex(sampler),&texture));
            void *uniform;
            CHECK(sceGxmReserveVertexDefaultUniformBuffer(context,&uniform));
            CHECK(sceGxmSetUniformDataF(uniform,matrix,0,16,identity));
            CHECK(sceGxmDraw(context,SCE_GXM_PRIMITIVE_TRIANGLES,SCE_GXM_INDEX_FORMAT_U16,indices,3));
            CHECK(sceGxmEndScene(context,NULL,NULL));scene_active=0;sceGxmFinish(context);
            uint32_t expected=expected_pixel(test,pass);int mismatch=0;
            for(unsigned y=16;y<=48;y+=16) for(unsigned x=16;x<=48;x+=16) {
                uint32_t actual=pixels[y*64+x];
                for(unsigned channel=0;channel<4;++channel) {
                    int delta=(int)((actual>>(channel*8))&255)-(int)((expected>>(channel*8))&255);
                    if(delta < -(int)pass || delta > (int)pass) {
                        hooks->result("texture_alpha_sample",y*64+x);
                        hooks->result("actual",actual);hooks->result("expected",expected);
                        mismatch=failures=1;break;
                    }
                }
            }
            hooks->record(mismatch ? "FAIL texture_alpha_" : "PASS texture_alpha_");
            hooks->record(test->tag);hooks->record(pass ? "_blend\n" : "_raw\n");
        }
    }
    ret=failures ? -1 : 0;
done:
    if(scene_active) {
        int error=sceGxmEndScene(context,NULL,NULL);
        if(error) hooks->result("FAIL texture_alpha_end_cleanup",error);
        sceGxmFinish(context);
    }
    sceGxmSetFragmentProgram(context,fragment);
    if(straight) {
        int error=sceGxmShaderPatcherReleaseFragmentProgram(patcher,straight);
        if(error) { hooks->result("FAIL texture_alpha_release",error);ret=-1; }
    }
    return ret;
#undef CHECK
}
