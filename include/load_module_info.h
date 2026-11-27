#ifndef LOAD_MODULE_INFO_H
#define LOAD_MODULE_INFO_H
#include <psp2common/kernel/modulemgr.h>
#include "sce-elf.h"
#include "load_segment.h"
#include "load_exports.h"

/* Decode relocated module metadata and loaded ranges. Output is unchanged on
 * failure. The raw record must come from the mapped image after relocation. */
int load_module_info_build(SceKernelModuleInfo *info, const sce_module_info_raw *raw,
                           const segment_info_t *segments, int count, int info_segment,
                           const LoadLifecycle *lifecycle, LoadExportRead read, void *context);
#endif
