#ifndef VITA2HOS_UAM_SHADER_CACHE_H
#define VITA2HOS_UAM_SHADER_CACHE_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Calls are serialized with the compiler. Load returns independently owned,
 * free()-able storage only after source, revision, integrity and DKSH checks.
 * All cache errors are misses; callers may always compile normally instead. */
bool uam_cache_load(const char *directory,const char *revision,uint32_t stage,
    const char *source,uint32_t capacity,void **data,uint32_t *size);
void uam_cache_store(const char *directory,const char *revision,uint32_t stage,
    const char *source,const void *data,uint32_t size);
#ifdef __cplusplus
}
#endif
#endif
