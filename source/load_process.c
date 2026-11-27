#include "load_process.h"
#include "load_module_paths.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>

Result load_process_close(LoadProcess *process)
{
    if (!process) return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    process->bound = false;
    process->run_started = false;
    process->modules_started = 0;
    process->guest_tp_retargeted = false;
    process->entry = NULL;
    Result result = 0;
    for (size_t i = process->count; i > 0; --i) {
        Result close = load_image_close(&process->images[i - 1]);
        if (R_FAILED(close) && R_SUCCEEDED(result)) result = close;
    }
    module_guest_exports_clear(&process->exports);
    if (R_FAILED(result)) return result;
    free(process->images);
    process->images = NULL;
    process->count = 0;
    return 0;
}

static int load_process_load_impl(LoadProcess *process, const char *executable,
                       const char *const *modules, size_t module_count, const char *app_root)
{
    if (!process || !executable || !*executable || process->images || process->count ||
        process->exports.libraries || process->bound || module_count > LOAD_PROCESS_MAX_MODULES ||
        (module_count && !modules)) return -1;
    for (size_t i = 0; i < module_count; ++i)
        if (!modules[i] || !*modules[i]) return -1;
    process->entry = NULL;
    process->source_tag = 0;
    process->images = calloc(app_root ? LOAD_PROCESS_MAX_MODULES + 1 : module_count + 1,
                             sizeof(*process->images));
    if (!process->images) return -1;
    int result = -1;
    process->count = 1; /* Include partial preparation in rollback. */
    result = load_file_prepare(&process->images[0], executable);
    process->source_tag = process->images[0].source_tag;
    if (result < 0) goto fail;
    result = -1;
    if (!process->images[0].entry) goto fail;
    LoadModulePaths preloads;
    const char *preload_paths[LOAD_PROCESS_MAX_MODULES];
    if (app_root) {
        if (load_module_paths_preloads(&preloads, app_root, &process->images[0].parameters) < 0)
            goto fail;
        module_count=preloads.count;
        for (size_t i=0; i<module_count; ++i) preload_paths[i]=preloads.paths[i];
        modules=preload_paths;
    }
    for (size_t i = 0; i < module_count; ++i) {
        ++process->count;
        result = load_module_prepare(&process->images[i + 1], modules[i]);
        if (result < 0) goto fail;
    }
    result = -1;
    for (size_t i = 0; i < process->count; ++i)
        if (load_image_publish(&process->images[i], &process->exports) < 0) goto fail;
    for (size_t i = 0; i < process->count; ++i)
        if (load_image_bind(&process->images[i], &process->exports) < 0) goto fail;
    for (size_t i = 0; i < process->count; ++i)
        process->guest_tp_retargeted |= process->images[i].guest_tp_retargeted;
    process->entry = process->images[0].entry;
    process->bound = true;
    return 0;
fail:
    if (R_FAILED(load_process_close(process))) {
        LOG("Process image rollback failed; retaining mapping ownership.");
        return -1;
    }
    return result < 0 ? result : -1;
}

int load_process_load(LoadProcess *process, const char *executable,
                       const char *const *modules, size_t module_count)
{
    return load_process_load_impl(process, executable, modules, module_count, NULL);
}

int load_process_load_app(LoadProcess *process, const char *executable,
                          const char *app_root, const LoadModulePaths *modules)
{
    if (!app_root || !*app_root || !modules || modules->count > LOAD_PROCESS_MAX_MODULES)
        return -1;
    const char *paths[LOAD_PROCESS_MAX_MODULES];
    if (modules->present) {
        for (size_t i=0; i<modules->count; ++i) {
            if (!memchr(modules->paths[i], 0, LOAD_MODULE_PATH_SIZE)) return -1;
            paths[i]=modules->paths[i];
        }
        return load_process_load_impl(process, executable, paths, modules->count, NULL);
    }
    return load_process_load_impl(process, executable, NULL, 0, app_root);
}
