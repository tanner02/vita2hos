#include <stdlib.h>

#include "load.h"
#include "missing_imports.h"

int load_image_publish(LoadImage *image, ModuleGuestExports *exports)
{
    if (!image || !exports || !image->prepared || image->bound || image->export_registry ||
        exports == &image->exports) return -1;
    if (module_guest_exports_merge(exports, &image->exports) < 0) return -1;
    image->export_registry = exports;
    return 0;
}

Result load_image_close(LoadImage *image)
{
    if (!image)
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    /* Stop publishing guest addresses before any backing mapping is removed.
     * The caller has already stopped execution of this image. */
    if (image->export_registry) {
        module_guest_exports_remove(image->export_registry, image);
        image->export_registry = NULL;
    }
    module_guest_exports_clear(&image->exports);
    guest_tls_template_clear(&image->tls);
    free(image->segments);
    image->segments = NULL;
    image->segment_count = 0;
    image->imports = image->entry = NULL;
    image->imports_size = 0;
    image->prepared = image->bound = false;
    image->lifecycle = (LoadLifecycle){0};
    image->module = false;
    image->guest_tp_retargeted = false;
    image->parameters = (LoadProcessParameters){0};
    image->module_info = (SceKernelModuleInfo){0};
    atomic_store(&image->module_state,0);
    Result result = 0;
    for (int i = 0; i < image->fixed_count; ++i) {
        Result close = load_fixed_close(&image->fixed[i]);
        if (R_FAILED(close) && R_SUCCEEDED(result))
            result = close;
    }
    if (R_FAILED(result))
        return result; /* Retain ownership for a cleanup retry. */
    free(image->fixed);
    image->fixed = NULL;
    image->fixed_count = 0;
    if (image->jit_active) {
        result = jitClose(&image->jit);
        if (R_FAILED(result))
            return result;
        image->jit_active = false;
    }
    free(image->data);
    image->data = NULL;
    missing_import_release(image);
    return 0;
}
