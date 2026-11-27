/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "draw_probe.h"
#include <psp2/display.h>

/* Geometry and texture are generated locally; only the pinned shader fixtures
 * are shared with the offscreen probe. No libc, game assets or math imports. */
int probe_cube(SceGxmContext *context, SceGxmVertexProgram *vertex,
               SceGxmFragmentProgram *fragment, const SceGxmProgramParameter *matrix,
               const SceGxmProgramParameter *sampler, const ProbeHooks *hooks)
{
    enum { WIDTH = 960, HEIGHT = 544, FRAMES = 360 };
    SceGxmRenderTarget *target = NULL;
    SceGxmRenderTargetParams params = {0};
    params.width = WIDTH; params.height = HEIGHT; params.scenesPerFrame = 1;
    int ret = sceGxmCreateRenderTarget(&params, &target);
    if (ret) return ret;
#define CHECK_CUBE(expression) do { ret = (expression); if (ret) { \
    hooks->record("FAIL " #expression "\n"); goto done; } } while (0)
    uint32_t *buffers[2] = {hooks->allocate(WIDTH * HEIGHT * 4), hooks->allocate(WIDTH * HEIGHT * 4)};
    void *depth = hooks->allocate(WIDTH * HEIGHT * 4);
    float (*vertices)[8] = hooks->allocate(24 * 8 * sizeof(float));
    uint16_t *indices = hooks->allocate(36 * sizeof(uint16_t));
    uint32_t *texels = hooks->allocate(16 * 16 * 4);
    if (!buffers[0] || !buffers[1] || !depth || !vertices || !indices || !texels) {
        hooks->record("FAIL cube_allocation\n"); ret = -1; goto done;
    }
    SceGxmColorSurface colors[2];
    for (unsigned i = 0; i < 2; ++i) {
        CHECK_CUBE(sceGxmColorSurfaceInit(&colors[i], SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR,
            SCE_GXM_COLOR_SURFACE_LINEAR, SCE_GXM_COLOR_SURFACE_SCALE_NONE,
            SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT, WIDTH, HEIGHT, WIDTH, buffers[i]));
    }
    SceGxmDepthStencilSurface ds;
    CHECK_CUBE(sceGxmDepthStencilSurfaceInit(&ds, SCE_GXM_DEPTH_STENCIL_FORMAT_DF32_S8,
        SCE_GXM_DEPTH_STENCIL_SURFACE_LINEAR, WIDTH, depth, NULL));
    sceGxmDepthStencilSurfaceSetBackgroundDepth(&ds, 1);
    sceGxmDepthStencilSurfaceSetBackgroundStencil(&ds, 0);
    for (unsigned y = 0; y < 16; ++y) {
        for (unsigned x = 0; x < 16; ++x) {
            const uint32_t palette[4] = {0xff40bfff, 0xffe08030, 0xff80e060, 0xfff0e0d0};
            texels[y * 16 + x] = palette[((x / 4) + (y / 4)) & 3];
        }
    }
    SceGxmTexture texture;
    CHECK_CUBE(sceGxmTextureInitLinear(&texture, texels, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR, 16, 16, 1));
    CHECK_CUBE(sceGxmTextureSetMinFilter(&texture, SCE_GXM_TEXTURE_FILTER_POINT));
    CHECK_CUBE(sceGxmTextureSetMagFilter(&texture, SCE_GXM_TEXTURE_FILTER_POINT));
    CHECK_CUBE(sceGxmTextureSetUAddrMode(&texture, SCE_GXM_TEXTURE_ADDR_CLAMP));
    CHECK_CUBE(sceGxmTextureSetVAddrMode(&texture, SCE_GXM_TEXTURE_ADDR_CLAMP));
    const unsigned char faces[6][4] = {
        {0,1,3,2}, {5,4,6,7}, {4,0,2,6}, {1,5,7,3}, {2,3,7,6}, {4,5,1,0}
    };
    const unsigned char triangles[6] = {0,1,2,0,2,3};
    for (unsigned face = 0; face < 6; ++face) {
        for (unsigned i = 0; i < 6; ++i) indices[face * 6 + i] = face * 4 + triangles[i];
        for (unsigned i = 0; i < 4; ++i) {
            vertices[face * 4 + i][3] = 1;
            vertices[face * 4 + i][4] = i == 1 || i == 2 ? 1 : 0;
            vertices[face * 4 + i][5] = i >= 2 ? 1 : 0;
            vertices[face * 4 + i][6] = vertices[face * 4 + i][7] = 0;
        }
    }
    /* Column-major perspective matrix, with positive camera Z and [0,1] Z.
     * GXM's default depth viewport maps this to [.5,1], preserving depth order. */
    const float projection[16] = {
        1.6f * HEIGHT / WIDTH,0,0,0, 0,1.6f,0,0,
        0,0,20.0f/19.9f,1, 0,0,-2.0f/19.9f,0
    };
    sceGxmSetVertexProgram(context, vertex); sceGxmSetFragmentProgram(context, fragment);
    sceGxmSetCullMode(context, SCE_GXM_CULL_NONE);
    sceGxmSetFrontDepthWriteEnable(context, SCE_GXM_DEPTH_WRITE_ENABLED);
    sceGxmSetFrontDepthFunc(context, SCE_GXM_DEPTH_FUNC_LESS);
    sceGxmSetFrontStencilFunc(context, SCE_GXM_STENCIL_FUNC_ALWAYS, SCE_GXM_STENCIL_OP_KEEP,
        SCE_GXM_STENCIL_OP_KEEP, SCE_GXM_STENCIL_OP_KEEP, 0xff, 0);
    float sine = 0, cosine = 1;
    hooks->record("RUN visible_cube frames=360\n");
    for (unsigned frame = 0; frame < FRAMES; ++frame) {
        unsigned selected = frame & 1;
        for (unsigned i = 0; i < WIDTH * HEIGHT; ++i) buffers[selected][i] = 0xff302018;
        for (unsigned face = 0; face < 6; ++face) {
            for (unsigned i = 0; i < 4; ++i) {
                unsigned corner = faces[face][i];
                float x = corner & 1 ? 1 : -1;
                float y = corner & 2 ? 1 : -1;
                float z = corner & 4 ? 1 : -1;
                float rx = cosine * x + sine * z, rz = -sine * x + cosine * z;
                vertices[face * 4 + i][0] = rx;
                vertices[face * 4 + i][1] = 0.8253356f * y - 0.5646425f * rz;
                vertices[face * 4 + i][2] = 0.5646425f * y + 0.8253356f * rz + 4.5f;
            }
        }
        CHECK_CUBE(sceGxmBeginScene(context, 0, target, NULL, NULL, NULL, &colors[selected], &ds));
        CHECK_CUBE(sceGxmSetVertexStream(context, 0, vertices));
        CHECK_CUBE(sceGxmSetFragmentTexture(context, sceGxmProgramParameterGetResourceIndex(sampler), &texture));
        void *uniform;
        CHECK_CUBE(sceGxmReserveVertexDefaultUniformBuffer(context, &uniform));
        CHECK_CUBE(sceGxmSetUniformDataF(uniform, matrix, 0, 16, projection));
        CHECK_CUBE(sceGxmDraw(context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, indices, 36));
        CHECK_CUBE(sceGxmEndScene(context, NULL, NULL)); sceGxmFinish(context);
        SceDisplayFrameBuf display = {0};
        display.size = sizeof(display); display.base = buffers[selected]; display.pitch = WIDTH;
        display.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
        display.width = WIDTH; display.height = HEIGHT;
        CHECK_CUBE(sceDisplaySetFrameBuf(&display, SCE_DISPLAY_SETBUF_NEXTFRAME));
        /* Wait for presentation and retirement before reusing the other buffer. */
        CHECK_CUBE(sceDisplayWaitVblankStart()); CHECK_CUBE(sceDisplayWaitVblankStart());
        float next_sine = sine * 0.9998000f + cosine * 0.0199987f;
        cosine = cosine * 0.9998000f - sine * 0.0199987f; sine = next_sine;
        if ((frame + 1) % 120 == 0) hooks->result("cube_frames", frame + 1);
    }
    hooks->record("PASS cube_frames\n");
    for (unsigned i = 0; i < 120; ++i) CHECK_CUBE(sceDisplayWaitVblankStart());
done:
    if (target) sceGxmDestroyRenderTarget(target);
    /* The guest's display allocations remain live until the runtime stops its
     * presenter and the process exits. The current runtime cannot detach a
     * framebuffer via SetFrameBuf(NULL); freeing it here would race presentation. */
    return ret;
}
