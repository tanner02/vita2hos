/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "draw_probe.h"
#include <string.h>

typedef struct {
    SceGxmAttributeFormat format;
    unsigned bytes;
    uint32_t low, high;
    const char *pass;
} FetchCase;

/* Numeric integer inputs must convert to float, normalized inputs must scale,
 * and negative signed inputs must remain negative. No shader metadata changes
 * are needed: these are real inputs to the pinned texture vertex program. */
static const FetchCase cases[] = {
    {SCE_GXM_ATTRIBUTE_FORMAT_U8,   1, 0, 1, "PASS vertex_fetch_u8\n"},
    {SCE_GXM_ATTRIBUTE_FORMAT_S8,   1, 0xff, 1, "PASS vertex_fetch_s8\n"},
    {SCE_GXM_ATTRIBUTE_FORMAT_U16,  2, 0, 1, "PASS vertex_fetch_u16\n"},
    {SCE_GXM_ATTRIBUTE_FORMAT_S16,  2, 0xffff, 1, "PASS vertex_fetch_s16\n"},
    {SCE_GXM_ATTRIBUTE_FORMAT_U8N,  1, 0, 255, "PASS vertex_fetch_u8n\n"},
    {SCE_GXM_ATTRIBUTE_FORMAT_S8N,  1, 0x81, 127, "PASS vertex_fetch_s8n\n"},
    {SCE_GXM_ATTRIBUTE_FORMAT_U16N, 2, 0, 65535, "PASS vertex_fetch_u16n\n"},
    {SCE_GXM_ATTRIBUTE_FORMAT_S16N, 2, 0x8001, 32767, "PASS vertex_fetch_s16n\n"},
    {SCE_GXM_ATTRIBUTE_FORMAT_F16,  2, 0xbc00, 0x3c00, "PASS vertex_fetch_f16\n"},
    {SCE_GXM_ATTRIBUTE_FORMAT_F32,  4, 0xbf800000, 0x3f800000, "PASS vertex_fetch_f32\n"},
};

int probe_vertex_fetch(SceGxmContext *context, SceGxmRenderTarget *target,
    SceGxmColorSurface *surface, uint32_t *pixels, SceGxmShaderPatcher *patcher,
    SceGxmShaderPatcherId vertex_id, SceGxmFragmentProgram *fragment,
    const SceGxmProgramParameter *position, const SceGxmProgramParameter *uv,
    const SceGxmProgramParameter *matrix, const SceGxmProgramParameter *sampler,
    const ProbeHooks *hooks)
{
    float *positions = hooks->allocate(4 * 4 * sizeof(float));
    unsigned char *coords = hooks->allocate(4 * 16);
    uint16_t *indices = hooks->allocate(6 * sizeof(uint16_t));
    uint32_t *texels = hooks->allocate(16 * 16 * 4);
    if (!positions || !coords || !indices || !texels) return -1;
    const uint16_t quad[6] = {0,1,2, 2,1,3};
    memcpy(indices, quad, sizeof(quad));
    for (unsigned i=0;i<4;++i) {
        positions[i*4] = (i&1) ? 1.0f : -1.0f;
        positions[i*4+1] = (i&2) ? 1.0f : -1.0f;
        positions[i*4+2] = 0.5f; positions[i*4+3] = 1.0f;
    }
    SceGxmVertexAttribute attributes[2] = {0};
    attributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
    attributes[0].componentCount = 4;
    attributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(position);
    attributes[1].componentCount = 2; attributes[1].streamIndex = 1;
    attributes[1].offset = 4;
    attributes[1].regIndex = sceGxmProgramParameterGetResourceIndex(uv);
    SceGxmVertexStream streams[2] = {0};
    streams[0].stride = streams[1].stride = 16;
    streams[0].indexSource = streams[1].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
    const uint32_t palette[4] = {0xff0000ff,0xff00ff00,0xffff0000,0xffffffff};
    const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    SceGxmVertexProgram *vertex = NULL;
    SceGxmTexture texture;
    int ret;
#define CHECK(call) do { ret=(call); if(ret) { hooks->record("FAIL " #call "\n"); goto done; } } while(0)
    CHECK(sceGxmTextureInitLinear(&texture,texels,SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR,16,16,1));
    CHECK(sceGxmTextureSetMinFilter(&texture,SCE_GXM_TEXTURE_FILTER_POINT));
    CHECK(sceGxmTextureSetMagFilter(&texture,SCE_GXM_TEXTURE_FILTER_POINT));
    CHECK(sceGxmTextureSetUAddrMode(&texture,SCE_GXM_TEXTURE_ADDR_CLAMP));
    CHECK(sceGxmTextureSetVAddrMode(&texture,SCE_GXM_TEXTURE_ADDR_CLAMP));
    for (unsigned c=0;c<sizeof(cases)/sizeof(cases[0]);++c) {
        const FetchCase *test=&cases[c];
        hooks->result("vertex_fetch_case",c);
        memset(coords,0xa5,4*16);
        for (unsigned i=0;i<4;++i) for (unsigned axis=0;axis<2;++axis) {
            uint32_t bits=(i&(1U<<axis)) ? test->high : test->low;
            /* Explicit little-endian storage, with poison before/after each
             * tightly packed vec2 and a separate position stream. */
            for (unsigned b=0;b<test->bytes;++b)
                coords[i*16+4+axis*test->bytes+b]=(unsigned char)(bits>>(8*b));
        }
        for (unsigned y=0;y<16;++y) for (unsigned x=0;x<16;++x)
            texels[y*16+x]=palette[((y>=8)*2+(x>=8)+c)&3];
        for (unsigned i=0;i<64*64;++i) pixels[i]=0xff332211;
        attributes[1].format=test->format;
        CHECK(sceGxmShaderPatcherCreateVertexProgram(patcher,vertex_id,attributes,2,streams,2,&vertex));
        sceGxmSetVertexProgram(context,vertex); sceGxmSetFragmentProgram(context,fragment);
        CHECK(sceGxmBeginScene(context,0,target,NULL,NULL,NULL,surface,NULL));
        sceGxmSetViewport(context,32,32,32,-32,0.5f,0.5f);
        CHECK(sceGxmSetVertexStream(context,0,positions));
        CHECK(sceGxmSetVertexStream(context,1,coords));
        CHECK(sceGxmSetFragmentTexture(context,sceGxmProgramParameterGetResourceIndex(sampler),&texture));
        void *uniform;
        CHECK(sceGxmReserveVertexDefaultUniformBuffer(context,&uniform));
        CHECK(sceGxmSetUniformDataF(uniform,matrix,0,16,identity));
        CHECK(sceGxmDraw(context,SCE_GXM_PRIMITIVE_TRIANGLES,SCE_GXM_INDEX_FORMAT_U16,indices,6));
        CHECK(sceGxmEndScene(context,NULL,NULL)); sceGxmFinish(context);
        for (unsigned y=0;y<2;++y) for (unsigned x=0;x<2;++x) {
            unsigned index=(8+y*48)*64+8+x*48;
            uint32_t expected=palette[((1-y)*2+x+c)&3];
            if (pixels[index]!=expected) {
                hooks->result("FAIL vertex_fetch_pixel",index);
                hooks->result("actual",pixels[index]);hooks->result("expected",expected);
                ret=-1;goto done;
            }
        }
        CHECK(sceGxmShaderPatcherReleaseVertexProgram(patcher,vertex));vertex=NULL;
        hooks->record(test->pass);
    }
    ret=0;
done:
    if(vertex) { sceGxmFinish(context);sceGxmShaderPatcherReleaseVertexProgram(patcher,vertex); }
    return ret;
#undef CHECK
}
