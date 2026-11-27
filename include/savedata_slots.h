#ifndef SAVEDATA_SLOTS_H
#define SAVEDATA_SLOTS_H
#include <stdbool.h>
#include <stdint.h>
#define SAVEDATA_SLOT_BYTES 0x34c
/* Internal backend: caller supplies an existing title-specific directory and
 * serializes operations. No guest mount or SDK success is implied. Returns
 * zero or negative errno; get preserves output on every failure. */
int savedata_slot_get(const char *root,uint32_t id,void *data);
int savedata_slot_put(const char *root,uint32_t id,const void *data,bool create);
int savedata_slot_delete(const char *root,uint32_t id);
#endif
