#include <stdint.h>
#include <stdlib.h>
#include <switch.h>
#include <psp2/kernel/clib.h>
#include "module.h"

/* The pinned backend is compiled ONLY_MSPACES with system growth disabled.
 * These declarations are its C ABI; do not include its C++ implementation
 * header in this translation unit. All calls, including initial setup, share
 * one lock because the backend's own locks are disabled. */
extern void *create_mspace_with_base(void *base,size_t size,int locked);
extern size_t destroy_mspace(void *space);
extern void *mspace_malloc(void *space,size_t size);
extern void mspace_free(void *space,void *pointer);
typedef struct Allocation { struct Allocation *next;void *pointer; } Allocation;
typedef struct Space {
    struct Space *next;
    void *handle;
    uintptr_t base,end;
    Allocation *allocations;
} Space;
static Mutex space_lock;
static Space *spaces;
static Space *find_space(void *handle)
{ for (Space *s=spaces;s;s=s->next) if(s->handle==handle)return s;return NULL; }
EXPORT(SceLibKernel,0x3B9E301A,SceClibMspace,sceClibMspaceCreate,void *base,SceSize size)
{
    uintptr_t start=(uintptr_t)base;
    if (!base || (start&7) || !size || size>UINTPTR_MAX-start) return NULL;
    Space *record=calloc(1,sizeof(*record));
    if (!record) return NULL;
    mutexLock(&space_lock);
    for (Space *s=spaces;s;s=s->next) if(start<s->end && start+size>s->base) {
        mutexUnlock(&space_lock);free(record);return NULL;
    }
    record->handle=create_mspace_with_base(base,size,0);
    void *handle=record->handle;
    if(handle) {
        record->base=start;record->end=start+size;
        record->next=spaces;spaces=record;
    }
    mutexUnlock(&space_lock);
    if(!handle)free(record);
    return handle;
}
EXPORT(SceLibKernel,0x86EF7680,void *,sceClibMspaceMalloc,SceClibMspace handle,SceSize size)
{
    mutexLock(&space_lock);
    Space *space=find_space(handle);
    void *pointer=NULL;
    if(space) {
        Allocation *record=malloc(sizeof(*record));
        if(record) {
            pointer=mspace_malloc(handle,size);
            if(pointer) {record->pointer=pointer;record->next=space->allocations;space->allocations=record;}
            else free(record);
        }
    }
    mutexUnlock(&space_lock);
    return pointer;
}
EXPORT(SceLibKernel,0x9C56B4D1,void,sceClibMspaceFree,SceClibMspace handle,void *pointer)
{
    if(!pointer)return;
    mutexLock(&space_lock);
    Space *space=find_space(handle);
    if(space) {
        Allocation **link=&space->allocations;
        while(*link && (*link)->pointer!=pointer)link=&(*link)->next;
        if(*link) {
            Allocation *record=*link;*link=record->next;
            mspace_free(handle,pointer);free(record);
        }
    }
    mutexUnlock(&space_lock);
}
EXPORT(SceLibKernel,0xAE1A21EC,void,sceClibMspaceDestroy,SceClibMspace handle)
{
    mutexLock(&space_lock);
    Space **link=&spaces;
    while(*link && (*link)->handle!=handle)link=&(*link)->next;
    if(*link) {
        Space *space=*link;*link=space->next;
        /* The backend's external base is caller-owned, even with live chunks. */
        destroy_mspace(handle);
        while(space->allocations) {
            Allocation *record=space->allocations;space->allocations=record->next;free(record);
        }
        free(space);
    }
    mutexUnlock(&space_lock);
}
