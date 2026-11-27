#ifndef SAVEDATA_MOUNT_H
#define SAVEDATA_MOUNT_H
#include <stddef.h>
/* Resolver has static lifetime and returns zero or negative native errno.
 * It must recheck its service state, including after mount revocation. */
typedef int (*SavedataResolver)(const char *,char *,size_t);
void savedata_mount_set(SavedataResolver resolver);
int savedata_mount_resolve(const char *path,char *output,size_t capacity);
#endif
