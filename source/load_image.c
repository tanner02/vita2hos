#include <stdlib.h>

#include "load.h"

Result load_image_close(LoadImage *image)
{
    if (!image)
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    /* Stop publishing guest addresses before any backing mapping is removed.
     * The caller has already stopped execution of this image. */
    module_guest_exports_clear(&image->exports);
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
    return 0;
}
