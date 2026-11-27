#ifndef LOAD_EXPORTS_H
#define LOAD_EXPORTS_H

#include "module.h"
#include "load_process_params.h"

/* Addresses in records have already been relocated. The reader must validate
 * the entire requested range; function targets require executable storage.
 * Returned aliases are for reading only; symbol addresses retain guest VAs. */
typedef const void *(*LoadExportRead)(void *context, uint32_t address, size_t size,
                                    bool executable);
typedef struct {
    const void *start, *stop, *exit;
} LoadLifecycle;
/* Decode library-zero lifecycle functions. Output is unchanged on failure. */
int load_exports_lifecycle(LoadLifecycle *lifecycle, const void *records, size_t size,
                           LoadExportRead read, void *context);
int load_exports_metadata(LoadLifecycle *lifecycle, uint32_t *process_param,
                          const void *records, size_t size, LoadExportRead read, void *context);
int load_process_parameters_register(ModuleGuestExports *exports, const void *owner,
                                      uint32_t address, LoadExportRead read, void *context);
/* Optional settings are indirect guest pointers. Output is unchanged on error. */
int load_process_parameters_read(LoadProcessParameters *parameters, uint32_t address,
                                  LoadExportRead read, void *context);
int load_exports_register(ModuleGuestExports *exports, const void *owner,
                          const void *records, size_t size,
                          LoadExportRead read, void *context);

#endif
