#ifndef MISSING_IMPORTS_H
#define MISSING_IMPORTS_H
#include <stdint.h>
#define MISSING_IMPORT_SLOTS 1024
#ifdef __cplusplus
extern "C" {
#endif
/* Owner is a stable image address; release only after its execution stops.
 * Zero means the bounded pool is full and the caller must use its old stub. */
uintptr_t missing_import_bind(const void *owner,uint32_t library,uint32_t nid,uint32_t stub);
void missing_import_release(const void *owner);
int missing_import_report(unsigned index,uintptr_t return_address);
uintptr_t missing_import_stub_address(unsigned index);
#ifdef __cplusplus
}
#endif
#endif
