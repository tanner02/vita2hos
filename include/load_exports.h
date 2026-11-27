#ifndef LOAD_EXPORTS_H
#define LOAD_EXPORTS_H

#include "module.h"

/* Addresses in records have already been relocated. The reader must validate
 * the entire requested range; function targets require executable storage.
 * Returned aliases are for reading only; symbol addresses retain guest VAs. */
typedef const void *(*LoadExportRead)(void *context, uint32_t address, size_t size,
                                    bool executable);
int load_exports_register(ModuleGuestExports *exports, const void *owner,
                          const void *records, size_t size,
                          LoadExportRead read, void *context);

#endif
