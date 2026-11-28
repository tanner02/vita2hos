/* Owned diagnostic pixels and compressed blocks; no embedded shader fixtures. */
#include "draw_probe.h"
#include <string.h>

typedef struct {
    SceGxmTextureFormat format;
    unsigned height, base_bytes, mip_bytes, block_bytes;
    const char *tag;
} MipCase;

static const MipCase cases[] = {
    {SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR, 8, 2048, 512, 0, "texture_mip_rgba8"},
    {SCE_GXM_TEXTURE_FORMAT_UBC1_ABGR, 32, 1024, 256, 8, "texture_mip_bc1"},
    {SCE_GXM_TEXTURE_FORMAT_UBC2_ABGR, 32, 2048, 512, 16, "texture_mip_bc2"},
    {SCE_GXM_TEXTURE_FORMAT_UBC3_ABGR, 32, 2048, 512, 16, "texture_mip_bc3"}
};

static void fill_level(unsigned char *destination, unsigned bytes,
                       unsigned block_bytes, unsigned kind, int green)
{
    if (!block_bytes) {
        const uint32_t color = green ? UINT32_C(0xff00ff00) : UINT32_C(0xff0000ff);
        for (unsigned offset = 0; offset < bytes; offset += 4)
            memcpy(destination + offset, &color, 4);
        return;
    }
    for (unsigned offset = 0; offset < bytes; offset += block_bytes) {
        unsigned char *block = destination + offset;
        memset(block, 0, block_bytes);
        unsigned color_offset = block_bytes == 8 ? 0 : 8;
        if (kind == 2) memset(block, 255, 8); /* BC2: opaque explicit alpha. */
        if (kind == 3) block[0] = 255; /* BC3: alpha0=255, alpha1/selectors=0. */
        block[color_offset] = green ? 0xe0 : 0x00;
        block[color_offset+1] = green ? 0x07 : 0xf8;
        /* color1=0 and selectors=0 choose the opaque red/green endpoint. */
    }
}

int probe_texture_mips(SceGxmContext *context, SceGxmRenderTarget *target,
    SceGxmColorSurface *surface, uint32_t *pixels, SceGxmVertexProgram *vertex,
    SceGxmFragmentProgram *fragment, const SceGxmProgramParameter *matrix,
    const SceGxmProgramParameter *sampler, float vertices[][8], uint16_t *indices,
    const ProbeHooks *hooks)
{
    unsigned char *data = hooks->allocate(4096);
    if (!data) { hooks->record("FAIL texture_mip_allocation\n"); return -1; }
    const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    for (unsigned i = 0; i < 3; ++i) {
        vertices[i][0] = i == 1 ? 3.0f : -1.0f;
        vertices[i][1] = i == 2 ? 3.0f : -1.0f;
        vertices[i][2] = 0.5f; vertices[i][3] = 1.0f;
        vertices[i][4] = i == 1 ? 2.0f : 0.0f;
        vertices[i][5] = i == 2 ? 2.0f : 0.0f;
        vertices[i][6] = vertices[i][7] = 0.0f;
        indices[i] = (uint16_t)i;
    }
    sceGxmSetVertexProgram(context, vertex); sceGxmSetFragmentProgram(context, fragment);
    sceGxmSetCullMode(context, SCE_GXM_CULL_NONE);
    sceGxmSetTwoSidedEnable(context, SCE_GXM_TWO_SIDED_DISABLED);
    sceGxmSetFrontFragmentProgramEnable(context, SCE_GXM_FRAGMENT_PROGRAM_ENABLED);
    int failures = 0;
    for (unsigned c = 0; c < sizeof(cases)/sizeof(cases[0]); ++c) {
        const MipCase *test = &cases[c];
        hooks->record("RUN "); hooks->record(test->tag); hooks->record("\n");
        fill_level(data, test->base_bytes, test->block_bytes, c, 0);
        fill_level(data + test->base_bytes, test->mip_bytes, test->block_bytes, c, 1);
        for (unsigned i = 0; i < 64*64; ++i) pixels[i] = UINT32_C(0xff332211);
        SceGxmTexture texture;
#define CHECK(call) do { int error = (call); if (error) { \
    hooks->record("FAIL "); hooks->record(test->tag); hooks->record("\n"); \
    hooks->result(#call, error); return -1; } } while (0)
        CHECK(sceGxmTextureInitLinear(&texture, data, test->format, 64, test->height, 2));
        CHECK(sceGxmTextureSetMinFilter(&texture, SCE_GXM_TEXTURE_FILTER_POINT));
        CHECK(sceGxmTextureSetMagFilter(&texture, SCE_GXM_TEXTURE_FILTER_POINT));
        CHECK(sceGxmTextureSetMipFilter(&texture, SCE_GXM_TEXTURE_MIP_FILTER_DISABLED));
        CHECK(sceGxmTextureSetUAddrMode(&texture, SCE_GXM_TEXTURE_ADDR_CLAMP));
        CHECK(sceGxmTextureSetVAddrMode(&texture, SCE_GXM_TEXTURE_ADDR_CLAMP));
        /* Width and viewport are both 64: maximum derivative gives LOD 0.
         * Texture initialization supplies zero LOD bias (encoded as 31). */
        CHECK(sceGxmBeginScene(context, 0, target, NULL, NULL, NULL, surface, NULL));
        sceGxmSetViewport(context, 32, 32, 32, -32, 0.5f, 0.5f);
        sceGxmSetRegionClip(context,SCE_GXM_REGION_CLIP_NONE,0,0,63,63);
        CHECK(sceGxmSetVertexStream(context, 0, vertices));
        CHECK(sceGxmSetFragmentTexture(context, sceGxmProgramParameterGetResourceIndex(sampler), &texture));
        void *uniform;
        CHECK(sceGxmReserveVertexDefaultUniformBuffer(context, &uniform));
        CHECK(sceGxmSetUniformDataF(uniform, matrix, 0, 16, identity));
        CHECK(sceGxmDraw(context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, indices, 3));
        CHECK(sceGxmEndScene(context, NULL, NULL)); sceGxmFinish(context);
        int mismatch = 0;
        for (unsigned y = 16; y <= 48; y += 16) {
            for (unsigned x = 16; x <= 48; x += 16) {
                unsigned index = y*64+x;
                if (pixels[index] != UINT32_C(0xff0000ff)) {
                    hooks->result("texture_mip_sample", index);
                    hooks->result("actual", pixels[index]);
                    hooks->result("expected", UINT32_C(0xff0000ff));
                    mismatch = failures = 1;
                }
            }
        }
        hooks->record(mismatch ? "FAIL " : "PASS ");
        hooks->record(test->tag); hooks->record("\n");
#undef CHECK
    }
    return failures ? -1 : 0;
}
