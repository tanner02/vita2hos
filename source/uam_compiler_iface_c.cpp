#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <uam/compiler_iface.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <unistd.h>
#ifndef __SWITCH__
#include <mutex>
#endif

#include "uam_compiler_iface_c.h"
#include "config.h"
#ifdef __SWITCH__
#include "log.h"
#endif

#ifndef UAM_TEMP_FILE_TEMPLATE
#define UAM_TEMP_FILE_TEMPLATE VITA2HOS_ROOT_PATH "/.uam-XXXXXX"
#endif
#ifndef UAM_CAPTURE_PREFIX
#define UAM_CAPTURE_PREFIX VITA2HOS_ROOT_PATH "/uam-pending"
#endif

namespace {
/* UAM owns process-global GLSL context/type caches. Its destructor releases
 * those caches, so the protected lifetime must include construction/destruction. */
#ifdef __SWITCH__
Mutex compiler_lock;
struct CompilerGuard {
    CompilerGuard() { mutexLock(&compiler_lock); }
    ~CompilerGuard() { mutexUnlock(&compiler_lock); }
};
#else
std::mutex compiler_lock;
using CompilerGuard = std::lock_guard<std::mutex>;
#endif

/* Opt-in, bounded crash evidence. Only the input currently being compiled is
 * retained; successful compilation removes it. No shader corpus is accumulated. */
bool capture_input(pipeline_stage stage, const char *source)
{
    FILE *enabled = fopen(UAM_CAPTURE_PREFIX ".enabled", "rb");
    if (!enabled) return false;
    fclose(enabled);
    /* A failed/oversized capture must never leave an older input looking like
     * evidence for the next compile. */
    unlink(UAM_CAPTURE_PREFIX ".glsl");
    unlink(UAM_CAPTURE_PREFIX ".txt");
    const size_t limit = 2 * 1024 * 1024;
    size_t length = strnlen(source, limit + 1);
    if (length > limit) return false;
    unsigned long long hash = 14695981039346656037ULL;
    for (size_t i = 0; i < length; ++i) {
        hash ^= (unsigned char)source[i];
        hash *= 1099511628211ULL;
    }
    FILE *file = fopen(UAM_CAPTURE_PREFIX ".glsl.tmp", "wb");
    if (!file) return false;
    bool ok = fwrite(source, 1, length, file) == length;
    if (fclose(file) != 0) ok = false;
    if (!ok || rename(UAM_CAPTURE_PREFIX ".glsl.tmp", UAM_CAPTURE_PREFIX ".glsl") != 0) {
        unlink(UAM_CAPTURE_PREFIX ".glsl.tmp");
        return false;
    }
    file = fopen(UAM_CAPTURE_PREFIX ".txt", "wb");
    if (!file) {
        unlink(UAM_CAPTURE_PREFIX ".glsl");
        return false;
    }
    ok = fprintf(file, "stage=%d\nbytes=%zu\nfnv1a64=%016llx\n", (int)stage, length, hash) > 0;
    if (fclose(file) != 0) ok = false;
    if (!ok) {
        unlink(UAM_CAPTURE_PREFIX ".glsl");
        unlink(UAM_CAPTURE_PREFIX ".txt");
    }
#ifdef __SWITCH__
    uint64_t tid = 0;
    svcGetThreadId(&tid, CUR_THREAD_HANDLE);
    LOG("UAM_CAPTURE stage=%d bytes=%zu fnv1a64=%016llx tid=%" PRIu64 " saved=%u",
        (int)stage, length, hash, tid, (unsigned)ok);
#endif
    return ok;
}
}

extern "C" {

static bool compile_alloc(pipeline_stage stage, const char *glsl_source, void **buffer,
                           uint32_t capacity, uint32_t *size)
{
    if (buffer)
        *buffer = nullptr;
    if (size)
        *size = 0;
    if (!glsl_source || !buffer || !size || !capacity)
        return false;
#ifdef __SWITCH__
    CompilerGuard guard;
#else
    CompilerGuard guard(compiler_lock);
#endif
    struct CaptureCleanup {
        bool captured, success = false;
        ~CaptureCleanup() {
            if (captured && success) {
                unlink(UAM_CAPTURE_PREFIX ".glsl");
                unlink(UAM_CAPTURE_PREFIX ".txt");
            }
        }
    } capture{capture_input(stage, glsl_source)};
#ifdef __SWITCH__
    LOG("UAM_COMPILE_ENTER stage=%d captured=%u", (int)stage, (unsigned)capture.captured);
#endif
#ifdef __SWITCH__
    /* Destroyed after the compiler: output_cleanup includes the temporary
     * file round trip, validation, unlink and compiler teardown. Both phases
     * are wall times and can include preemption by other guest threads. */
    struct PhaseTiming {
        uint64_t start = armGetSystemTick(), compiled = 0;
        bool success = false;
        ~PhaseTiming() {
            if (success)
                LOG("UAM_PHASE_TIMING compile_us=%" PRIu64 " output_cleanup_us=%" PRIu64,
                    armTicksToNs(compiled - start) / 1000,
                    armTicksToNs(armGetSystemTick() - compiled) / 1000);
        }
    } timing;
#endif
    /* The build links UAM with preserved indirect metadata in IR clones,
     * allowing its normal optimization level without the SSBO emitLDG fault. */
    DekoCompiler compiler{ stage, 3 };
    bool rc = compiler.CompileGlsl(glsl_source);
    if (!rc)
        return false;
#ifdef __SWITCH__
    timing.compiled = armGetSystemTick();
#endif

    /* The installed UAM memory writer has no capacity parameter. Stage its
     * file output, validate the complete DKSH, then copy only a bounded result. */
    char temporary[] = UAM_TEMP_FILE_TEMPLATE;
    int fd = mkstemp(temporary);
    if (fd < 0) return false;
    struct Cleanup {
        const char *path;
        ~Cleanup() { unlink(path); }
    } cleanup{temporary};
    if (close(fd) != 0) return false;
    compiler.OutputDksh(temporary);
    struct FileCloser { void operator()(FILE *file) const { fclose(file); } };
    std::unique_ptr<FILE, FileCloser> file(fopen(temporary, "rb"));
    if (!file || fseek(file.get(), 0, SEEK_END) != 0) return false;
    long length = ftell(file.get());
    if (length < 24 || (uint64_t)length > capacity || fseek(file.get(), 0, SEEK_SET) != 0)
        return false;
    std::unique_ptr<void, decltype(&free)> data(malloc((size_t)length), free);
    if (!data || fread(data.get(), 1, (size_t)length, file.get()) != (size_t)length)
        return false;
    uint32_t header[6];
    memcpy(header, data.get(), sizeof(header));
    if (memcmp(data.get(), "DKSH", 4) || header[1] < sizeof(header) ||
        header[1] > header[2] || header[2] > (uint32_t)length ||
        header[3] != (uint32_t)length - header[2])
        return false;
    *buffer = data.release();
    *size = (uint32_t)length;
    capture.success = true;
#ifdef __SWITCH__
    timing.success = true;
#endif
    return true;
}

bool uam_compiler_compile_glsl_alloc(pipeline_stage stage, const char *glsl_source,
                                     void **buffer, uint32_t *size)
{
    return compile_alloc(stage, glsl_source, buffer, UINT32_MAX, size);
}

bool uam_compiler_compile_glsl(pipeline_stage stage, const char *glsl_source, void *buffer,
                               uint32_t capacity, uint32_t *size)
{
    if (size) *size = 0;
    if (!buffer || !size) return false;
    void *compiled = nullptr;
    if (!compile_alloc(stage, glsl_source, &compiled, capacity, size)) return false;
    memcpy(buffer, compiled, *size);
    free(compiled);
    return true;
}
}
