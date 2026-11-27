#ifndef LOAD_H
#define LOAD_H

#include <switch.h>
#include "load_fixed.h"
#include "module.h"
#include "load_segment.h"
#include "load_exports.h"
#include "guest_tls.h"

typedef struct {
    Jit jit;
    bool jit_active;
    void *data;
    LoadFixedMemory *fixed;
    int fixed_count;
    uint64_t source_tag;
    ModuleGuestExports exports;
    ModuleGuestExports *export_registry; /* Optional process-owned publication. */
    segment_info_t *segments; /* Loaded ranges only; no source-file pointers. */
    int segment_count;
    void *imports;
    size_t imports_size;
    void *entry;
    bool prepared, bound;
    bool module; /* Dependency preparation permits an absent module start. */
    LoadLifecycle lifecycle;
    GuestTlsTemplate tls;
    bool guest_tp_retargeted;
    LoadProcessParameters parameters;
} LoadImage;

enum { LOAD_LAYOUT_CONFLICT = -2 };

int load_file(LoadImage *image, const char *filename, void **entry);
/* Preparation maps/relocates and collects exports without binding or executing.
 * A process can prepare every dependency, merge exports into a process registry,
 * then bind every image. No guest code may run until all binds succeed.
 * Images must remain at stable addresses. A shared registry must outlive every
 * published image; close all images, then clear the registry. On bind failure,
 * close the process images without executing them. */
int load_file_prepare(LoadImage *image, const char *filename);
int load_module_prepare(LoadImage *image, const char *filename);
int load_image_publish(LoadImage *image, ModuleGuestExports *exports);
int load_image_bind(LoadImage *image, ModuleGuestExports *exports);
Result load_image_close(LoadImage *image);

#endif
