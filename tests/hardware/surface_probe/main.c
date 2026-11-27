/* Freestanding Vita guests with no libc startup. The surface variant performs
 * no draws; draw/cube variants embed the pinned shader fixtures. */
#include <psp2/gxm.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/processmgr.h>
#include <stdint.h>
#include <stddef.h>
#ifdef VITA2HOS_DRAW_PROBE
#include "draw_probe.h"
#ifdef VITA2HOS_VISIBLE_CUBE
#define PROBE_NAME "visible_cube"
#define PROBE_VERSION "9"
#define PROBE_REPORT "app0:/visible-cube.txt"
#else
#define PROBE_NAME "draw_probe"
#define PROBE_VERSION "12"
#define PROBE_REPORT "app0:/draw-probe.txt"
#endif
#else
#define PROBE_NAME "surface_probe"
#define PROBE_VERSION "3"
#define PROBE_REPORT "app0:/surface-probe.txt"
#endif

enum { WIDTH = 64, HEIGHT = 64, PIXELS = WIDTH * HEIGHT };
static SceUID allocations[32];
static unsigned allocation_count;
static SceUID report = -1;
#ifdef VITA2HOS_VISIBLE_CUBE
static unsigned char *ring_guards[4];
enum { RING_GUARD_SIZE = 4096 };
#endif

void *memset(void *destination, int value, size_t size)
{
    unsigned char *p = destination;
    for (size_t i = 0; i < size; ++i) p[i] = value;
    return destination;
}

void *memcpy(void *destination, const void *source, size_t size)
{
    unsigned char *d = destination;
    const unsigned char *s = source;
    for (size_t i = 0; i < size; ++i) d[i] = s[i];
    return destination;
}

static void record(const char *text)
{
    unsigned length = 0;
    while (text[length]) ++length;
    while (length && report >= 0) {
        int count = sceIoWrite(report, text, length);
        if (count <= 0) { report = -1; return; }
        text += count;
        length -= count;
    }
}

static void result(const char *name, int code)
{
    static const char digits[] = "0123456789abcdef";
    char hex[12] = "0x00000000\n";
    for (unsigned i = 0; i < 8; ++i) hex[2 + i] = digits[((uint32_t)code >> (28 - 4 * i)) & 15];
    record(name); record(" "); record(hex);
}

static void *allocate(unsigned bytes)
{
    if (allocation_count == sizeof(allocations) / sizeof(allocations[0])) return NULL;
    SceUID uid = sceKernelAllocMemBlock("surface_probe", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
                                       (bytes + 4095u) & ~4095u, NULL);
    if (uid < 0) return NULL;
    allocations[allocation_count++] = uid;
    void *base = NULL;
    if (sceKernelGetMemBlockBase(uid, &base) < 0) return NULL;
    return base;
}

static uint32_t pattern(unsigned index, unsigned seed)
{
    return 0xff000000u | (((index * 3 + seed) & 255u) << 16) |
           (((index * 5 + seed * 7) & 255u) << 8) | ((index * 11 + seed * 13) & 255u);
}

static void display_callback(const void *data) { (void)data; }

int module_start(SceSize args, void *argp)
{
    (void)args; (void)argp;
    report = sceIoOpen(PROBE_REPORT, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
    if (report < 0) { sceKernelExitProcess(1); return 1; }
    record(PROBE_NAME " v" PROBE_VERSION "\nRUNNING - no hardware result until a final PASS line\n");
    int status = 1, initialized = 0, code;
    SceGxmContext *context = NULL;
    SceGxmRenderTarget *target = NULL, *borrowed_target = NULL;
    SceGxmInitializeParams init = {0};
    init.displayQueueMaxPendingCount = 1;
    init.displayQueueCallback = display_callback;
    init.parameterBufferSize = SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE;
    code = sceGxmInitialize(&init);
    result("initialize", code);
    if (code) goto done;
    initialized = 1;

    SceGxmContextParams params = {0};
    params.hostMemSize = SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE;
    params.hostMem = allocate(params.hostMemSize);
    params.vdmRingBufferMemSize = SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE;
#ifndef VITA2HOS_VISIBLE_CUBE
    params.vdmRingBufferMem = allocate(params.vdmRingBufferMemSize);
#endif
    params.vertexRingBufferMemSize = SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE;
#ifndef VITA2HOS_VISIBLE_CUBE
    params.vertexRingBufferMem = allocate(params.vertexRingBufferMemSize);
#endif
    params.fragmentRingBufferMemSize = SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE;
#ifndef VITA2HOS_VISIBLE_CUBE
    params.fragmentRingBufferMem = allocate(params.fragmentRingBufferMemSize);
#endif
    params.fragmentUsseRingBufferMemSize = SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE;
    void *usse = allocate(params.fragmentUsseRingBufferMemSize);
    uint32_t *buffers[2] = {allocate(PIXELS * 4), allocate(PIXELS * 4)};
#ifdef VITA2HOS_VISIBLE_CUBE
    /* Put all three rings in one allocation, each behind a page-sized guard.
     * Correct GPU and CPU addressing must retain their distinct offsets. */
    unsigned slab_size = params.vdmRingBufferMemSize + params.vertexRingBufferMemSize +
                         params.fragmentRingBufferMemSize + 4 * RING_GUARD_SIZE;
    unsigned char *slab = allocate(slab_size);
    if (!slab) { record("FAIL ring_slab_allocation\n"); goto done; }
    ring_guards[0] = slab;
    params.vdmRingBufferMem = slab + RING_GUARD_SIZE;
    ring_guards[1] = (unsigned char *)params.vdmRingBufferMem + params.vdmRingBufferMemSize;
    params.vertexRingBufferMem = ring_guards[1] + RING_GUARD_SIZE;
    ring_guards[2] = (unsigned char *)params.vertexRingBufferMem + params.vertexRingBufferMemSize;
    params.fragmentRingBufferMem = ring_guards[2] + RING_GUARD_SIZE;
    ring_guards[3] = (unsigned char *)params.fragmentRingBufferMem + params.fragmentRingBufferMemSize;
    for (unsigned i=0;i<4;++i) memset(ring_guards[i],0xa5,RING_GUARD_SIZE);
    record("RUN interior_ring_buffers guarded_shared_allocation\n");
#endif
    if (!params.hostMem || !params.vdmRingBufferMem || !params.vertexRingBufferMem ||
        !params.fragmentRingBufferMem || !usse || !buffers[0] || !buffers[1]) {
        record("FAIL allocation\n"); goto done;
    }
    code = sceGxmMapFragmentUsseMemory(usse, params.fragmentUsseRingBufferMemSize,
                                      &params.fragmentUsseRingBufferOffset);
    if (code) { result("FAIL map_usse", code); goto done; }
    code = sceGxmCreateContext(&params, &context);
    if (code) { result("FAIL create_context", code); goto done; }
    SceGxmRenderTargetParams target_params = {0};
    target_params.driverMemBlock = -1; /* Request libgxm-owned driver memory. */
    target_params.width = WIDTH; target_params.height = HEIGHT; target_params.scenesPerFrame = 1;
    code = sceGxmCreateRenderTarget(&target_params, &target);
    if (code) { result("FAIL create_target", code); goto done; }
    SceUID driver_uid = -1;
    void *driver_base = NULL;
    code = sceGxmRenderTargetGetDriverMemBlock(target, &driver_uid);
    if (code || driver_uid <= 0 || sceKernelGetMemBlockBase(driver_uid, &driver_base) || !driver_base) {
        result("FAIL owned_driver_memory", code); goto done;
    }
    unsigned driver_size = 0;
    code = sceGxmGetRenderTargetMemSize(&target_params, &driver_size);
    if (code || !driver_size || (driver_size & 4095)) {
        result("FAIL driver_memory_size", code); goto done;
    }
    void *borrowed_base = allocate(driver_size);
    if (!borrowed_base) { record("FAIL borrowed_driver_allocate\n"); goto done; }
    SceUID borrowed_uid = allocations[allocation_count - 1];
    target_params.driverMemBlock = borrowed_uid;
    code = sceGxmCreateRenderTarget(&target_params, &borrowed_target);
    if (!code) code = sceGxmRenderTargetGetDriverMemBlock(borrowed_target, &driver_uid);
    if (code || driver_uid != borrowed_uid) { result("FAIL borrowed_driver_memory", code); goto done; }
    code = sceGxmDestroyRenderTarget(borrowed_target);
    if (code) { result("FAIL borrowed_driver_destroy", code); goto done; }
    borrowed_target = NULL;driver_base = NULL;
    if (sceKernelGetMemBlockBase(borrowed_uid, &driver_base) || driver_base != borrowed_base) {
        record("FAIL borrowed_driver_lifetime\n"); goto done;
    }
    record("PASS render_target_driver_memory\n");
    SceGxmDepthStencilSurface disabled;
    code = sceGxmDepthStencilSurfaceInitDisabled(&disabled);
    if (code || sceGxmDepthStencilSurfaceIsEnabled(&disabled)) {
        result("FAIL disabled_surface", code); goto done;
    }
    SceGxmColorSurface surfaces[2];
    for (unsigned b = 0; b < 2; ++b) {
        for (unsigned i = 0; i < PIXELS; ++i) buffers[b][i] = pattern(i, b + 1);
        code = sceGxmColorSurfaceInit(&surfaces[b], SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR,
            SCE_GXM_COLOR_SURFACE_LINEAR, SCE_GXM_COLOR_SURFACE_SCALE_NONE,
            SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT, WIDTH, HEIGHT, WIDTH, buffers[b]);
        if (code) { result("FAIL init_color", code); goto done; }
    }
    if (sceGxmEndScene(context, NULL, NULL) != (int)SCE_GXM_ERROR_NOT_WITHIN_SCENE) {
        record("FAIL end_without_begin\n"); goto done;
    }
    volatile unsigned int *region = sceGxmGetNotificationRegion();
    if (!region) { record("FAIL notification_region\n"); goto done; }
    for (unsigned pass = 0; pass < 3; ++pass) {
        unsigned selected = pass & 1;
        region[0] = region[1] = 0;
        SceGxmNotification vertex = {region, 0x12340000u + pass};
        SceGxmNotification fragment = {region + 1, 0x56780000u + pass};
        SceGxmNotification invalid = {region + 512, 0};
        code = sceGxmBeginScene(context, 0, target, NULL, NULL, NULL, &surfaces[selected], &disabled);
        if (code) { result("FAIL begin", code); goto done; }
        code = sceGxmEndScene(context, &vertex, &invalid);
        if (code != (int)SCE_GXM_ERROR_INVALID_POINTER) {
            result("FAIL invalid_notification", code); goto done;
        }
        code = sceGxmEndScene(context, &vertex, &fragment);
        if (code) { result("FAIL end", code); goto done; }
        code = sceGxmNotificationWait(&fragment);
        if (code) { result("FAIL notification_wait", code); goto done; }
        sceGxmFinish(context);
        if (region[0] != vertex.value || region[1] != fragment.value) {
            record("FAIL notification_values\n"); goto done;
        }
        for (unsigned b = 0; b < 2; ++b) {
            for (unsigned i = 0; i < PIXELS; ++i) {
                if (buffers[b][i] != pattern(i, b + 1)) {
                    result("FAIL pass", pass); result("buffer", b); result("pixel", i);
                    result("actual", buffers[b][i]); result("expected", pattern(i, b + 1));
                    goto done;
                }
            }
        }
        result("PASS framebuffer_switch", pass);
    }
    /* A depth-only scene must also tolerate no color surface and no sync object. */
    code = sceGxmBeginScene(context, 0, target, NULL, NULL, NULL, NULL, &disabled);
    if (!code) code = sceGxmEndScene(context, NULL, NULL);
    if (code) { result("FAIL null_color_scene", code); goto done; }
    sceGxmFinish(context);
    record("PASS null_color_scene\n");
    /* Exercise sample replication, resolve and state restoration in one run.
     * No draws occur here: matching pixels prove transfer preservation only,
     * not antialiased coverage or sample positions. The draw/cube tests follow. */
    const SceGxmMultisampleMode modes[3] = {
        SCE_GXM_MULTISAMPLE_2X,SCE_GXM_MULTISAMPLE_4X,SCE_GXM_MULTISAMPLE_NONE};
    for (unsigned pass=0;pass<3;++pass) {
        target_params.driverMemBlock=-1;
        target_params.multisampleMode=modes[pass];
        result("RUN multisample_mode",modes[pass]);
        code=sceGxmCreateRenderTarget(&target_params,&borrowed_target);
        if (code) { result("FAIL multisample_create",code);goto done; }
        SceGxmColorSurface ms_color;
        unsigned selected=pass&1;
        code=sceGxmColorSurfaceInit(&ms_color,SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR,
            SCE_GXM_COLOR_SURFACE_LINEAR,modes[pass]==SCE_GXM_MULTISAMPLE_NONE ?
                SCE_GXM_COLOR_SURFACE_SCALE_NONE : SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE,
            SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,WIDTH,HEIGHT,WIDTH,buffers[selected]);
        if (!code) code=sceGxmBeginScene(context,0,borrowed_target,NULL,NULL,NULL,&ms_color,&disabled);
        if (!code) code=sceGxmEndScene(context,NULL,NULL);
        if (code) { result("FAIL multisample_scene",code);goto done; }
        sceGxmFinish(context);
        for (unsigned b=0;b<2;++b) {
            for (unsigned i=0;i<PIXELS;++i) {
                if (buffers[b][i]!=pattern(i,b+1)) {
                    result("FAIL multisample_pixel",i);result("mode",modes[pass]);
                    result("buffer",b);result("actual",buffers[b][i]);
                    result("expected",pattern(i,b+1));goto done;
                }
            }
        }
        code=sceGxmDestroyRenderTarget(borrowed_target);
        if (code) { result("FAIL multisample_destroy",code);goto done; }
        borrowed_target=NULL;
        record(pass==0 ? "PASS multisample_2x_preservation\n" : pass==1 ?
            "PASS multisample_4x_preservation\n" : "PASS multisample_restore_1x\n");
    }
#ifdef VITA2HOS_DRAW_PROBE
    const ProbeHooks hooks = {allocate, record, result};
    code = probe_draw(context, target, &surfaces[0], buffers[0], &hooks);
    if (code) { result("FAIL draw_tests", code); goto done; }
#endif
    status = 0;
done:
    if (borrowed_target) sceGxmDestroyRenderTarget(borrowed_target);
    if (context) sceGxmDestroyContext(context);
    if (target) sceGxmDestroyRenderTarget(target);
    if (initialized) sceGxmTerminate();
#ifndef VITA2HOS_VISIBLE_CUBE
    while (allocation_count) sceKernelFreeMemBlock(allocations[--allocation_count]);
#endif
#ifdef VITA2HOS_VISIBLE_CUBE
    if (!status) {
        for (unsigned i=0;i<4;++i) for (unsigned j=0;j<RING_GUARD_SIZE;++j) {
            if (ring_guards[i][j]!=0xa5) {
                result("FAIL ring_guard_index",i); result("ring_guard_byte",j);
                status=1; goto ring_guards_done;
            }
        }
        record("PASS interior_ring_buffers_guards\n");
    }
ring_guards_done:
#endif
    record(status ? "FAIL " PROBE_NAME "\n" : "PASS " PROBE_NAME "\n");
    if (report >= 0) sceIoClose(report);
    sceKernelExitProcess(status);
    return status;
}
