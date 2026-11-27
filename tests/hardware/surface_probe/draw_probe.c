/* SPDX-License-Identifier: GPL-2.0-or-later
 * Uses pinned Vita3K shader test fixtures; see COPYING.txt. */
#include "draw_probe.h"
#include "draw_shaders.h"
#include <stddef.h>

static const ProbeHooks *callbacks;
static void *host_allocate(void *user, SceSize size) { (void)user; return callbacks->allocate(size); }
static void host_free(void *user, void *memory) { (void)user; (void)memory; }
static void *usse_allocate(void *user, SceSize size, unsigned *offset)
{ *offset = 0; return host_allocate(user, size); }

static int color(SceGxmContext *context, const SceGxmProgramParameter *parameter,
                  const SceGxmProgramParameter *matrix, const float value[4])
{
    void *uniform;
    const float identity[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    int ret = sceGxmReserveVertexDefaultUniformBuffer(context, &uniform);
    if (ret) return ret;
    ret = sceGxmSetUniformDataF(uniform, matrix, 0, 16, identity);
    if (ret) return ret;
    ret = sceGxmReserveFragmentDefaultUniformBuffer(context, &uniform);
    return ret ? ret : sceGxmSetUniformDataF(uniform, parameter, 0, 4, value);
}

static int check_pixels(const uint32_t *pixels, uint32_t expected)
{
    /* Keep samples far from viewport edges and triangle clipping boundaries. */
    for (unsigned y = 16; y <= 48; y += 16) {
        for (unsigned x = 16; x <= 48; x += 16) {
            if (pixels[y * 64 + x] != expected) {
                callbacks->result("pixel", y * 64 + x);
                callbacks->result("actual", pixels[y * 64 + x]);
                callbacks->result("expected", expected);
                return -1;
            }
        }
    }
    return 0;
}

int probe_draw(SceGxmContext *context, SceGxmRenderTarget *target,
               SceGxmColorSurface *surface, uint32_t *pixels, const ProbeHooks *hooks)
{
    callbacks = hooks;
    /* color_v preserves guest Z under an identity WVP; clear_v forces Z=1. */
    const SceGxmProgram *vertex_source = (const SceGxmProgram *)color_v;
    const SceGxmProgram *fragment_source = (const SceGxmProgram *)clear_f;
    SceGxmShaderPatcher *patcher = NULL;
    SceGxmShaderPatcherId vertex_id = NULL, fragment_id = NULL;
    SceGxmVertexProgram *vertex = NULL;
    SceGxmFragmentProgram *fragment = NULL;
    int ret = -1;
    SceGxmShaderPatcherParams params = {0};
    params.hostAllocCallback = host_allocate; params.hostFreeCallback = host_free;
    params.bufferAllocCallback = host_allocate; params.bufferFreeCallback = host_free;
    params.vertexUsseAllocCallback = usse_allocate; params.vertexUsseFreeCallback = host_free;
    params.fragmentUsseAllocCallback = usse_allocate; params.fragmentUsseFreeCallback = host_free;
    const SceGxmProgramParameter *position = sceGxmProgramFindParameterByName(vertex_source, "aPosition");
    const SceGxmProgramParameter *vertex_color = sceGxmProgramFindParameterByName(vertex_source, "aColor");
    const SceGxmProgramParameter *matrix = sceGxmProgramFindParameterByName(vertex_source, "wvp");
    const SceGxmProgramParameter *clear_color = sceGxmProgramFindParameterByName(fragment_source, "uClearColor");
    if (!position || !vertex_color || !matrix || !clear_color) { hooks->record("FAIL shader_parameters\n"); return -1; }
#define CHECK(expression) do { ret = (expression); if (ret) { hooks->record("FAIL " #expression "\n"); goto done; } } while (0)
    hooks->record("RUN compile_clear_shaders\n");
    CHECK(sceGxmShaderPatcherCreate(&params, &patcher));
    CHECK(sceGxmShaderPatcherRegisterProgram(patcher, vertex_source, &vertex_id));
    CHECK(sceGxmShaderPatcherRegisterProgram(patcher, fragment_source, &fragment_id));
    SceGxmVertexAttribute attributes[2] = {0};
    attributes[0].format = attributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
    attributes[0].componentCount = attributes[1].componentCount = 4;
    attributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(position);
    attributes[1].regIndex = sceGxmProgramParameterGetResourceIndex(vertex_color);
    attributes[1].offset = 4 * sizeof(float);
    SceGxmVertexStream stream = {0};
    stream.stride = 8 * sizeof(float); stream.indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;
    CHECK(sceGxmShaderPatcherCreateVertexProgram(patcher, vertex_id, attributes, 2, &stream, 1, &vertex));
    CHECK(sceGxmShaderPatcherCreateFragmentProgram(patcher, fragment_id, SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
        SCE_GXM_MULTISAMPLE_NONE, NULL, vertex_source, &fragment));
    float (*vertices)[8] = hooks->allocate(9 * 8 * sizeof(float));
    uint16_t *indices = hooks->allocate(3 * sizeof(uint16_t));
    void *depth = hooks->allocate(64 * 64 * 4);
    if (!vertices || !indices || !depth) { hooks->record("FAIL draw_allocation\n"); ret = -1; goto done; }
    for (unsigned set = 0; set < 3; ++set) {
        float z = set == 0 ? 0.75f : set == 1 ? 0.25f : 0.5f;
        for (unsigned i = 0; i < 3; ++i) {
            vertices[set * 3 + i][0] = i == 1 ? 3.0f : -1.0f;
            vertices[set * 3 + i][1] = i == 2 ? 3.0f : -1.0f;
            vertices[set * 3 + i][2] = z; vertices[set * 3 + i][3] = 1.0f;
            for (unsigned c = 4; c < 8; ++c) vertices[set * 3 + i][c] = 0.0f;
        }
    }
    indices[0] = 0; indices[1] = 1; indices[2] = 2;
    SceGxmDepthStencilSurface ds;
    CHECK(sceGxmDepthStencilSurfaceInit(&ds, SCE_GXM_DEPTH_STENCIL_FORMAT_DF32_S8,
        SCE_GXM_DEPTH_STENCIL_SURFACE_LINEAR, 64, depth, NULL));
    sceGxmDepthStencilSurfaceSetBackgroundDepth(&ds, 1.0f);
    sceGxmDepthStencilSurfaceSetBackgroundStencil(&ds, 0);
    const float red[4] = {1, 0, 0, 1}, green[4] = {0, 1, 0, 1};
    sceGxmSetVertexProgram(context, vertex); sceGxmSetFragmentProgram(context, fragment);
    sceGxmSetCullMode(context, SCE_GXM_CULL_NONE);
    sceGxmSetTwoSidedEnable(context, SCE_GXM_TWO_SIDED_DISABLED);
    sceGxmSetFrontDepthWriteEnable(context, SCE_GXM_DEPTH_WRITE_ENABLED);
    sceGxmSetFrontDepthFunc(context, SCE_GXM_DEPTH_FUNC_LESS);
    sceGxmSetFrontStencilFunc(context, SCE_GXM_STENCIL_FUNC_ALWAYS, SCE_GXM_STENCIL_OP_KEEP,
        SCE_GXM_STENCIL_OP_KEEP, SCE_GXM_STENCIL_OP_KEEP, 0xff, 0);
    CHECK(sceGxmBeginScene(context, 0, target, NULL, NULL, NULL, surface, &ds));
    CHECK(sceGxmSetVertexStream(context, 0, vertices));
    CHECK(color(context, clear_color, matrix, red));
    CHECK(sceGxmDraw(context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, indices, 3));
    CHECK(sceGxmEndScene(context, NULL, NULL)); sceGxmFinish(context);
    CHECK(check_pixels(pixels, 0xff0000ff)); hooks->record("PASS shader_red\n");

    CHECK(sceGxmBeginScene(context, 0, target, NULL, NULL, NULL, surface, &ds));
    CHECK(sceGxmSetVertexStream(context, 0, vertices)); CHECK(color(context, clear_color, matrix, red));
    CHECK(sceGxmDraw(context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, indices, 3));
    sceGxmSetFrontFragmentProgramEnable(context, SCE_GXM_FRAGMENT_PROGRAM_DISABLED);
    CHECK(sceGxmSetVertexStream(context, 0, vertices + 3)); CHECK(color(context, clear_color, matrix, green));
    CHECK(sceGxmDraw(context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, indices, 3));
    sceGxmSetFrontFragmentProgramEnable(context, SCE_GXM_FRAGMENT_PROGRAM_ENABLED);
    CHECK(sceGxmSetVertexStream(context, 0, vertices + 6)); CHECK(color(context, clear_color, matrix, green));
    CHECK(sceGxmDraw(context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, indices, 3));
    CHECK(sceGxmEndScene(context, NULL, NULL)); sceGxmFinish(context);
    CHECK(check_pixels(pixels, 0xff0000ff)); hooks->record("PASS depth_only_occlusion\n");

    sceGxmSetFrontDepthFunc(context, SCE_GXM_DEPTH_FUNC_ALWAYS);
    sceGxmSetFrontDepthWriteEnable(context, SCE_GXM_DEPTH_WRITE_DISABLED);
    CHECK(sceGxmBeginScene(context, 0, target, NULL, NULL, NULL, surface, &ds));
    sceGxmSetFrontFragmentProgramEnable(context, SCE_GXM_FRAGMENT_PROGRAM_DISABLED);
    sceGxmSetFrontStencilRef(context, 1);
    sceGxmSetFrontStencilFunc(context, SCE_GXM_STENCIL_FUNC_ALWAYS, SCE_GXM_STENCIL_OP_KEEP,
        SCE_GXM_STENCIL_OP_KEEP, SCE_GXM_STENCIL_OP_REPLACE, 0xff, 0xff);
    CHECK(sceGxmSetVertexStream(context, 0, vertices)); CHECK(color(context, clear_color, matrix, green));
    CHECK(sceGxmDraw(context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, indices, 3));
    sceGxmSetFrontFragmentProgramEnable(context, SCE_GXM_FRAGMENT_PROGRAM_ENABLED);
    sceGxmSetFrontStencilFunc(context, SCE_GXM_STENCIL_FUNC_EQUAL, SCE_GXM_STENCIL_OP_KEEP,
        SCE_GXM_STENCIL_OP_KEEP, SCE_GXM_STENCIL_OP_KEEP, 0xff, 0);
    CHECK(color(context, clear_color, matrix, green));
    CHECK(sceGxmDraw(context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, indices, 3));
    CHECK(sceGxmEndScene(context, NULL, NULL)); sceGxmFinish(context);
    CHECK(check_pixels(pixels, 0xff00ff00)); hooks->record("PASS stencil_replace\n");
done:
    if (fragment) sceGxmShaderPatcherReleaseFragmentProgram(patcher, fragment);
    if (vertex) sceGxmShaderPatcherReleaseVertexProgram(patcher, vertex);
    if (fragment_id) sceGxmShaderPatcherUnregisterProgram(patcher, fragment_id);
    if (vertex_id) sceGxmShaderPatcherUnregisterProgram(patcher, vertex_id);
    if (patcher) sceGxmShaderPatcherDestroy(patcher);
    return ret;
}
