#ifndef SAVEDATA_IDENTITY_H
#define SAVEDATA_IDENTITY_H
#include <stddef.h>
typedef struct {char title[10];char directory[64];} SavedataIdentity;
/* Bounded PARAM.SFO parser. Output is unchanged on failure. No files/mounts
 * are created; identifiers do not establish license or publisher authenticity. */
int savedata_identity_parse(const void *data,size_t size,SavedataIdentity *identity);
#endif
