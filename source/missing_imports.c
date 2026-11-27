#include <switch.h>
#include <psp2/kernel/error.h>
#include "missing_imports.h"
#include "log.h"
typedef struct {const void *owner;uint32_t library,nid,stub;bool reported;} MissingSlot;
static MissingSlot slots[MISSING_IMPORT_SLOTS];
static Mutex missing_guard;
uintptr_t missing_import_bind(const void *owner,uint32_t library,uint32_t nid,uint32_t stub)
{
    if(!owner)return 0;
    mutexLock(&missing_guard);
    unsigned free_slot=MISSING_IMPORT_SLOTS,index=MISSING_IMPORT_SLOTS;
    for(unsigned i=0;i<MISSING_IMPORT_SLOTS;++i) {
        if(!slots[i].owner && free_slot==MISSING_IMPORT_SLOTS)free_slot=i;
        if(slots[i].owner==owner && slots[i].library==library && slots[i].nid==nid && slots[i].stub==stub){index=i;break;}
    }
    if(index==MISSING_IMPORT_SLOTS && free_slot!=MISSING_IMPORT_SLOTS) {
        index=free_slot;slots[index]=(MissingSlot){.owner=owner,.library=library,.nid=nid,.stub=stub};
    }
    uintptr_t address=index<MISSING_IMPORT_SLOTS ? missing_import_stub_address(index) : 0;
    mutexUnlock(&missing_guard);return address;
}
void missing_import_release(const void *owner)
{
    if(!owner)return;
    mutexLock(&missing_guard);
    for(unsigned i=0;i<MISSING_IMPORT_SLOTS;++i)if(slots[i].owner==owner)slots[i]=(MissingSlot){0};
    mutexUnlock(&missing_guard);
}
int missing_import_report(unsigned index,uintptr_t return_address)
{
    bool report=false;uint32_t library=0,nid=0,stub=0;
    mutexLock(&missing_guard);
    if(index<MISSING_IMPORT_SLOTS && slots[index].owner && !slots[index].reported) {
        slots[index].reported=true;report=true;
        library=slots[index].library;nid=slots[index].nid;stub=slots[index].stub;
    }
    mutexUnlock(&missing_guard);
    if(report)LOG("UNSUPPORTED_CALL kind=function library=0x%08" PRIx32
        " nid=0x%08" PRIx32 " stub=0x%08" PRIx32 " return_address=0x%08" PRIxPTR,
        library,nid,stub,return_address);
    return (int)SCE_KERNEL_ERROR_MODULEMGR_NO_FUNC_NID;
}
