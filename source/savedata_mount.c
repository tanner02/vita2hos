#include "savedata_mount.h"
#include <stdatomic.h>
#include <errno.h>
static _Atomic(SavedataResolver) current;
void savedata_mount_set(SavedataResolver resolver){atomic_store(&current,resolver);}
int savedata_mount_resolve(const char *path,char *output,size_t capacity)
{
    SavedataResolver resolver=atomic_load(&current);
    /* No registry lock is held across service code; shutdown can revoke the
     * mount while a previously captured static callback checks readiness. */
    return resolver ? resolver(path,output,capacity) : -ENODEV;
}
