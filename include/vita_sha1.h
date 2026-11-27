#ifndef VITA2HOS_SHA1_H
#define VITA2HOS_SHA1_H
#include <stddef.h>
#include <stdint.h>
typedef struct {uint32_t state[5];uint64_t bytes;size_t used;uint8_t block[64];} VitaSha1;
void vita_sha1_init(VitaSha1 *context);
void vita_sha1_update(VitaSha1 *context,const void *data,size_t size);
void vita_sha1_finish(VitaSha1 *context,uint8_t digest[20]);
#endif
