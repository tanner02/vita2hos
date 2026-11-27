#include "config.h"

#include <deko3d.h>
#include <psp2/gxm.h>
#include <psp2/kernel/error.h>
#include <psp2/kernel/threadmgr.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <switch.h>

#include "modules/SceGxm.h"
#include "circ_buf.h"
#include "deko_utils.h"
#include "log.h"
#include "module.h"
#include "uam_compiler_iface_c.h"
#include "util.h"
#include "vita3k_shader_recompiler_iface_c.h"

#include "gxm/gxm_to_dk.h"
#include "gxm/util.h"
#include "gxm/draw.h"
#include "gxm/viewport.h"
#include "gxm/region_clip.h"
#include "gxm/uniform_staging.h"
#include "gxm/blend.h"
#include "gxm/shader_code.h"
#include "gxm/program_registry.h"
#include "gxm/program_ref.h"
#include "gxm/texture_layout.h"
#include "gxm/texture_staging.h"
#include "modules/SceSysmem.h"

#define DUMP_SHADER_SPIRV     0
#define DUMP_SHADER_GLSL      0
#define ENABLE_SHADER_DUMP_CB 0

typedef struct SceGxmContext {
    SceGxmContextParams params;
    DkMemBlock cmdbuf_memblock;
    DkCmdBuf cmdbuf;
    struct {
        DkMemBlock memblock;
        uint32_t size;
    } vertex_rb, fragment_rb;
    DkMemBlock gxm_vert_unif_block_memblock;
    DkMemBlock gxm_frag_unif_block_memblock;
    GxmTextureStaging *texture_staging;
    GxmUniformStaging *uniform_staging;
    /* Dynamic state */
    struct {
        struct {
            uint32_t head;
        } vertex_rb, fragment_rb;
        const SceGxmVertexProgram *vertex_program;
        const SceGxmFragmentProgram *fragment_program;
        const void *vertex_streams[SCE_GXM_MAX_VERTEX_STREAMS];
        const void *vertex_uniform_buffers[SCE_GXM_MAX_UNIFORM_BUFFERS];
        const void *fragment_uniform_buffers[SCE_GXM_MAX_UNIFORM_BUFFERS];
        const SceGxmRenderTarget *render_target;
        const SceGxmColorSurfaceInner *color_surface;
        const SceGxmDepthStencilSurface *ds_surface;
        SceGxmSyncObject *fragment_sync_object;
        bool in_scene;
        bool two_sided_mode;
        GxmViewportState viewport;
        GxmRegionClip region_clip;
        DkRasterizerState rasterizer;
        DkColorState color;
        DkBlendState blend;
        DkColorWriteState color_write;
        DkDepthStencilState depth_stencil;
        struct {
            uint8_t ref;
            uint8_t compare_mask;
            uint8_t write_mask;
        } front_stencil, back_stencil;
        SceGxmTextureInner fragment_textures[SCE_GXM_MAX_TEXTURE_UNITS];
        dk_surface_t background_ds;
        struct {
            void *cpu_addr;
            DkGpuAddr gpu_addr;
            bool allocated;
        } vertex_default_uniform, fragment_default_uniform;
        /* Dirty state tracking */
        union {
            struct {
                uint32_t vertex_shader : 1;
                uint32_t fragment_shader : 1;
                uint32_t depth_stencil : 1;
                uint32_t front_stencil : 1;
                uint32_t back_stencil : 1;
                uint32_t color_write : 1;
                uint32_t fragment_textures : 1;
                uint32_t vertex_default_uniform : 1;
                uint32_t fragment_default_uniform : 1;
                uint32_t vertex_streams : 1;
                uint32_t blend : 1;
                uint32_t rasterizer : 1;
                uint32_t viewport : 1;
            } bit;
            uint32_t raw;
        } dirty;
    } state;
} SceGxmContext;
static_assert(sizeof(SceGxmContext) <= SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE,
              "Oversized SceGxmContext");

typedef struct SceGxmSyncObject {
    DkFence fence;
} SceGxmSyncObject;

typedef struct SceGxmRegisteredProgram {
    const SceGxmProgram *programHeader;
} SceGxmRegisteredProgram;

typedef struct SceGxmShaderPatcher {
    SceGxmShaderPatcherParams params;
    SceGxmShaderPatcherId *registered_programs;
    uint32_t registered_count;
} SceGxmShaderPatcher;

typedef struct SceGxmVertexProgram {
    GxmProgramRef ref;
    uint32_t default_uniform_count;
    GxmUniformLayout uniform_layout;
    SceGxmVertexAttribute *attributes;
    unsigned int attributeCount;
    SceGxmVertexStream *streams;
    unsigned int streamCount;
    DkShader dk_shader;
} SceGxmVertexProgram;

typedef struct SceGxmFragmentProgram {
    GxmProgramRef ref;
    uint32_t default_uniform_count;
    GxmUniformLayout uniform_layout;
    SceGxmOutputRegisterFormat outputFormat;
    SceGxmMultisampleMode multisampleMode;
    SceGxmBlendInfo blendInfo;
    GxmBlendState blend;
    DkShader dk_shader;
} SceGxmFragmentProgram;

typedef struct SceGxmRenderTarget {
    SceGxmRenderTargetParams params;
    dk_surface_t shadow_color_surface;
    dk_surface_t shadow_ds_surface;
} SceGxmRenderTarget;

typedef struct {
    DkFence *new_fence;
    DkFence *old_fence;
    void *callback_data;
} DisplayQueueEntry;

typedef struct {
    uint32_t tail;
    uint32_t head;
    uint32_t num_entries;
    DisplayQueueEntry *entries;
    uint32_t display_queue_max_pending_count;
    SceGxmDisplayQueueCallback *display_queue_callback;
    uint32_t display_queue_callback_data_size;
    atomic_bool exit_thread;
    SceUID thid;
    UEvent ready_evflag;
    UEvent pending_evflag;
} DisplayQueueControlBlock;

/* Vita3K's shader recompiler */
typedef GxmViewportUniform GXMRenderVertUniformBlock;

struct GXMRenderFragUniformBlock {
    float back_disabled;
    float front_disabled;
    float writing_mask;
    float use_raw_image;
    float res_multiplier;
};

/* Global state */

static bool g_gxm_initialized;

static DkDevice g_dk_device;
static DkQueue g_render_queue;
static DkMemBlock g_notification_region_memblock;
static DisplayQueueControlBlock *g_display_queue;
static GxmShaderCode *g_shader_code_blocks;
static Mutex g_shader_code_lock;

static int SceGxmDisplayQueue_thread(SceSize args, void *argp);

#if DUMP_SHADER_SPIRV
static void dump_shader_spirv(const char *prefix, const uint32_t *spirv, uint32_t num_instr)
{
    static uint32_t cnt = 0;
    char name[128];
    snprintf(name, sizeof(name), VITA2HOS_DUMP_SHADER_PATH "/%s_%d.spv", prefix, cnt++);
    util_write_binary_file(name, spirv, num_instr * sizeof(uint32_t));
}
#endif

#if DUMP_SHADER_GLSL
static void dump_shader_glsl(const char *prefix, const char *glsl)
{
    static uint32_t cnt = 0;
    char name[128];
    snprintf(name, sizeof(name), VITA2HOS_DUMP_SHADER_PATH "/%s_%d.glsl", prefix, cnt++);
    util_write_text_file(name, glsl);
}
#endif

#if ENABLE_SHADER_DUMP_CB
static bool shader_dump_cb(const char *ext, const char *dump)
{
    LOG("Shader dumper CB: %s: %s", ext, dump);
    return true;
}
#define SHADER_DUMP_CB shader_dump_cb
#else
#define SHADER_DUMP_CB NULL
#endif

EXPORT(SceGxm, 0xB0F1E4EC, int, sceGxmInitialize, const SceGxmInitializeParams *params)
{
    DkQueueMaker queue_maker;
    uint32_t display_queue_num_entries;

    if (g_gxm_initialized)
        return SCE_GXM_ERROR_ALREADY_INITIALIZED;

    /* Create graphics queue */
    dkQueueMakerDefaults(&queue_maker, g_dk_device);
    queue_maker.flags = DkQueueFlags_Graphics;
    g_render_queue = dkQueueCreate(&queue_maker);

    /* Create memory block for the "notification region" */
    g_notification_region_memblock =
        dk_alloc_memblock(g_dk_device, SCE_GXM_NOTIFICATION_COUNT * sizeof(uint32_t),
                          DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuUncached);

    /* Allocate and initialize the display queue, and its worker thread */
    display_queue_num_entries = next_pow2(params->displayQueueMaxPendingCount + 1);
    g_display_queue = malloc(sizeof(DisplayQueueControlBlock) +
                             (sizeof(DisplayQueueEntry) + params->displayQueueCallbackDataSize) *
                                 display_queue_num_entries);
    assert(g_display_queue);
    g_display_queue->head = 0;
    g_display_queue->tail = 0;
    g_display_queue->num_entries = display_queue_num_entries;
    g_display_queue->entries =
        (DisplayQueueEntry *)((char *)g_display_queue + sizeof(DisplayQueueControlBlock));
    g_display_queue->display_queue_max_pending_count = params->displayQueueMaxPendingCount;
    g_display_queue->display_queue_callback = params->displayQueueCallback;
    g_display_queue->display_queue_callback_data_size = params->displayQueueCallbackDataSize;
    atomic_init(&g_display_queue->exit_thread, false);
    g_display_queue->thid = sceKernelCreateThread("SceGxmDisplayQueue", SceGxmDisplayQueue_thread,
                                                  64, 0x1000, 0, 0, NULL);
    assert(g_display_queue->thid > 0);

    for (uint32_t i = 0; i < display_queue_num_entries; i++) {
        g_display_queue->entries[i].callback_data =
            (char *)g_display_queue->entries +
            (sizeof(DisplayQueueEntry) * display_queue_num_entries) +
            i * params->displayQueueCallbackDataSize;
    }

    ueventCreate(&g_display_queue->ready_evflag, true);
    ueventCreate(&g_display_queue->pending_evflag, true);

    sceKernelStartThread(g_display_queue->thid, sizeof(g_display_queue), &g_display_queue);

    g_gxm_initialized = true;

    return 0;
}

EXPORT(SceGxm, 0xB627DE66, int, sceGxmTerminate)
{
    if (!g_gxm_initialized)
        return SCE_GXM_ERROR_UNINITIALIZED;
    atomic_store(&g_display_queue->exit_thread, true);
    ueventSignal(&g_display_queue->pending_evflag);
    int ret = sceKernelWaitThreadEnd(g_display_queue->thid, NULL, NULL);
    if (ret != 0)
        return ret;
    ret = sceKernelDeleteThread(g_display_queue->thid);
    if (ret != 0)
        return ret;

    free(g_display_queue);
    g_display_queue = NULL;
    /* Code can still be referenced by submitted command lists after its
     * program wrapper is released. Retain it until the queue is idle here. */
    dkQueueWaitIdle(g_render_queue);
    mutexLock(&g_shader_code_lock);
    gxm_shader_code_clear(&g_shader_code_blocks);
    mutexUnlock(&g_shader_code_lock);
    dkMemBlockDestroy(g_notification_region_memblock);
    g_notification_region_memblock = NULL;
    dkQueueDestroy(g_render_queue);
    g_render_queue = NULL;

    g_gxm_initialized = false;
    return 0;
}

EXPORT(SceGxm, 0xE84CE5B4, int, sceGxmCreateContext, const SceGxmContextParams *params,
       SceGxmContext **context)
{
    DkCmdBufMaker cmdbuf_maker;
    SceGxmContext *ctx = params->hostMem;

    if (params->hostMemSize < sizeof(SceGxmContext))
        return SCE_GXM_ERROR_INVALID_VALUE;

    memset(ctx, 0, sizeof(*ctx));
    ctx->params = *params;

    /* Get the passed backing storage buffer for the main command buffer */
    ctx->cmdbuf_memblock = SceSysmem_get_dk_memblock_for_addr(params->vdmRingBufferMem);
    assert(ctx->cmdbuf_memblock);

    /* Create the command buffer */
    dkCmdBufMakerDefaults(&cmdbuf_maker, g_dk_device);
    ctx->cmdbuf = dkCmdBufCreate(&cmdbuf_maker);
    assert(ctx->cmdbuf);

    /* Assing the backing storage buffer to the main command buffer */
    dkCmdBufAddMemory(ctx->cmdbuf, ctx->cmdbuf_memblock, 0, ctx->params.vdmRingBufferMemSize);

    /* Get the passed vertex ringbuffer for vertex default uniform buffer reservations */
    ctx->vertex_rb.memblock = SceSysmem_get_dk_memblock_for_addr(params->vertexRingBufferMem);
    assert(ctx->vertex_rb.memblock);
    assert(params->vertexRingBufferMem == dkMemBlockGetCpuAddr(ctx->vertex_rb.memblock));
    ctx->vertex_rb.size = params->vertexRingBufferMemSize;

    /* Get the passed fragment ringbuffer for fragment default uniform buffer reservations */
    ctx->fragment_rb.memblock = SceSysmem_get_dk_memblock_for_addr(params->fragmentRingBufferMem);
    assert(ctx->fragment_rb.memblock);
    assert(params->fragmentRingBufferMem == dkMemBlockGetCpuAddr(ctx->fragment_rb.memblock));
    ctx->fragment_rb.size = params->fragmentRingBufferMemSize;

    ctx->gxm_vert_unif_block_memblock = dk_alloc_memblock(
        g_dk_device, ALIGN(sizeof(GXMRenderVertUniformBlock), DK_UNIFORM_BUF_ALIGNMENT),
        DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached);

    ctx->gxm_frag_unif_block_memblock = dk_alloc_memblock(
        g_dk_device, ALIGN(sizeof(struct GXMRenderFragUniformBlock), DK_UNIFORM_BUF_ALIGNMENT),
        DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached);

    /* Init default state */
    memset(&ctx->state, 0, sizeof(ctx->state));

    dkRasterizerStateDefaults(&ctx->state.rasterizer);
    ctx->state.rasterizer.cullMode = DkFace_None;
    ctx->state.rasterizer.frontFace = DkFrontFace_CW;

    dkColorStateDefaults(&ctx->state.color);
    dkColorWriteStateDefaults(&ctx->state.color_write);

    ctx->state.depth_stencil.depthTestEnable = true;
    ctx->state.depth_stencil.depthWriteEnable = true;
    ctx->state.depth_stencil.stencilTestEnable = true;
    ctx->state.depth_stencil.depthCompareOp = DkCompareOp_Lequal;

    ctx->state.depth_stencil.stencilFrontFailOp = DkStencilOp_Keep;
    ctx->state.depth_stencil.stencilFrontPassOp = DkStencilOp_Keep;
    ctx->state.depth_stencil.stencilFrontDepthFailOp = DkStencilOp_Keep;
    ctx->state.depth_stencil.stencilFrontCompareOp = DkCompareOp_Always;

    ctx->state.depth_stencil.stencilBackFailOp = DkStencilOp_Keep;
    ctx->state.depth_stencil.stencilBackPassOp = DkStencilOp_Keep;
    ctx->state.depth_stencil.stencilBackDepthFailOp = DkStencilOp_Keep;
    ctx->state.depth_stencil.stencilBackCompareOp = DkCompareOp_Always;

    ctx->state.front_stencil.ref = 0;
    ctx->state.front_stencil.compare_mask = 0;
    ctx->state.front_stencil.write_mask = 0;

    ctx->state.back_stencil.ref = 0;
    ctx->state.back_stencil.compare_mask = 0;
    ctx->state.back_stencil.write_mask = 0;

    *context = ctx;

    return 0;
}

EXPORT(SceGxm, 0xEDDC5FB2, int, sceGxmDestroyContext, SceGxmContext *context)
{
    dkQueueWaitIdle(g_render_queue);
    gxm_texture_staging_release(&context->texture_staging);
    gxm_uniform_staging_release(&context->uniform_staging);
    dkMemBlockDestroy(context->gxm_vert_unif_block_memblock);
    dkMemBlockDestroy(context->gxm_frag_unif_block_memblock);
    dkCmdBufDestroy(context->cmdbuf);

    if (context->state.background_ds.memblock)
        dkMemBlockDestroy(context->state.background_ds.memblock);

    return 0;
}

EXPORT(SceGxm, 0x0733D8AE, void, sceGxmFinish, SceGxmContext *context)
{
    dkQueueWaitIdle(g_render_queue);
}

EXPORT(SceGxm, 0x8BDE825A, volatile unsigned int *, sceGxmGetNotificationRegion, void)
{
    return dkMemBlockGetCpuAddr(g_notification_region_memblock);
}

EXPORT(SceGxm, 0x6A6013E1, int, sceGxmSyncObjectCreate, SceGxmSyncObject **syncObject)
{
    SceGxmSyncObject *sync_object;

    sync_object = malloc(sizeof(*sync_object));
    if (!sync_object)
        return SCE_KERNEL_ERROR_NO_MEMORY;

    memset(sync_object, 0, sizeof(*sync_object));
    *syncObject = sync_object;

    return 0;
}

EXPORT(SceGxm, 0x889AE88C, int, sceGxmSyncObjectDestroy, SceGxmSyncObject *syncObject)
{
    free(syncObject);
    return 0;
}

EXPORT(SceGxm, 0x9F448E79, int, sceGxmNotificationWait, const SceGxmNotification *notification)
{
    DkVariable variable;
    uint32_t offset =
        dk_memblock_cpu_addr_offset(g_notification_region_memblock, (void *)notification->address);
    assert(offset < SCE_GXM_NOTIFICATION_COUNT * sizeof(uint32_t));

    dkVariableInitialize(&variable, g_notification_region_memblock, offset);

    while (dkVariableRead(&variable) != notification->value)
        dkQueueWaitIdle(g_render_queue);

    return 0;
}

EXPORT(SceGxm, 0x05032658, int, sceGxmShaderPatcherCreate, const SceGxmShaderPatcherParams *params,
       SceGxmShaderPatcher **shaderPatcher)
{
    SceGxmShaderPatcher *shader_patcher;

    if (!params || !shaderPatcher)
        return SCE_GXM_ERROR_INVALID_POINTER;
    *shaderPatcher = NULL;
    shader_patcher = malloc(sizeof(*shader_patcher));
    if (!shader_patcher)
        return SCE_KERNEL_ERROR_NO_MEMORY;

    memset(shader_patcher, 0, sizeof(*shader_patcher));
    shader_patcher->params = *params;
    *shaderPatcher = shader_patcher;

    return 0;
}

EXPORT(SceGxm, 0xEAA5B100, int, sceGxmShaderPatcherDestroy, SceGxmShaderPatcher *shaderPatcher)
{
    if (!shaderPatcher)
        return SCE_GXM_ERROR_INVALID_POINTER;
    for (uint32_t i = 0; i < shaderPatcher->registered_count; ++i)
        free(shaderPatcher->registered_programs[i]);
    free(shaderPatcher->registered_programs);
    free(shaderPatcher);
    return 0;
}

EXPORT(SceGxm, 0x2B528462, int, sceGxmShaderPatcherRegisterProgram,
       SceGxmShaderPatcher *shaderPatcher, const SceGxmProgram *programHeader,
       SceGxmShaderPatcherId *programId)
{
    SceGxmRegisteredProgram *shader_patcher_id;

    if (!programId)
        return SCE_GXM_ERROR_INVALID_POINTER;
    *programId = NULL;
    if (!shaderPatcher || !programHeader)
        return SCE_GXM_ERROR_INVALID_POINTER;
    shader_patcher_id = malloc(sizeof(*shader_patcher_id));
    if (!shader_patcher_id)
        return SCE_KERNEL_ERROR_NO_MEMORY;

    memset(shader_patcher_id, 0, sizeof(*shader_patcher_id));
    shader_patcher_id->programHeader = programHeader;

    int ret = gxm_program_registry_add(&shaderPatcher->registered_programs,
                                        &shaderPatcher->registered_count, shader_patcher_id);
    if (ret != 0) {
        free(shader_patcher_id);
        return ret;
    }

    *programId = shader_patcher_id;

    return 0;
}

EXPORT(SceGxm, 0xF103AF8A, int, sceGxmShaderPatcherUnregisterProgram,
       SceGxmShaderPatcher *shaderPatcher, SceGxmShaderPatcherId programId)
{
    if (!shaderPatcher || !programId)
        return SCE_GXM_ERROR_INVALID_POINTER;
    int ret = gxm_program_registry_remove(shaderPatcher->registered_programs,
                                           &shaderPatcher->registered_count, programId);
    if (ret != 0)
        return ret;
    free(programId);
    return 0;
}

static int translate_shader(DkShader *shader, const SceGxmProgram *program, pipeline_stage stage,
                            const char *prefix,
                            const SceGxmVertexAttribute *attributes, unsigned int attributeCount)
{
    bool ret;
    char *glsl = NULL;
    uint32_t shader_size = 0;
    DkShaderMaker shader_maker;
    if (!shader || !program)
        return SCE_GXM_ERROR_INVALID_POINTER;

#if DUMP_SHADER_SPIRV
    uint32_t *spirv;
    uint32_t num_instr;

    ret = convert_gxp_to_spirv_c(&spirv, &num_instr, program, prefix, false, false, false, false,
                                 attributes, attributeCount, false, false, SHADER_DUMP_CB);
    if (ret) {
        dump_shader_spirv(prefix, spirv, num_instr);
        free(spirv);
    }
#endif

    LOG("Converting shader (%s) to GLSL...", prefix);
    ret = convert_gxp_to_glsl_c(&glsl, program, prefix, false, false, false, false, attributes,
                                attributeCount, false, false, SHADER_DUMP_CB);
    LOG("  ret: %d", ret);
#if DUMP_SHADER_GLSL
    if (ret)
        dump_shader_glsl(prefix, glsl);
#endif
    if (!ret || !glsl) {
        free(glsl);
        return SCE_GXM_ERROR_INVALID_VALUE;
    }

    LOG("UAM compiling shader (%s)...", prefix);
    void *compiled = NULL;
    ret = uam_compiler_compile_glsl_alloc(stage, glsl, &compiled, &shader_size);
    LOG("  compile ret: %d, size: 0x%x", ret, shader_size);
    free(glsl);
    if (!ret || !shader_size) {
        free(compiled);
        return SCE_GXM_ERROR_INVALID_VALUE;
    }

    DkMemBlock code;
    mutexLock(&g_shader_code_lock);
    int upload_result = gxm_shader_code_upload(g_dk_device, &g_shader_code_blocks,
                                              compiled, shader_size, &code);
    mutexUnlock(&g_shader_code_lock);
    free(compiled);
    if (upload_result != 0) return upload_result;
    dkShaderMakerDefaults(&shader_maker, code, 0);
    dkShaderInitialize(shader, &shader_maker);

    return 0;
}

EXPORT(SceGxm, 0xB7BBA6D5, int, sceGxmShaderPatcherCreateVertexProgram,
       SceGxmShaderPatcher *shaderPatcher, SceGxmShaderPatcherId programId,
       const SceGxmVertexAttribute *attributes, unsigned int attributeCount,
       const SceGxmVertexStream *streams, unsigned int streamCount,
       SceGxmVertexProgram **vertexProgram)
{
    int ret;
    SceGxmVertexProgram *vertex_program;

    if (!vertexProgram)
        return SCE_GXM_ERROR_INVALID_POINTER;
    *vertexProgram = NULL;
    if (!shaderPatcher || !programId)
        return SCE_GXM_ERROR_INVALID_POINTER;
    if (!gxm_program_registry_contains(shaderPatcher->registered_programs,
                                        shaderPatcher->registered_count, programId))
        return SCE_GXM_ERROR_INVALID_VALUE;
    if (!programId->programHeader)
        return SCE_GXM_ERROR_INVALID_POINTER;
    ret = gxm_validate_vertex_layout(attributes, attributeCount, streams, streamCount);
    if (ret != 0)
        return ret;

    vertex_program = malloc(sizeof(*vertex_program));
    if (!vertex_program)
        return SCE_KERNEL_ERROR_NO_MEMORY;

    memset(vertex_program, 0, sizeof(*vertex_program));
    atomic_init(&vertex_program->ref.count, 1);
    vertex_program->default_uniform_count = programId->programHeader->default_uniform_buffer_count;
    if (!gxp_uniform_layout_c(programId->programHeader, &vertex_program->uniform_layout)) {
        ret = SCE_GXM_ERROR_INVALID_VALUE;
        goto fail;
    }
    if (attributeCount) {
        vertex_program->attributes = calloc(attributeCount, sizeof(SceGxmVertexAttribute));
        if (!vertex_program->attributes) {
            ret = SCE_KERNEL_ERROR_NO_MEMORY;
            goto fail;
        }
        memcpy(vertex_program->attributes, attributes, attributeCount * sizeof(SceGxmVertexAttribute));
    }
    vertex_program->attributeCount = attributeCount;
    if (streamCount) {
        vertex_program->streams = calloc(streamCount, sizeof(SceGxmVertexStream));
        if (!vertex_program->streams) {
            ret = SCE_KERNEL_ERROR_NO_MEMORY;
            goto fail;
        }
        memcpy(vertex_program->streams, streams, streamCount * sizeof(SceGxmVertexStream));
    }
    vertex_program->streamCount = streamCount;

    ret = translate_shader(&vertex_program->dk_shader, programId->programHeader,
                           pipeline_stage_vertex, "vert",
                           attributes, attributeCount);
    if (ret != 0)
        goto fail;

    *vertexProgram = vertex_program;

    return 0;
fail:
    free(vertex_program->attributes);
    free(vertex_program->streams);
    free(vertex_program);
    return ret;
}

EXPORT(SceGxm, 0xAC1FF2DA, int, sceGxmShaderPatcherReleaseVertexProgram,
       SceGxmShaderPatcher *shaderPatcher, SceGxmVertexProgram *vertexProgram)
{
    if (!shaderPatcher || !vertexProgram) return SCE_GXM_ERROR_INVALID_POINTER;
    bool destroy;
    int ret = gxm_program_ref_release(&vertexProgram->ref, &destroy);
    if (ret || !destroy) return ret;
    /* GPU code has independent storage retained until GXM termination. */
    free(vertexProgram->attributes);
    free(vertexProgram->streams);
    free(vertexProgram);
    return 0;
}

EXPORT(SceGxm, 0x4ED2E49D, int, sceGxmShaderPatcherCreateFragmentProgram,
       SceGxmShaderPatcher *shaderPatcher, SceGxmShaderPatcherId programId,
       SceGxmOutputRegisterFormat outputFormat, SceGxmMultisampleMode multisampleMode,
       const SceGxmBlendInfo *blendInfo, const SceGxmProgram *vertexProgram,
       SceGxmFragmentProgram **fragmentProgram)
{
    int ret;
    SceGxmFragmentProgram *fragment_program;

    if (!fragmentProgram)
        return SCE_GXM_ERROR_INVALID_POINTER;
    *fragmentProgram = NULL;
    if (!shaderPatcher || !programId)
        return SCE_GXM_ERROR_INVALID_POINTER;
    if (!gxm_program_registry_contains(shaderPatcher->registered_programs,
                                        shaderPatcher->registered_count, programId))
        return SCE_GXM_ERROR_INVALID_VALUE;
    if (!programId->programHeader)
        return SCE_GXM_ERROR_INVALID_POINTER;
    fragment_program = malloc(sizeof(*fragment_program));
    if (!fragment_program)
        return SCE_KERNEL_ERROR_NO_MEMORY;

    memset(fragment_program, 0, sizeof(*fragment_program));
    atomic_init(&fragment_program->ref.count, 1);
    fragment_program->default_uniform_count = programId->programHeader->default_uniform_buffer_count;
    if (!gxp_uniform_layout_c(programId->programHeader, &fragment_program->uniform_layout)) {
        free(fragment_program);
        return SCE_GXM_ERROR_INVALID_VALUE;
    }
    fragment_program->outputFormat = outputFormat;
    fragment_program->multisampleMode = multisampleMode;
    if (blendInfo) {
        fragment_program->blendInfo = *blendInfo;
    } else {
        fragment_program->blendInfo = (SceGxmBlendInfo){
            SCE_GXM_COLOR_MASK_ALL,    SCE_GXM_BLEND_FUNC_NONE,   SCE_GXM_BLEND_FUNC_NONE,
            SCE_GXM_BLEND_FACTOR_ONE,  SCE_GXM_BLEND_FACTOR_ZERO, SCE_GXM_BLEND_FACTOR_ONE,
            SCE_GXM_BLEND_FACTOR_ZERO,
        };
    }

    ret = gxm_translate_blend(&fragment_program->blendInfo, &fragment_program->blend);
    if (ret != 0) {
        free(fragment_program);
        return ret;
    }
    ret = translate_shader(&fragment_program->dk_shader, programId->programHeader,
                           pipeline_stage_fragment, "frag",
                           NULL, 0);
    if (ret != 0) {
        free(fragment_program);
        return ret;
    }

    *fragmentProgram = fragment_program;

    return 0;
}

EXPORT(SceGxm, 0xBE2743D1, int, sceGxmShaderPatcherReleaseFragmentProgram,
       SceGxmShaderPatcher *shaderPatcher, SceGxmFragmentProgram *fragmentProgram)
{
    if (!shaderPatcher || !fragmentProgram) return SCE_GXM_ERROR_INVALID_POINTER;
    bool destroy;
    int ret = gxm_program_ref_release(&fragmentProgram->ref, &destroy);
    if (ret || !destroy) return ret;
    /* GPU code has independent storage retained until GXM termination. */
    free(fragmentProgram);
    return 0;
}

EXPORT(SceGxm, 0x0FD1E589, int, sceGxmShaderPatcherAddRefVertexProgram,
       SceGxmShaderPatcher *shaderPatcher, SceGxmVertexProgram *vertexProgram)
{
    if (!shaderPatcher || !vertexProgram) return SCE_GXM_ERROR_INVALID_POINTER;
    return gxm_program_ref_add(&vertexProgram->ref);
}

EXPORT(SceGxm, 0x4CD2D19F, int, sceGxmShaderPatcherAddRefFragmentProgram,
       SceGxmShaderPatcher *shaderPatcher, SceGxmFragmentProgram *fragmentProgram)
{
    if (!shaderPatcher || !fragmentProgram) return SCE_GXM_ERROR_INVALID_POINTER;
    return gxm_program_ref_add(&fragmentProgram->ref);
}

EXPORT(SceGxm, 0xA1A16FF6, int, sceGxmShaderPatcherGetVertexProgramRefCount,
       SceGxmShaderPatcher *shaderPatcher, SceGxmVertexProgram *vertexProgram, unsigned int *count)
{
    if (!shaderPatcher || !vertexProgram) return SCE_GXM_ERROR_INVALID_POINTER;
    return gxm_program_ref_get(&vertexProgram->ref, count);
}

EXPORT(SceGxm, 0x2C5550F0, int, sceGxmShaderPatcherGetFragmentProgramRefCount,
       SceGxmShaderPatcher *shaderPatcher, SceGxmFragmentProgram *fragmentProgram, unsigned int *count)
{
    if (!shaderPatcher || !fragmentProgram) return SCE_GXM_ERROR_INVALID_POINTER;
    return gxm_program_ref_get(&fragmentProgram->ref, count);
}

EXPORT(SceGxm, 0xA949A803, const SceGxmProgram *, sceGxmShaderPatcherGetProgramFromId,
       SceGxmShaderPatcherId programId)
{
    if (programId)
        return programId->programHeader;

    return NULL;
}

EXPORT(SceGxm, 0xB291C959, int, sceGxmGetRenderTargetMemSize,
       const SceGxmRenderTargetParams *params, unsigned int *driverMemSize)
{
    *driverMemSize = sizeof(SceGxmRenderTarget);
    return 0;
}

EXPORT(SceGxm, 0x207AF96B, int, sceGxmCreateRenderTarget, const SceGxmRenderTargetParams *params,
       SceGxmRenderTarget **renderTarget)
{
    SceGxmRenderTarget *render_target;

    if (!g_gxm_initialized)
        return SCE_GXM_ERROR_UNINITIALIZED;
    else if (!params)
        return SCE_GXM_ERROR_INVALID_POINTER;
    else if (params->scenesPerFrame > SCE_GXM_MAX_SCENES_PER_RENDERTARGET)
        return SCE_GXM_ERROR_INVALID_VALUE;

    render_target = malloc(sizeof(*render_target));
    if (!render_target)
        return SCE_KERNEL_ERROR_NO_MEMORY;

    render_target->params = *params;
    /* Create shadow color and depth/stencil surfaces */
    dk_surface_create(g_dk_device, &render_target->shadow_color_surface, params->width,
                      params->height, DkImageFormat_RGBA8_Unorm,
                      DkImageFlags_UsageRender | DkImageFlags_Usage2DEngine |
                          DkImageFlags_HwCompression);
    dk_surface_create(g_dk_device, &render_target->shadow_ds_surface, params->width, params->height,
                      DkImageFormat_ZF32_X24S8,
                      DkImageFlags_UsageRender | DkImageFlags_Usage2DEngine |
                          DkImageFlags_HwCompression);

    *renderTarget = render_target;

    return 0;
}

EXPORT(SceGxm, 0x0B94C50A, int, sceGxmDestroyRenderTarget, SceGxmRenderTarget *renderTarget)
{
    dk_surface_destroy(&renderTarget->shadow_color_surface);
    dk_surface_destroy(&renderTarget->shadow_ds_surface);
    free(renderTarget);
    return 0;
}

EXPORT(SceGxm, 0xED0F6E25, int, sceGxmColorSurfaceInit, SceGxmColorSurface *surface,
       SceGxmColorFormat colorFormat, SceGxmColorSurfaceType surfaceType,
       SceGxmColorSurfaceScaleMode scaleMode, SceGxmOutputRegisterSize outputRegisterSize,
       unsigned int width, unsigned int height, unsigned int strideInPixels, void *data)
{
    SceGxmColorSurfaceInner *inner = (SceGxmColorSurfaceInner *)surface;

    memset(inner, 0, sizeof(*inner));
    inner->disabled = 0;
    inner->downscale = scaleMode == SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE;
    inner->width = width;
    inner->height = height;
    inner->strideInPixels = strideInPixels;
    inner->data = data;
    inner->colorFormat = colorFormat;
    inner->surfaceType = surfaceType;
    inner->outputRegisterSize = outputRegisterSize;

    return 0;
}

EXPORT(SceGxm, 0xCA9D41D1, int, sceGxmDepthStencilSurfaceInit, SceGxmDepthStencilSurface *surface,
       SceGxmDepthStencilFormat depthStencilFormat, SceGxmDepthStencilSurfaceType surfaceType,
       unsigned int strideInSamples, void *depthData, void *stencilData)

{
    if (!surface)
        return SCE_GXM_ERROR_INVALID_POINTER;
    else if ((uint32_t)depthData % SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT != 0)
        return SCE_GXM_ERROR_INVALID_ALIGNMENT;
    else if ((strideInSamples == 0) || ((strideInSamples % SCE_GXM_TILE_SIZEX) != 0))
        return SCE_GXM_ERROR_INVALID_VALUE;

    memset(surface, 0, sizeof(*surface));
    surface->zlsControl =
        SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_DISABLED | SCE_GXM_DEPTH_STENCIL_FORCE_STORE_DISABLED |
        ((strideInSamples >> 5) - 1) << SCE_GXM_DEPTH_STENCIL_ZLS_CTRL_STRIDE_OFFSET |
        (surfaceType &
         (SCE_GXM_DEPTH_STENCIL_ZLS_CTRL_TYPE_MASK << SCE_GXM_DEPTH_STENCIL_ZLS_CTRL_TYPE_OFFSET)) |
        (depthStencilFormat & (SCE_GXM_DEPTH_STENCIL_ZLS_CTRL_FORMAT_MASK
                               << SCE_GXM_DEPTH_STENCIL_ZLS_CTRL_FORMAT_OFFSET));
    surface->depthData = depthData;
    surface->stencilData = stencilData;
    surface->backgroundDepth = 1.0f;
    surface->backgroundControl = SCE_GXM_DEPTH_STENCIL_BG_CTRL_MASK_BIT;

    return 0;
}

EXPORT(SceGxm, 0x14BD831F, void, sceGxmSetFrontDepthFunc, SceGxmContext *context,
       SceGxmDepthFunc depthFunc)
{
    context->state.depth_stencil.depthCompareOp = gxm_depth_func_to_dk_compare_op(depthFunc);
    context->state.dirty.bit.depth_stencil = true;

    if (!context->state.two_sided_mode)
        sceGxmSetBackDepthFunc(context, depthFunc);
}

EXPORT(SceGxm, 0xF32CBF34, void, sceGxmSetFrontDepthWriteEnable, SceGxmContext *context,
       SceGxmDepthWriteMode enable)
{
    context->state.depth_stencil.depthWriteEnable = (enable == SCE_GXM_DEPTH_WRITE_ENABLED) ? 1 : 0;
    context->state.dirty.bit.depth_stencil = true;

    if (!context->state.two_sided_mode)
        sceGxmSetBackDepthWriteEnable(context, enable);
}

EXPORT(SceGxm, 0x8FA6FE44, void, sceGxmSetFrontStencilRef, SceGxmContext *context,
       unsigned int sref)
{
    context->state.front_stencil.ref = sref;
    context->state.dirty.bit.front_stencil = true;

    if (!context->state.two_sided_mode)
        sceGxmSetBackStencilRef(context, sref);
}

EXPORT(SceGxm, 0xB042A4D2, void, sceGxmSetBackDepthFunc, SceGxmContext *context,
       SceGxmDepthFunc depthFunc)
{
    /* Unsupported */
}

EXPORT(SceGxm, 0xC18B706B, void, sceGxmSetBackDepthWriteEnable, SceGxmContext *context,
       SceGxmDepthWriteMode enable)
{
    /* Unsupported */
}

EXPORT(SceGxm, 0x866A0517, void, sceGxmSetBackStencilRef, SceGxmContext *context, unsigned int sref)
{
    context->state.back_stencil.ref = sref;
    context->state.dirty.bit.back_stencil = true;
}

EXPORT(SceGxm, 0xB8645A9A, void, sceGxmSetFrontStencilFunc, SceGxmContext *context,
       SceGxmStencilFunc func, SceGxmStencilOp stencilFail, SceGxmStencilOp depthFail,
       SceGxmStencilOp depthPass, unsigned char compareMask, unsigned char writeMask)
{

    context->state.depth_stencil.stencilFrontFailOp = gxm_stencil_op_to_dk_stencil_op(stencilFail);
    context->state.depth_stencil.stencilFrontDepthFailOp =
        gxm_stencil_op_to_dk_stencil_op(depthFail);
    context->state.depth_stencil.stencilFrontCompareOp = gxm_stencil_func_to_dk_compare_op(func);
    context->state.front_stencil.compare_mask = compareMask;
    context->state.front_stencil.write_mask = writeMask;
    context->state.dirty.bit.depth_stencil = true;
    context->state.dirty.bit.front_stencil = true;

    if (!context->state.two_sided_mode) {
        sceGxmSetBackStencilFunc(context, func, stencilFail, depthFail, depthPass, compareMask,
                                 writeMask);
    }
}

EXPORT(SceGxm, 0x1A68C8D2, void, sceGxmSetBackStencilFunc, SceGxmContext *context,
       SceGxmStencilFunc func, SceGxmStencilOp stencilFail, SceGxmStencilOp depthFail,
       SceGxmStencilOp depthPass, unsigned char compareMask, unsigned char writeMask)
{
    context->state.depth_stencil.stencilBackFailOp = gxm_stencil_op_to_dk_stencil_op(stencilFail);
    context->state.depth_stencil.stencilBackDepthFailOp =
        gxm_stencil_op_to_dk_stencil_op(depthFail);
    context->state.depth_stencil.stencilBackCompareOp = gxm_stencil_func_to_dk_compare_op(func);
    context->state.back_stencil.compare_mask = compareMask;
    context->state.back_stencil.write_mask = writeMask;
    context->state.dirty.bit.depth_stencil = true;
    context->state.dirty.bit.back_stencil = true;
}

EXPORT(SceGxm, 0x3EB3380B, void, sceGxmSetViewport, SceGxmContext *context,
       float xOffset, float xScale, float yOffset, float yScale, float zOffset, float zScale)
{
    if (!context)
        return;
    if (!gxm_viewport_set(&context->state.viewport, xOffset, xScale, yOffset, yScale,
                          zOffset, zScale)) {
        LOG("sceGxmSetViewport: invalid transform");
        return;
    }
    context->state.dirty.bit.viewport = true;
}

EXPORT(SceGxm, 0x814F61EB, void, sceGxmSetViewportEnable, SceGxmContext *context,
       SceGxmViewportMode enable)
{
    if (!context)
        return;
    if (enable != SCE_GXM_VIEWPORT_ENABLED && enable != SCE_GXM_VIEWPORT_DISABLED) {
        LOG("sceGxmSetViewportEnable: invalid mode %u", (unsigned)enable);
        return;
    }
    context->state.viewport.disabled = enable == SCE_GXM_VIEWPORT_DISABLED;
    context->state.dirty.bit.viewport = true;
}

EXPORT(SceGxm, 0x70C86868, void, sceGxmSetRegionClip, SceGxmContext *context,
       SceGxmRegionClipMode mode, unsigned int xMin, unsigned int yMin,
       unsigned int xMax, unsigned int yMax)
{
    if (!context)
        return;
    if (gxm_region_clip_set(&context->state.region_clip, mode, xMin, yMin, xMax, yMax))
        LOG("sceGxmSetRegionClip: invalid mode %u", (unsigned)mode);
}

EXPORT(SceGxm, 0x60CF708E, void, sceGxmSetDefaultRegionClipAndViewport,
       SceGxmContext *context, unsigned int xMax, unsigned int yMax)
{
    if (!context)
        return;
    float width = (float)xMax + 1, height = (float)yMax + 1;
    gxm_viewport_set(&context->state.viewport, width * 0.5f, width * 0.5f,
                     height * 0.5f, -height * 0.5f, 0.5f, 0.5f);
    context->state.dirty.bit.viewport = true;
    gxm_region_clip_set(&context->state.region_clip, SCE_GXM_REGION_CLIP_OUTSIDE,
                        0, 0, xMax, yMax);
}

static void set_vita3k_gxm_uniform_blocks(SceGxmContext *context, float width, float height)
{
    GXMRenderVertUniformBlock vert_unif;
    DkViewport viewport;
    gxm_viewport_translate(&context->state.viewport, width, height, &viewport, &vert_unif);
    dkCmdBufSetViewports(context->cmdbuf, 0, &viewport, 1);

    const struct GXMRenderFragUniformBlock frag_unif = {
        .back_disabled = 0.0f,
        .front_disabled = 0.0f,
        .writing_mask = 0.0f,
        .use_raw_image = 0.0f,
        .res_multiplier = 0,
    };

    dkCmdBufPushConstants(context->cmdbuf,
                          dkMemBlockGetGpuAddr(context->gxm_vert_unif_block_memblock),
                          ALIGN(sizeof(vert_unif), DK_UNIFORM_BUF_ALIGNMENT), 0,
                          sizeof(vert_unif), &vert_unif);
    dkCmdBufBindUniformBuffer(context->cmdbuf, DkStage_Vertex, 2 /* hardcoded */,
                              dkMemBlockGetGpuAddr(context->gxm_vert_unif_block_memblock),
                              ALIGN(sizeof(vert_unif), DK_UNIFORM_BUF_ALIGNMENT));

    dkCmdBufPushConstants(context->cmdbuf,
                          dkMemBlockGetGpuAddr(context->gxm_frag_unif_block_memblock),
                          ALIGN(sizeof(frag_unif), DK_UNIFORM_BUF_ALIGNMENT), 0,
                          sizeof(frag_unif), &frag_unif);
    dkCmdBufBindUniformBuffer(context->cmdbuf, DkStage_Fragment, 3 /* hardcoded */,
                              dkMemBlockGetGpuAddr(context->gxm_frag_unif_block_memblock),
                              ALIGN(sizeof(frag_unif), DK_UNIFORM_BUF_ALIGNMENT));
}

EXPORT(SceGxm, 0x8734FF4E, int, sceGxmBeginScene, SceGxmContext *context, unsigned int flags,
       const SceGxmRenderTarget *renderTarget, const SceGxmValidRegion *validRegion,
       SceGxmSyncObject *vertexSyncObject, SceGxmSyncObject *fragmentSyncObject,
       const SceGxmColorSurface *colorSurface, const SceGxmDepthStencilSurface *depthStencil)
{
    uint16_t rt_width = renderTarget->params.width;
    uint16_t rt_height = renderTarget->params.height;
    DkViewport viewport = { 0.0f, 0.0f, (float)rt_width, (float)rt_height, 0.0f, 1.0f };
    DkScissor scissor = { 0, 0, rt_width, rt_height };
    SceGxmColorSurfaceInner *color_surface_inner = (SceGxmColorSurfaceInner *)colorSurface;

    if (context->state.in_scene)
        return SCE_GXM_ERROR_WITHIN_SCENE;

    if (validRegion && (validRegion->xMax >= rt_width || validRegion->yMax >= rt_height))
        return SCE_GXM_ERROR_INVALID_VALUE;

    /* BeginScene resets the transform to the default region, while retaining
     * the viewport enable mode. Later setters are replayed at the next draw. */
    sceGxmSetDefaultRegionClipAndViewport(context, validRegion ? validRegion->xMax : rt_width - 1U,
                                         validRegion ? validRegion->yMax : rt_height - 1U);

    if (context->texture_staging || context->uniform_staging) {
        /* The previous scene was submitted by EndScene. Retire its temporary
         * images and uniform snapshots only after the GPU is finished. */
        dkQueueWaitIdle(g_render_queue);
        gxm_texture_staging_release(&context->texture_staging);
        gxm_uniform_staging_release(&context->uniform_staging);
    }

    LOG("sceGxmBeginScene to renderTarget %p, fragmentSyncObject: %p, "
        "w: %" PRId32 ", h: %" PRId32 ", stride: %" PRId32 ", CPU addr: %p",
        renderTarget, fragmentSyncObject, color_surface_inner->width, color_surface_inner->height,
        color_surface_inner->strideInPixels, color_surface_inner->data);

    dkCmdBufClear(context->cmdbuf);
    dkCmdBufBindRenderTarget(context->cmdbuf, &renderTarget->shadow_color_surface.view,
                             &renderTarget->shadow_ds_surface.view);
    dkCmdBufSetViewports(context->cmdbuf, 0, &viewport, 1);
    dkCmdBufSetScissors(context->cmdbuf, 0, &scissor, 1);
    dkCmdBufBindRasterizerState(context->cmdbuf, &context->state.rasterizer);
    dkCmdBufBindColorState(context->cmdbuf, &context->state.color);

    context->state.vertex_rb.head = 0;
    context->state.fragment_rb.head = 0;

    /* Wait until the framebuffer is swapped out before writing to it */
    if (fragmentSyncObject)
        dkCmdBufWaitFence(context->cmdbuf, &fragmentSyncObject->fence);

    if (!depthStencil) {
        dkCmdBufClearDepthStencil(context->cmdbuf, true, 1.0f, 0xFF, 0);
    } else {
        if (!(depthStencil->zlsControl & SCE_GXM_DEPTH_STENCIL_FORCE_LOAD_ENABLED)) {
            dkCmdBufClearDepthStencil(context->cmdbuf, true, depthStencil->backgroundDepth, 0xFF,
                                      depthStencil->zlsControl &
                                          SCE_GXM_DEPTH_STENCIL_BG_CTRL_STENCIL_MASK);
        } else {
            /* TODO: Copy GXM DS to shadow? */
        }
    }

    /* Mark all state as dirty to make sure we bind everything before the first draw call */
    context->state.dirty.raw = ~(uint32_t)0;
    context->state.render_target = renderTarget;
    context->state.color_surface = color_surface_inner;
    context->state.ds_surface = depthStencil;
    context->state.fragment_sync_object = fragmentSyncObject;
    context->state.in_scene = true;

    return 0;
}

EXPORT(SceGxm, 0xFE300E2F, int, sceGxmEndScene, SceGxmContext *context,
       const SceGxmNotification *vertexNotification, const SceGxmNotification *fragmentNotification)
{
    DkCmdList cmd_list;
    DkVariable variable;
    uint32_t offset;
    DkImage color_surface_image;
    DkImage ds_surface_image;
    const SceGxmRenderTarget *const render_target = context->state.render_target;
    const dk_surface_t *const shadow_color_surface = &render_target->shadow_color_surface;
    const dk_surface_t *const shadow_ds_surface = &render_target->shadow_ds_surface;
    const SceGxmColorSurfaceInner *const gxm_color_surface = context->state.color_surface;
    const SceGxmDepthStencilSurface *const gxm_ds_surface = context->state.ds_surface;
    const uint32_t rt_width = render_target->params.width;
    const uint32_t rt_height = render_target->params.height;
    bool discard_color;
    bool discard_stencil;

    LOG("sceGxmEndScene");

    if (!context->state.in_scene)
        return SCE_GXM_ERROR_NOT_WITHIN_SCENE;

    if (vertexNotification) {
        offset = dk_memblock_cpu_addr_offset(g_notification_region_memblock,
                                             (void *)vertexNotification->address);
        assert(offset < SCE_GXM_NOTIFICATION_COUNT * sizeof(uint32_t));

        dkVariableInitialize(&variable, g_notification_region_memblock, offset);
        dkCmdBufSignalVariable(context->cmdbuf, &variable, DkVarOp_Set, vertexNotification->value,
                               DkPipelinePos_Rasterizer);
    }

    if (fragmentNotification) {
        offset = dk_memblock_cpu_addr_offset(g_notification_region_memblock,
                                             (void *)fragmentNotification->address);
        assert(offset < SCE_GXM_NOTIFICATION_COUNT * sizeof(uint32_t));

        dkVariableInitialize(&variable, g_notification_region_memblock, offset);
        dkCmdBufSignalVariable(context->cmdbuf, &variable, DkVarOp_Set, fragmentNotification->value,
                               DkPipelinePos_Bottom);
    }

    /* Wait for fragments to be completed before the copy/discard */
    dkCmdBufBarrier(context->cmdbuf, DkBarrier_Fragments, 0);

    /* Check whether we need to discard the color and/or depth/stencil surfaces */
    discard_color = gxm_color_surface == NULL;
    discard_stencil = !gxm_ds_surface ||
                      !(gxm_ds_surface->zlsControl & SCE_GXM_DEPTH_STENCIL_FORCE_STORE_ENABLED);

    /* Copy from the shadow color surface to the GXM color surface */
    if (discard_color) {
        dkCmdBufDiscardColor(context->cmdbuf, 0);
    } else {
        if (dk_image_for_gxm_color_surface(g_dk_device, &color_surface_image, gxm_color_surface)) {
            LOG("Copying color surface: shadow -> GXM");
            dk_cmdbuf_copy_image(context->cmdbuf, &shadow_color_surface->image,
                                 shadow_color_surface->width, shadow_color_surface->height,
                                 &color_surface_image, gxm_color_surface->width,
                                 gxm_color_surface->height);
        }
    }

    /* Copy from the shadow depth/stencil surface to the GXM depth/stencil surface */
    if (discard_stencil) {
        dkCmdBufDiscardDepthStencil(context->cmdbuf);
    } else {
        if (dk_image_for_gxm_ds_surface(g_dk_device, &ds_surface_image, rt_width, rt_height,
                                        gxm_ds_surface)) {
            LOG("Copying depth/stencil surface: shadow -> GXM");
            dk_cmdbuf_copy_image(context->cmdbuf, &shadow_ds_surface->image,
                                 shadow_ds_surface->width, shadow_ds_surface->height,
                                 &ds_surface_image, rt_width, rt_height);
        }
    }

    /* Signal fence when rendering and copying finishes */
    dkQueueSignalFence(g_render_queue, &context->state.fragment_sync_object->fence, true);

    cmd_list = dkCmdBufFinishList(context->cmdbuf);
    dkQueueSubmitCommands(g_render_queue, cmd_list);
    dkQueueFlush(g_render_queue);

    context->state.in_scene = false;

    return 0;
}

static int SceGxmDisplayQueue_thread(SceSize args, void *argp)
{
    DisplayQueueControlBlock *queue = *(DisplayQueueControlBlock **)argp;
    DisplayQueueEntry *entry;

    ueventSignal(&queue->ready_evflag);

    while (!atomic_load(&queue->exit_thread)) {
        while (CIRC_CNT(queue->head, queue->tail, queue->num_entries) > 0) {
            if (atomic_load(&queue->exit_thread))
                break;

            /* Get the first pending framebuffer swap */
            entry = &queue->entries[queue->tail];

            /* Wait until rendering finishes */
            dkFenceWait(entry->new_fence, -1);

            /* Call the user-specified callback: this sets the new framebuffer */
            queue->display_queue_callback(entry->callback_data);

            /* Signal that the old buffer has swapped out, so it can be rendered to again */
            // dkFenceSignal(entry->old_fence);

            queue->tail = (queue->tail + 1) & (queue->num_entries - 1);
            ueventSignal(&queue->ready_evflag);
        }

        if (atomic_load(&queue->exit_thread))
            break;

        waitSingle(waiterForUEvent(&queue->pending_evflag), -1);
    }

    return 0;
}

EXPORT(SceGxm, 0xEC5C26B5, int, sceGxmDisplayQueueAddEntry, SceGxmSyncObject *oldBuffer,
       SceGxmSyncObject *newBuffer, const void *callbackData)
{
    DisplayQueueControlBlock *queue = g_display_queue;

    LOG("sceGxmDisplayQueueAddEntry: old: %p, new: %p", oldBuffer, newBuffer);

    /* Throttle down if we already have enough pending display queue entries */
    while (CIRC_CNT(queue->head, queue->tail, queue->num_entries) ==
           queue->display_queue_max_pending_count) {
        waitSingle(waiterForUEvent(&queue->ready_evflag), -1);
    }

    /* Push the fences and the callback data to the display queue */
    queue->entries[queue->head].new_fence = &newBuffer->fence;
    queue->entries[queue->head].old_fence = &oldBuffer->fence;
    memcpy(queue->entries[queue->head].callback_data, callbackData,
           queue->display_queue_callback_data_size);
    queue->head = (queue->head + 1) & (queue->num_entries - 1);

    ueventSignal(&queue->pending_evflag);

    return 0;
}

EXPORT(SceGxm, 0xB98C5B0D, int, sceGxmDisplayQueueFinish, void)
{
    DisplayQueueControlBlock *queue = g_display_queue;

    while (CIRC_CNT(queue->head, queue->tail, queue->num_entries) > 0)
        waitSingle(waiterForUEvent(&queue->ready_evflag), -1);

    return 0;
}

EXPORT(SceGxm, 0xE1CA72AE, void, sceGxmSetCullMode, SceGxmContext *context, SceGxmCullMode mode)
{
    if (!context)
        return;
    DkFace face;
    if (gxm_translate_cull(mode, &face) != 0) {
        LOG("sceGxmSetCullMode: unsupported mode 0x%x", mode);
        return;
    }
    context->state.rasterizer.cullMode = face;
    context->state.dirty.bit.rasterizer = true;
}

EXPORT(SceGxm, 0x31FF8ABD, void, sceGxmSetVertexProgram, SceGxmContext *context,
       const SceGxmVertexProgram *vertexProgram)
{
    context->state.vertex_program = vertexProgram;
    context->state.dirty.bit.vertex_shader = true;
}

EXPORT(SceGxm, 0xAD2F48D9, void, sceGxmSetFragmentProgram, SceGxmContext *context,
       const SceGxmFragmentProgram *fragmentProgram)
{
    const uint32_t mask =
        ((fragmentProgram->blendInfo.colorMask & SCE_GXM_COLOR_MASK_R) ? DkColorMask_R : 0) |
        ((fragmentProgram->blendInfo.colorMask & SCE_GXM_COLOR_MASK_G) ? DkColorMask_G : 0) |
        ((fragmentProgram->blendInfo.colorMask & SCE_GXM_COLOR_MASK_B) ? DkColorMask_B : 0) |
        ((fragmentProgram->blendInfo.colorMask & SCE_GXM_COLOR_MASK_A) ? DkColorMask_A : 0);

    dkColorWriteStateSetMask(&context->state.color_write, 0, mask);
    context->state.dirty.bit.color_write = true;
    context->state.blend = fragmentProgram->blend.state;
    dkColorStateSetBlendEnable(&context->state.color, 0, fragmentProgram->blend.enabled);
    context->state.dirty.bit.blend = true;

    context->state.fragment_program = fragmentProgram;
    context->state.dirty.bit.fragment_shader = true;
}

EXPORT(SceGxm, 0x29C34DF5, int, sceGxmSetFragmentTexture, SceGxmContext *context,
       unsigned int textureIndex, const SceGxmTexture *texture)
{
    context->state.fragment_textures[textureIndex] = *(SceGxmTextureInner *)texture;
    context->state.dirty.bit.fragment_textures = true;
    return 0;
}

EXPORT(SceGxm, 0x895DF2E9, int, sceGxmSetVertexStream, SceGxmContext *context,
       unsigned int streamIndex, const void *streamData)
{
    if (!context || !streamData)
        return SCE_GXM_ERROR_INVALID_POINTER;
    if (streamIndex >= SCE_GXM_MAX_VERTEX_STREAMS)
        return SCE_GXM_ERROR_INVALID_VALUE;
    context->state.vertex_streams[streamIndex] = streamData;
    context->state.dirty.bit.vertex_streams = true;
    return 0;
}

EXPORT(SceGxm, 0x8FA3F9C3, unsigned int, sceGxmProgramGetDefaultUniformBufferSize,
       const SceGxmProgram *program)
{
    return program->default_uniform_buffer_count * sizeof(float);
}

EXPORT(SceGxm, 0xC697CAE5, int, sceGxmSetVertexDefaultUniformBuffer, SceGxmContext *context,
       const void *uniformBuffer)
{
    VitaMemBlockInfo *block;
    uint32_t offset;

    if (!context || !uniformBuffer)
        return SCE_GXM_ERROR_INVALID_POINTER;
    block = SceSysmem_get_vita_memblock_info_for_addr(uniformBuffer);
    if (!block)
        return SCE_GXM_ERROR_INVALID_VALUE;

    offset = (uintptr_t)uniformBuffer - (uintptr_t)block->base;

    context->state.vertex_default_uniform.cpu_addr =
        dkMemBlockGetCpuAddr(block->dk_memblock) + offset;
    context->state.vertex_default_uniform.gpu_addr =
        dkMemBlockGetGpuAddr(block->dk_memblock) + offset;
    context->state.dirty.bit.vertex_default_uniform = true;

    return 0;
}

EXPORT(SceGxm, 0x97118913, int, sceGxmReserveVertexDefaultUniformBuffer, SceGxmContext *context,
       void **uniformBuffer)
{
    uint32_t size;

    if (!context->state.in_scene)
        return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
    else if (!context->state.vertex_program)
        return SCE_GXM_ERROR_NULL_PROGRAM;

    size = context->state.vertex_program->default_uniform_count * sizeof(float);
    if (size == 0) {
        *uniformBuffer = NULL;
        return 0;
    }

    if (context->state.vertex_default_uniform.allocated) {
        *uniformBuffer = context->state.vertex_default_uniform.cpu_addr;
        return 0;
    }

    if (context->state.vertex_rb.head + size > context->vertex_rb.size)
        context->state.vertex_rb.head = 0;

    *uniformBuffer = context->state.vertex_default_uniform.cpu_addr =
        dkMemBlockGetCpuAddr(context->vertex_rb.memblock) + context->state.vertex_rb.head;
    context->state.vertex_default_uniform.gpu_addr =
        dkMemBlockGetGpuAddr(context->vertex_rb.memblock) + context->state.vertex_rb.head;

    context->state.vertex_rb.head += size;
    context->state.vertex_default_uniform.allocated = true;
    context->state.dirty.bit.vertex_default_uniform = true;

    return 0;
}

EXPORT(SceGxm, 0xA824EB24, int, sceGxmSetFragmentDefaultUniformBuffer, SceGxmContext *context,
       const void *uniformBuffer)
{
    VitaMemBlockInfo *block;
    uint32_t offset;

    if (!context || !uniformBuffer)
        return SCE_GXM_ERROR_INVALID_POINTER;
    block = SceSysmem_get_vita_memblock_info_for_addr(uniformBuffer);
    if (!block)
        return SCE_GXM_ERROR_INVALID_VALUE;

    offset = (uintptr_t)uniformBuffer - (uintptr_t)block->base;

    context->state.fragment_default_uniform.cpu_addr =
        dkMemBlockGetCpuAddr(block->dk_memblock) + offset;
    context->state.fragment_default_uniform.gpu_addr =
        dkMemBlockGetGpuAddr(block->dk_memblock) + offset;
    context->state.dirty.bit.fragment_default_uniform = true;

    return 0;
}

EXPORT(SceGxm, 0x7B1FABB6, int, sceGxmReserveFragmentDefaultUniformBuffer, SceGxmContext *context,
       void **uniformBuffer)
{
    uint32_t size;

    if (!context->state.in_scene)
        return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
    else if (!context->state.fragment_program)
        return SCE_GXM_ERROR_NULL_PROGRAM;

    size = context->state.fragment_program->default_uniform_count * sizeof(float);
    if (size == 0) {
        *uniformBuffer = NULL;
        return 0;
    }

    if (context->state.fragment_default_uniform.allocated) {
        *uniformBuffer = context->state.fragment_default_uniform.cpu_addr;
        return 0;
    }

    if (context->state.fragment_rb.head + size > context->fragment_rb.size)
        context->state.fragment_rb.head = 0;

    *uniformBuffer = context->state.fragment_default_uniform.cpu_addr =
        dkMemBlockGetCpuAddr(context->fragment_rb.memblock) + context->state.fragment_rb.head;
    context->state.fragment_default_uniform.gpu_addr =
        dkMemBlockGetGpuAddr(context->fragment_rb.memblock) + context->state.fragment_rb.head;

    context->state.fragment_rb.head += size;
    context->state.fragment_default_uniform.allocated = true;
    context->state.dirty.bit.fragment_default_uniform = true;

    return 0;
}

EXPORT(SceGxm, 0x5341BD46, void *, sceGxmTextureGetData, const SceGxmTexture *texture)
{
    return gxm_texture_get_data((SceGxmTextureInner *)texture);
}

EXPORT(SceGxm, 0xE868D2B3, SceGxmTextureFormat, sceGxmTextureGetFormat,
       const SceGxmTexture *texture)
{
    return gxm_texture_get_format((SceGxmTextureInner *)texture);
}

EXPORT(SceGxm, 0xF23FCE81, SceGxmTextureGammaMode, sceGxmTextureGetGammaMode,
       const SceGxmTexture *texture)
{
    return ((SceGxmTextureInner *)texture)->gamma_mode << 27;
}

EXPORT(SceGxm, 0x5420A086, uint32_t, sceGxmTextureGetHeight, const SceGxmTexture *texture)
{
    return gxm_texture_get_height((SceGxmTextureInner *)texture);
}

EXPORT(SceGxm, 0x2DE55DA5, uint32_t, sceGxmTextureGetLodBias, const SceGxmTexture *texture)
{
    SceGxmTextureInner *inner = (SceGxmTextureInner *)texture;

    if (!texture)
        return SCE_GXM_ERROR_INVALID_POINTER;

    if (gxm_texture_get_type(inner) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
        return 0;

    return inner->lod_bias;
}

EXPORT(SceGxm, 0xBE524A2C, uint32_t, sceGxmTextureGetLodMin, const SceGxmTexture *texture)
{
    SceGxmTextureInner *inner = (SceGxmTextureInner *)texture;

    if (!texture)
        return SCE_GXM_ERROR_INVALID_POINTER;

    if (gxm_texture_get_type(inner) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
        return 0;

    return inner->lod_min0 | (inner->lod_min1 << 2);
}

EXPORT(SceGxm, 0xAE7FBB51, SceGxmTextureFilter, sceGxmTextureGetMagFilter,
       const SceGxmTexture *texture)
{
    return ((SceGxmTextureInner *)texture)->mag_filter;
}

EXPORT(SceGxm, 0x920666C6, SceGxmTextureFilter, sceGxmTextureGetMinFilter,
       const SceGxmTexture *texture)
{
    SceGxmTextureInner *inner = (SceGxmTextureInner *)texture;

    if (gxm_texture_get_type(inner) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
        return inner->mag_filter;
    return inner->min_filter;
}

EXPORT(SceGxm, 0xCE94CA15, SceGxmTextureMipFilter, sceGxmTextureGetMipFilter,
       const SceGxmTexture *texture)
{
    SceGxmTextureInner *inner = (SceGxmTextureInner *)texture;

    if (gxm_texture_get_type(inner) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
        return SCE_GXM_TEXTURE_MIP_FILTER_DISABLED;
    return inner->mip_filter ? SCE_GXM_TEXTURE_MIP_FILTER_ENABLED
                             : SCE_GXM_TEXTURE_MIP_FILTER_DISABLED;
}

EXPORT(SceGxm, 0xF7B7B1E4, uint32_t, sceGxmTextureGetMipmapCount, const SceGxmTexture *texture)
{
    SceGxmTextureInner *inner = (SceGxmTextureInner *)texture;

    if (gxm_texture_get_type(inner) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
        return 0;
    return (inner->mip_count + 1) & 0xf;
}

EXPORT(SceGxm, 0x4CC42929, uint32_t, sceGxmTextureGetMipmapCountUnsafe,
       const SceGxmTexture *texture)
{
    return (((SceGxmTextureInner *)texture)->mip_count + 1) & 0xf;
}

EXPORT(SceGxm, 0x512BB86C, int, sceGxmTextureGetNormalizeMode, const SceGxmTexture *texture)
{
    return ((SceGxmTextureInner *)texture)->normalize_mode << 31;
}

EXPORT(SceGxm, 0x0D189C30, void *, sceGxmTextureGetPalette, const SceGxmTexture *texture)
{
    SceGxmTextureBaseFormat base_format =
        gxm_texture_get_base_format(sceGxmTextureGetFormat(texture));

    return gxm_base_format_is_paletted_format(base_format) ? (void *)(texture->palette_addr << 6)
                                                           : NULL;
}

EXPORT(SceGxm, 0xB0BD52F3, uint32_t, sceGxmTextureGetStride, const SceGxmTexture *texture)
{
    SceGxmTextureInner *inner = (SceGxmTextureInner *)texture;

    if (gxm_texture_get_type(inner) != SCE_GXM_TEXTURE_LINEAR_STRIDED)
        return 0;
    return gxm_texture_get_stride_in_bytes(inner);
}

EXPORT(SceGxm, 0xF65D4917, SceGxmTextureType, sceGxmTextureGetType, const SceGxmTexture *texture)
{
    return gxm_texture_get_type((SceGxmTextureInner *)texture);
}

EXPORT(SceGxm, 0x2AE22788, SceGxmTextureAddrMode, sceGxmTextureGetUAddrMode,
       const SceGxmTexture *texture)
{
    return ((SceGxmTextureInner *)texture)->uaddr_mode;
}

EXPORT(SceGxm, 0xC037DA83, int, sceGxmTextureGetUAddrModeSafe, const SceGxmTexture *texture)
{
    SceGxmTextureInner *inner = (SceGxmTextureInner *)texture;

    if (gxm_texture_get_type(inner) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
        return SCE_GXM_TEXTURE_ADDR_CLAMP;
    return inner->uaddr_mode;
}

EXPORT(SceGxm, 0x46136CA9, SceGxmTextureAddrMode, sceGxmTextureGetVAddrMode,
       const SceGxmTexture *texture)
{
    return ((SceGxmTextureInner *)texture)->vaddr_mode;
}

EXPORT(SceGxm, 0xD2F0D9C1, int, sceGxmTextureGetVAddrModeSafe, const SceGxmTexture *texture)
{
    SceGxmTextureInner *inner = (SceGxmTextureInner *)texture;

    if (gxm_texture_get_type(inner) == SCE_GXM_TEXTURE_LINEAR_STRIDED)
        return SCE_GXM_TEXTURE_ADDR_CLAMP;
    return inner->vaddr_mode;
}

EXPORT(SceGxm, 0x126A3EB3, uint32_t, sceGxmTextureGetWidth, const SceGxmTexture *texture)
{
    return gxm_texture_get_width((SceGxmTextureInner *)texture);
}

EXPORT(SceGxm, 0x277794C4, const SceGxmProgramParameter *, sceGxmProgramFindParameterByName,
       const SceGxmProgram *program, const char *name)
{
    const uint8_t *parameter_bytes;
    const char *parameter_name;
    const SceGxmProgramParameter *const parameters =
        (const SceGxmProgramParameter *)((char *)&program->parameters_offset +
                                         program->parameters_offset);

    for (uint32_t i = 0; i < program->parameter_count; i++) {
        parameter_bytes = (const uint8_t *)&parameters[i];
        parameter_name = (const char *)(parameter_bytes + parameters[i].name_offset);
        if (strcmp(parameter_name, name) == 0)
            return (const SceGxmProgramParameter *)parameter_bytes;
    }

    return NULL;
}

EXPORT(SceGxm, 0xDBA8D061, uint32_t, sceGxmProgramParameterGetArraySize,
       const SceGxmProgramParameter *parameter)
{
    return parameter->array_size;
}

EXPORT(SceGxm, 0x1997DC17, SceGxmParameterCategory, sceGxmProgramParameterGetCategory,
       const SceGxmProgramParameter *parameter)
{
    return parameter->category;
}

EXPORT(SceGxm, 0xBD2998D1, uint32_t, sceGxmProgramParameterGetComponentCount,
       const SceGxmProgramParameter *parameter)
{
    return parameter->component_count;
}

EXPORT(SceGxm, 0xBB58267D, uint32_t, sceGxmProgramParameterGetContainerIndex,
       const SceGxmProgramParameter *parameter)
{
    return parameter->container_index;
}

EXPORT(SceGxm, 0x6E61DDF5, uint32_t, sceGxmProgramParameterGetIndex, const SceGxmProgram *program,
       const SceGxmProgramParameter *parameter)
{
    uint32_t parameter_offset = program->parameters_offset;

    if (parameter_offset > 0)
        parameter_offset += (uintptr_t)&program->parameters_offset;
    return (uint32_t)((uintptr_t)parameter - parameter_offset) >> 4;
}

EXPORT(SceGxm, 0x6AF88A5D, const char *, sceGxmProgramParameterGetName,
       const SceGxmProgramParameter *parameter)
{
    if (!parameter)
        return NULL;
    return (const char *)((uintptr_t)parameter + parameter->name_offset);
}

EXPORT(SceGxm, 0x5C79D59A, uint32_t, sceGxmProgramParameterGetResourceIndex,
       const SceGxmProgramParameter *parameter)
{
    return parameter->resource_index;
}

EXPORT(SceGxm, 0xE6D9C4CE, SceGxmParameterSemantic, sceGxmProgramParameterGetSemantic,
       const SceGxmProgramParameter *parameter)
{
    if (parameter->category != SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE)
        return SCE_GXM_PARAMETER_SEMANTIC_NONE;

    return parameter->semantic;
}

EXPORT(SceGxm, 0xB85CC13E, uint32_t, sceGxmProgramParameterGetSemanticIndex,
       const SceGxmProgramParameter *parameter)
{
    return parameter->semantic_index & 0xf;
}

EXPORT(SceGxm, 0x7B9023C3, SceGxmParameterType, sceGxmProgramParameterGetType,
       const SceGxmProgramParameter *parameter)
{
    return parameter->type;
}

static int upload_texture_mip_chain(SceGxmContext *context, const GxmTextureMipChain *chain,
                                     const void *source, size_t source_size, DkImage *image)
{
    if (!chain->count || chain->count > GXM_TEXTURE_MAX_MIP_LEVELS ||
        source_size < chain->source_size) return SCE_GXM_ERROR_INVALID_VALUE;
    DkMemBlock levels[GXM_TEXTURE_MAX_MIP_LEVELS];
    for (unsigned i = 0; i < chain->count; ++i) {
        if (chain->offsets[i] > source_size ||
            chain->levels[i].source_size > source_size - chain->offsets[i])
            return SCE_GXM_ERROR_INVALID_VALUE;
        int ret = gxm_texture_stage(g_dk_device, &context->texture_staging, &chain->levels[i],
            (const char *)source + chain->offsets[i], source_size - chain->offsets[i], &levels[i]);
        if (ret) return ret;
    }
    DkImageLayoutMaker maker;
    DkImageLayout layout;
    dkImageLayoutMakerDefaults(&maker, g_dk_device);
    maker.format = chain->levels[0].format;
    maker.dimensions[0] = chain->levels[0].width;
    maker.dimensions[1] = chain->levels[0].height;
    maker.mipLevels = chain->count;
    /* Pitch-linear images cannot represent a mip chain. Use native tiled
     * storage and copy each packed linear level into its own image view. */
    dkImageLayoutInitialize(&layout, &maker);
    DkMemBlock destination;
    int ret = gxm_texture_allocate_image(g_dk_device, &context->texture_staging,
                                          dkImageLayoutGetSize(&layout), &destination);
    if (ret) return ret;
    dkImageInitialize(image, &layout, destination, 0);
    dkCmdBufBarrier(context->cmdbuf, DkBarrier_Full, DkInvalidateFlags_L2Cache);
    for (unsigned i = 0; i < chain->count; ++i) {
        const GxmTextureLayout *level = &chain->levels[i];
        DkImageView view;
        dkImageViewDefaults(&view, image);
        view.mipLevelOffset = i;
        view.mipLevelCount = 1;
        DkImageRect rect = {0, 0, 0, level->width, level->height, 1};
        /* In this deko3d fork, rowLength and imageHeight are byte strides. */
        DkCopyBuf buffer = {dkMemBlockGetGpuAddr(levels[i]), level->pitch, level->size};
        dkCmdBufCopyBufferToImage(context->cmdbuf, &buffer, &view, &rect, 0);
    }
    return 0;
}

static int upload_fragment_texture_descriptors(SceGxmContext *context)
{
    const SceGxmTextureInner *texture;
    VitaMemBlockInfo *tex_block;
    void *tex_data;
    DkSampler sampler;
    DkImageLayoutMaker image_layout_maker;
    DkImageLayout image_layout;
    DkImage image;
    DkImageView image_view;
    DkGpuAddr desc_addr;
    GxmTextureLayout texture_layout;
    GxmTextureMipChain mip_chain;
    int ret;
    bool staged = false;
    struct {
        DkImageDescriptor images[SCE_GXM_MAX_TEXTURE_UNITS];
        DkSamplerDescriptor samplers[SCE_GXM_MAX_TEXTURE_UNITS];
    } descriptors = {0};

    for (int i = 0; i < SCE_GXM_MAX_TEXTURE_UNITS; i++) {
        texture = &context->state.fragment_textures[i];
        if (!texture->data_addr)
            continue;

        tex_data = gxm_texture_get_data(texture);
        tex_block = SceSysmem_get_vita_memblock_info_for_addr(tex_data);
        if (!tex_block)
            return SCE_GXM_ERROR_INVALID_POINTER;

        ret = gxm_texture_get_mip_chain(texture, &mip_chain);
        if (ret != 0) {
            LOG("Unsupported texture at unit %d: format 0x%08x, type 0x%08x", i,
                (unsigned int)gxm_texture_get_format(texture), gxm_texture_get_type(texture));
            return ret;
        }
        texture_layout = mip_chain.levels[0];

        dkSamplerDefaults(&sampler);
        sampler.wrapMode[0] = gxm_texture_addr_mode_to_dk_wrap_mode(texture->uaddr_mode);
        sampler.wrapMode[1] = gxm_texture_addr_mode_to_dk_wrap_mode(texture->vaddr_mode);
        sampler.minFilter = gxm_texture_filter_to_dk_filter(
            gxm_texture_get_type(texture) == SCE_GXM_TEXTURE_LINEAR_STRIDED
                ? texture->mag_filter : texture->min_filter);
        sampler.magFilter = gxm_texture_filter_to_dk_filter(texture->mag_filter);
        sampler.mipFilter = mip_chain.count == 1 ? DkMipFilter_None :
            (texture->mip_filter ? DkMipFilter_Linear : DkMipFilter_Nearest);
        sampler.lodClampMax = mip_chain.count - 1;
        if (gxm_texture_get_type(texture) != SCE_GXM_TEXTURE_LINEAR_STRIDED) {
            unsigned minimum = texture->lod_min0 | (texture->lod_min1 << 2);
            sampler.lodClampMin = minimum < mip_chain.count ? minimum : mip_chain.count - 1;
            sampler.lodBias = ((float)texture->lod_bias - 31.0f) / 8.0f;
        }
        dkSamplerDescriptorInitialize(&descriptors.samplers[i], &sampler);

        uint32_t offset = dk_memblock_cpu_addr_offset(tex_block->dk_memblock, tex_data);
        if (offset > tex_block->size || mip_chain.source_size > tex_block->size - offset)
            return SCE_GXM_ERROR_INVALID_VALUE;
        if (mip_chain.count > 1) {
            ret = upload_texture_mip_chain(context, &mip_chain, tex_data, tex_block->size - offset, &image);
            if (ret) return ret;
            staged = true;
        } else {
            dkImageLayoutMakerDefaults(&image_layout_maker, g_dk_device);
            image_layout_maker.flags = DkImageFlags_PitchLinear;
            image_layout_maker.type = DkImageType_2D;
            image_layout_maker.format = texture_layout.format;
            image_layout_maker.dimensions[0] = texture_layout.width;
            image_layout_maker.dimensions[1] = texture_layout.height;
            image_layout_maker.pitchStride = texture_layout.pitch;
            dkImageLayoutInitialize(&image_layout, &image_layout_maker);

            DkMemBlock image_block = tex_block->dk_memblock;
            const uint32_t type = gxm_texture_get_type(texture);
            if ((type != SCE_GXM_TEXTURE_LINEAR && type != SCE_GXM_TEXTURE_LINEAR_STRIDED) ||
                texture_layout.pitch != texture_layout.source_pitch ||
                offset % dkImageLayoutGetAlignment(&image_layout) != 0) {
                ret = gxm_texture_stage(g_dk_device, &context->texture_staging, &texture_layout,
                                         tex_data, tex_block->size - offset, &image_block);
                if (ret != 0)
                    return ret;
                offset = 0;
                staged = true;
            } else if (dkImageLayoutGetSize(&image_layout) > tex_block->size - offset) {
                return SCE_GXM_ERROR_INVALID_VALUE;
            }
            dkImageInitialize(&image, &image_layout, image_block, offset);
        }
        dkImageViewDefaults(&image_view, &image);
        memcpy(image_view.swizzle, texture_layout.swizzle, sizeof(image_view.swizzle));
        dkImageDescriptorInitialize(&descriptors.images[i], &image_view, false, false);
        dkCmdBufBindTexture(context->cmdbuf, DkStage_Fragment, i, dkMakeTextureHandle(i, i));
    }

    /* Each recorded update owns immutable descriptor storage until scene
     * retirement. Reusing one table could change pending draws' textures. */
    DkMemBlock descriptor_block;
    ret = gxm_texture_stage_data(g_dk_device, &context->texture_staging,
                                  &descriptors, sizeof(descriptors), &descriptor_block);
    if (ret) return ret;
    dkCmdBufBarrier(context->cmdbuf, DkBarrier_Full,
                      DkInvalidateFlags_Descriptors | DkInvalidateFlags_L2Cache |
                      (staged ? DkInvalidateFlags_Image : 0));
    desc_addr = dkMemBlockGetGpuAddr(descriptor_block);
    dkCmdBufBindImageDescriptorSet(context->cmdbuf, desc_addr, SCE_GXM_MAX_TEXTURE_UNITS);
    dkCmdBufBindSamplerDescriptorSet(
        context->cmdbuf, desc_addr + SCE_GXM_MAX_TEXTURE_UNITS * sizeof(DkImageDescriptor),
        SCE_GXM_MAX_TEXTURE_UNITS);
    return 0;
}

EXPORT(SceGxm, 0xC68015E4, int, sceGxmSetVertexUniformBuffer, SceGxmContext *context,
       unsigned int bufferIndex, const void *bufferData)
{
    if (!context || !bufferData) return SCE_GXM_ERROR_INVALID_POINTER;
    if (bufferIndex >= SCE_GXM_MAX_UNIFORM_BUFFERS) return SCE_GXM_ERROR_INVALID_VALUE;
    context->state.vertex_uniform_buffers[bufferIndex] = bufferData;
    return 0;
}

EXPORT(SceGxm, 0xEA0FC310, int, sceGxmSetFragmentUniformBuffer, SceGxmContext *context,
       unsigned int bufferIndex, const void *bufferData)
{
    if (!context || !bufferData) return SCE_GXM_ERROR_INVALID_POINTER;
    if (bufferIndex >= SCE_GXM_MAX_UNIFORM_BUFFERS) return SCE_GXM_ERROR_INVALID_VALUE;
    context->state.fragment_uniform_buffers[bufferIndex] = bufferData;
    return 0;
}

static int stage_uniforms(SceGxmContext *context, DkStage stage,
                          const GxmUniformLayout *layout, const void *default_buffer,
                          const void *const named_buffers[SCE_GXM_MAX_UNIFORM_BUFFERS])
{
    GxmUniformSource sources[GXM_UNIFORM_LAYOUT_SLOTS] = {0};
    for (unsigned i = 0; i < GXM_UNIFORM_LAYOUT_SLOTS; ++i) {
        if (!layout->bytes[i]) continue;
        const void *data = i ? named_buffers[i - 1] : default_buffer;
        if (!data) return SCE_GXM_ERROR_INVALID_POINTER;
        VitaMemBlockInfo *block = SceSysmem_get_vita_memblock_info_for_addr(data);
        if (!block) return SCE_GXM_ERROR_INVALID_VALUE;
        uintptr_t offset = (uintptr_t)data - (uintptr_t)block->base;
        if (offset >= block->size || layout->bytes[i] > block->size - offset)
            return SCE_GXM_ERROR_INVALID_VALUE;
        void *cpu = dkMemBlockGetCpuAddr(block->dk_memblock);
        if (!cpu) return SCE_GXM_ERROR_INVALID_POINTER;
        sources[i] = (GxmUniformSource){(const char *)cpu + offset, block->size - offset};
    }
    DkMemBlock snapshot;
    int ret = gxm_uniform_stage(g_dk_device, &context->uniform_staging, layout, sources, &snapshot);
    if (ret) return ret;
    dkCmdBufBindStorageBuffer(context->cmdbuf, stage, stage == DkStage_Vertex ? 0 : 1,
                              snapshot ? dkMemBlockGetGpuAddr(snapshot) : 0, layout->size);
    return 0;
}

static int context_flush_dirty_state(SceGxmContext *context)
{
    if (context->state.dirty.bit.viewport)
        set_vita3k_gxm_uniform_blocks(context, context->state.color_surface->width,
                                      context->state.color_surface->height);
    const DkShader *shaders[2];
    const SceGxmVertexProgram *vertex_program = context->state.vertex_program;
    const SceGxmFragmentProgram *fragment_program = context->state.fragment_program;
    const SceGxmVertexAttribute *attributes = vertex_program->attributes;
    const SceGxmVertexStream *streams = vertex_program->streams;
    DkVtxAttribState vertex_attrib_state[SCE_GXM_MAX_VERTEX_ATTRIBUTES];
    DkVtxBufferState vertex_buffer_state[SCE_GXM_MAX_VERTEX_STREAMS];
    uint32_t i, shader_count = 0, shader_stage_mask = 0;

    if (context->state.dirty.bit.vertex_streams || context->state.dirty.bit.vertex_shader) {
        uint32_t used_streams = 0;
        for (i = 0; i < vertex_program->attributeCount; ++i)
            used_streams |= 1U << attributes[i].streamIndex;
        for (i = 0; i < vertex_program->streamCount; ++i) {
            if (!(used_streams & (1U << i)))
                continue;
            const void *data = context->state.vertex_streams[i];
            if (!data)
                return SCE_GXM_ERROR_INVALID_POINTER;
            VitaMemBlockInfo *block = SceSysmem_get_vita_memblock_info_for_addr(data);
            if (!block)
                return SCE_GXM_ERROR_INVALID_VALUE;
            uintptr_t offset = (uintptr_t)data - (uintptr_t)block->base;
            dkCmdBufBindVtxBuffer(context->cmdbuf, i,
                                  dkMemBlockGetGpuAddr(block->dk_memblock) + offset,
                                  block->size - offset);
        }
    }

    if (context->state.dirty.bit.vertex_shader && vertex_program) {
        memset(vertex_attrib_state, 0,
               vertex_program->attributeCount * sizeof(*vertex_attrib_state));
        for (i = 0; i < vertex_program->attributeCount; i++) {
            vertex_attrib_state[i].bufferId = attributes[i].streamIndex;
            vertex_attrib_state[i].isFixed = 0;
            vertex_attrib_state[i].offset = attributes[i].offset;
            vertex_attrib_state[i].size =
                gxm_to_dk_vtx_attrib_size(attributes[i].format, attributes[i].componentCount);
            vertex_attrib_state[i].type = gxm_to_dk_vtx_attrib_type(attributes[i].format);
            vertex_attrib_state[i].isBgra = 0;
        }

        for (i = 0; i < vertex_program->streamCount; i++) {
            vertex_buffer_state[i].stride = streams[i].stride;
            uint32_t divisor;
            int ret = gxm_stream_divisor((SceGxmIndexSource)streams[i].indexSource, &divisor);
            if (ret != 0)
                return ret;
            vertex_buffer_state[i].divisor = divisor;
        }

        dkCmdBufBindVtxAttribState(context->cmdbuf, vertex_attrib_state,
                                   vertex_program->attributeCount);
        dkCmdBufBindVtxBufferState(context->cmdbuf, vertex_buffer_state,
                                   vertex_program->streamCount);
    }

    if (context->state.dirty.bit.vertex_shader || context->state.dirty.bit.fragment_shader) {
        if (vertex_program) {
            shaders[shader_count++] = &vertex_program->dk_shader;
            shader_stage_mask |= DkStageFlag_Vertex;
        }
        if (fragment_program) {
            shaders[shader_count++] = &fragment_program->dk_shader;
            shader_stage_mask |= DkStageFlag_Fragment;
        }

        /* Always replaces the entire shader pipeline. Unspecified stages are disabled. */
        dkCmdBufBindShaders(context->cmdbuf, shader_stage_mask, shaders, shader_count);
    }

    if (context->state.dirty.bit.depth_stencil)
        dkCmdBufBindDepthStencilState(context->cmdbuf, &context->state.depth_stencil);

    if (context->state.dirty.bit.rasterizer)
        dkCmdBufBindRasterizerState(context->cmdbuf, &context->state.rasterizer);

    if (context->state.dirty.bit.front_stencil) {
        dkCmdBufSetStencil(context->cmdbuf, DkFace_Front, context->state.front_stencil.write_mask,
                           context->state.front_stencil.ref,
                           context->state.front_stencil.compare_mask);
    }

    if (context->state.dirty.bit.back_stencil) {
        dkCmdBufSetStencil(context->cmdbuf, DkFace_Back, context->state.back_stencil.write_mask,
                           context->state.back_stencil.ref,
                           context->state.back_stencil.compare_mask);
    }

    if (context->state.dirty.bit.color_write)
        dkCmdBufBindColorWriteState(context->cmdbuf, &context->state.color_write);

    if (context->state.dirty.bit.blend) {
        dkCmdBufBindColorState(context->cmdbuf, &context->state.color);
        dkCmdBufBindBlendStates(context->cmdbuf, 0, &context->state.blend, 1);
    }

    if (context->state.dirty.bit.fragment_textures) {
        int ret = upload_fragment_texture_descriptors(context);
        if (ret != 0)
            return ret;
    }

    /* Snapshot at every draw: callers may modify a buffer without rebinding it.
     * Distinct storage also protects queued draws when guest ringbuffers wrap. */
    int ret = stage_uniforms(context, DkStage_Vertex, &vertex_program->uniform_layout,
                             context->state.vertex_default_uniform.cpu_addr,
                             context->state.vertex_uniform_buffers);
    if (ret) return ret;
    ret = stage_uniforms(context, DkStage_Fragment, &fragment_program->uniform_layout,
                         context->state.fragment_default_uniform.cpu_addr,
                         context->state.fragment_uniform_buffers);
    if (ret) return ret;
    context->state.vertex_default_uniform.allocated = false;
    context->state.fragment_default_uniform.allocated = false;

    /* We have flushed all the dirty state */
    context->state.dirty.raw = 0;
    return 0;
}

static int draw_indexed(SceGxmContext *context, SceGxmPrimitiveType primType,
                        SceGxmIndexFormat indexType, const void *indexData,
                        unsigned int indexCount, unsigned int instances)
{
    VitaMemBlockInfo *index_block;
    uint32_t index_offset;
    DkPrimitive primitive;
    int ret;

    LOG("sceGxmDraw: primType: 0x%x, indexCount: %d", primType, indexCount);

    if (!context || !indexData)
        return SCE_GXM_ERROR_INVALID_POINTER;
    if (!context->state.in_scene)
        return SCE_GXM_ERROR_NOT_WITHIN_SCENE;
    if (!context->state.vertex_program || !context->state.fragment_program)
        return SCE_GXM_ERROR_NULL_PROGRAM;

    /* 16-bit stream index sources wrap at 65536. Native divisor fetches do
     * not wrap; reject that case until a wrapping fetch path is implemented. */
    const SceGxmVertexProgram *program = context->state.vertex_program;
    for (unsigned int i = 0; i < program->streamCount; ++i) {
        if (program->streams[i].indexSource == SCE_GXM_INDEX_SOURCE_INSTANCE_16BIT &&
            instances > 65536)
            return SCE_GXM_ERROR_INVALID_VALUE;
    }

    ret = gxm_translate_primitive(primType, &primitive);
    if (ret != 0)
        return ret;

    index_block = SceSysmem_get_vita_memblock_info_for_addr(indexData);
    if (!index_block)
        return SCE_GXM_ERROR_INVALID_VALUE;

    ret = gxm_validate_index_range(indexType, (uintptr_t)indexData,
                                  (uintptr_t)index_block->base, index_block->size,
                                  indexCount, &index_offset);
    if (ret != 0)
        return ret;

    ret = context_flush_dirty_state(context);
    if (ret != 0)
        return ret;

    dkCmdBufBindIdxBuffer(context->cmdbuf, gxm_to_dk_idx_format(indexType),
                          dkMemBlockGetGpuAddr(index_block->dk_memblock) + index_offset);
    DkScissor scissors[4];
    unsigned scissor_count = gxm_region_clip_scissors(&context->state.region_clip,
        context->state.render_target->params.width, context->state.render_target->params.height,
        scissors);
    for (unsigned i = 0; i < scissor_count; ++i) {
        dkCmdBufSetScissors(context->cmdbuf, 0, &scissors[i], 1);
        dkCmdBufDrawIndexed(context->cmdbuf, primitive, indexCount, instances, 0, 0, 0);
    }

    return 0;
}

EXPORT(SceGxm, 0xBC059AFC, int, sceGxmDraw, SceGxmContext *context, SceGxmPrimitiveType primType,
       SceGxmIndexFormat indexType, const void *indexData, unsigned int indexCount)
{
    return draw_indexed(context, primType, indexType, indexData, indexCount, 1);
}

EXPORT(SceGxm, 0x14C4E7D3, int, sceGxmDrawInstanced, SceGxmContext *context,
       SceGxmPrimitiveType primType, SceGxmIndexFormat indexType, const void *indexData,
       unsigned int indexCount, unsigned int indexWrap)
{
    uint32_t indices, instances;
    int ret = gxm_instanced_counts(indexCount, indexWrap, &indices, &instances);
    if (ret != 0) return ret;
    return draw_indexed(context, primType, indexType, indexData, indices, instances);
}

EXPORT(SceGxm, 0xC61E34FC, int, sceGxmMapMemory, void *base, SceSize size,
       SceGxmMemoryAttribFlags attr)
{
    VitaMemBlockInfo *block;

    /* By default, SceSysmem maps all the allocated blocks to the GPU.
     * Here we just check if it exists */
    block = SceSysmem_get_vita_memblock_info_for_addr(base);
    if (!block)
        return SCE_GXM_ERROR_INVALID_POINTER;

    return 0;
}

EXPORT(SceGxm, 0x828C68E8, int, sceGxmUnmapMemory, void *base)
{
    VitaMemBlockInfo *block = SceSysmem_get_vita_memblock_info_for_addr(base);
    if (!block)
        return SCE_GXM_ERROR_INVALID_POINTER;

    return 0;
}

EXPORT(SceGxm, 0xFA437510, int, sceGxmMapVertexUsseMemory, void *base, SceSize size,
       unsigned int *offset)
{
    return 0;
}

EXPORT(SceGxm, 0x099134F5, int, sceGxmUnmapVertexUsseMemory, void *base)
{
    return 0;
}

EXPORT(SceGxm, 0x008402C6, int, sceGxmMapFragmentUsseMemory, void *base, SceSize size,
       unsigned int *offset)
{
    return 0;
}

EXPORT(SceGxm, 0x80CCEDBB, int, sceGxmUnmapFragmentUsseMemor, void *base)
{
    return 0;
}

DECLARE_LIBRARY(SceGxm, 0xf76b66bd);

int SceGxm_init(DkDevice dk_device)
{
    g_dk_device = dk_device;

    return 0;
}

int SceGxm_finish(void)
{
    return g_gxm_initialized ? sceGxmTerminate() : 0;
}
