#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include <psp2/kernel/error.h>
#include <psp2/kernel/threadmgr/rwlock.h>
#include <psp2/kernel/threadmgr/mutex.h>
#include "module.h"
#include "modules/SceKernelThreadMgr.h"
#include "modules/SceSysmem.h"
typedef struct RwOwner {struct RwOwner *next;SceUID uid;int count;} RwOwner;
typedef struct RwWaiter {
    struct RwWaiter *next;CondVar condition;RwOwner *owner;
    int priority,result;bool write,done;
} RwWaiter;
typedef struct GuestRw {
    struct GuestRw *next;SceUID uid;unsigned attr,references;
    char name[32];
    bool write,deleted;int count;RwOwner *owners;RwWaiter *waiters;
} GuestRw;
static Mutex rw_guard;
static GuestRw *guest_rwlocks;
static GuestRw *find_rw(SceUID uid)
{for(GuestRw *r=guest_rwlocks;r;r=r->next)if(r->uid==uid)return r;return NULL;}
static RwOwner *find_owner(GuestRw *r,SceUID uid)
{for(RwOwner *o=r->owners;o;o=o->next)if(o->uid==uid)return o;return NULL;}
static void release_rw(GuestRw *r)
{
    if(--r->references)return;
    while(r->owners){RwOwner *o=r->owners;r->owners=o->next;free(o);}free(r);
}
static void grant_rw(GuestRw *r,RwWaiter *w)
{
    w->owner->next=r->owners;r->owners=w->owner;
    r->write=w->write;++r->count;w->result=0;w->done=true;
    condvarWakeAll(&w->condition);
}
static void handoff_rw(GuestRw *r)
{
    if(r->count || r->deleted)return;
    RwWaiter *w=r->waiters;while(w && w->done)w=w->next;
    if(!w)return;
    bool write=w->write;grant_rw(r,w);if(write)return;
    /* Batch adjacent readers in the selected FIFO/priority queue. A queued
     * writer forms a boundary; new readers may still join active readers. */
    for(w=w->next;w;w=w->next){if(w->done)continue;if(w->write)break;grant_rw(r,w);}
}
EXPORT(SceLibKernel,0x8667951D,SceUID,sceKernelCreateRWLock,const char *name,SceUInt32 attr,
       const SceKernelRWLockOptParam *option)
{
    if(!name)return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    if(attr & ~(SCE_KERNEL_ATTR_THREAD_PRIO|SCE_KERNEL_MUTEX_ATTR_RECURSIVE))return SCE_KERNEL_ERROR_ILLEGAL_ATTR;
    if(option && option->size!=sizeof(*option))return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
    GuestRw *r=calloc(1,sizeof(*r));if(!r)return SCE_KERNEL_ERROR_NO_MEMORY;
    r->uid=SceSysmem_get_next_uid();if(r->uid<=0){free(r);return SCE_KERNEL_ERROR_NO_MEMORY;}
    SceUID uid=r->uid;r->attr=attr;r->references=1;strncpy(r->name,name,sizeof(r->name)-1);
    mutexLock(&rw_guard);r->next=guest_rwlocks;guest_rwlocks=r;mutexUnlock(&rw_guard);return uid;
}
EXPORT(SceThreadmgr,0xB1877F5E,SceUID,_sceKernelCreateRWLock,const char *name,SceUInt32 attr,
       const SceKernelRWLockOptParam *option)
{return sceKernelCreateRWLock(name,attr,option);}
static int lock_rw(SceUID uid,unsigned *timeout,bool write,bool only_try)
{
    VitaThreadInfo *thread=SceKernelThreadMgr_get_thread_info();
    if(!thread || thread->uid<=0)return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    uint64_t start=armGetSystemTick(),duration=timeout ? (uint64_t)*timeout*1000 : UINT64_MAX;
    mutexLock(&rw_guard);GuestRw *r=find_rw(uid);int result=0;
    if(!r){result=SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID;goto done;}
    RwOwner *owner=find_owner(r,thread->uid);
    if(owner && (r->write!=write || (write && !(r->attr&SCE_KERNEL_MUTEX_ATTR_RECURSIVE)))) {
        result=SCE_KERNEL_ERROR_RW_LOCK_RECURSIVE;goto done;
    }
    if(r->count==INT_MAX){result=SCE_KERNEL_ERROR_RW_LOCK_LOCK_OVF;goto done;}
    bool available=!r->count || (!write && !r->write) || owner;
    if(!available && only_try){result=SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_LOCK;goto done;}
    if(available && owner){++owner->count;++r->count;goto done;}
    owner=calloc(1,sizeof(*owner));if(!owner){result=SCE_KERNEL_ERROR_NO_MEMORY;goto done;}
    owner->uid=thread->uid;owner->count=1;
    if(available){owner->next=r->owners;r->owners=owner;r->write=write;++r->count;goto done;}
    RwWaiter waiter={.owner=owner,.priority=atomic_load(&thread->priority),.write=write};
    condvarInit(&waiter.condition);++r->references;
    RwWaiter **tail=&r->waiters;
    while(*tail && ((*tail)->done || !(r->attr&SCE_KERNEL_ATTR_THREAD_PRIO) || (*tail)->priority<=waiter.priority))tail=&(*tail)->next;
    waiter.next=*tail;*tail=&waiter;
    while(!waiter.done) {
        uint64_t elapsed=armTicksToNs(armGetSystemTick()-start);
        if(timeout && elapsed>=duration){waiter.result=SCE_KERNEL_ERROR_WAIT_TIMEOUT;break;}
        Result rc=condvarWaitTimeout(&waiter.condition,&rw_guard,timeout ? duration-elapsed : UINT64_MAX);
        if(R_FAILED(rc) && rc!=0xEA01 && !waiter.done){waiter.result=SCE_KERNEL_ERROR_RW_LOCK_ERROR;break;}
    }
    RwWaiter **link=&r->waiters;while(*link!=&waiter)link=&(*link)->next;*link=waiter.next;
    result=waiter.result;
    if(result)free(owner);
    handoff_rw(r);release_rw(r);
done:
    if(timeout){uint64_t elapsed=armTicksToNs(armGetSystemTick()-start);*timeout=elapsed>=duration ? 0 : (unsigned)((duration-elapsed)/1000);}
    mutexUnlock(&rw_guard);return result;
}
EXPORT(SceLibKernel,0xFA670F0F,int,sceKernelLockReadRWLock,SceUID uid,unsigned *timeout)
{return lock_rw(uid,timeout,false,false);}
EXPORT(SceLibKernel,0x67A187BB,int,sceKernelLockWriteRWLock,SceUID uid,unsigned *timeout)
{return lock_rw(uid,timeout,true,false);}
EXPORT(SceThreadmgr,0x7EB9E8B5,int,_sceKernelLockReadRWLock,SceUID uid,unsigned *timeout)
{return lock_rw(uid,timeout,false,false);}
EXPORT(SceThreadmgr,0x80191FAA,int,_sceKernelLockWriteRWLock,SceUID uid,unsigned *timeout)
{return lock_rw(uid,timeout,true,false);}
EXPORT(SceThreadmgr,0xEFDDA456,int,sceKernelTryLockReadRWLock,SceUID uid)
{return lock_rw(uid,NULL,false,true);}
EXPORT(SceThreadmgr,0x206CBB66,int,sceKernelTryLockWriteRWLock,SceUID uid)
{return lock_rw(uid,NULL,true,true);}
static int unlock_rw(SceUID uid,bool write)
{
    VitaThreadInfo *thread=SceKernelThreadMgr_get_thread_info();
    if(!thread || thread->uid<=0)return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
    mutexLock(&rw_guard);GuestRw *r=find_rw(uid);int result=SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID;
    if(r){
        RwOwner **link=&r->owners;while(*link && (*link)->uid!=thread->uid)link=&(*link)->next;
        if(!*link)result=SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_UNLOCK;
        else if(r->write!=write)result=SCE_KERNEL_ERROR_RW_LOCK_NOT_OWNED;
        else {RwOwner *o=*link;if(!--o->count){*link=o->next;free(o);}--r->count;handoff_rw(r);result=0;}
    }
    mutexUnlock(&rw_guard);return result;
}
EXPORT(SceThreadmgr,0x3EF91145,int,sceKernelUnlockReadRWLock,SceUID uid)
{return unlock_rw(uid,false);}
EXPORT(SceThreadmgr,0xB4151397,int,sceKernelUnlockWriteRWLock,SceUID uid)
{return unlock_rw(uid,true);}
EXPORT(SceThreadmgr,0xE73649CA,int,sceKernelDeleteRWLock,SceUID uid)
{
    mutexLock(&rw_guard);GuestRw **link=&guest_rwlocks;
    while(*link && (*link)->uid!=uid)link=&(*link)->next;
    int result=SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID;
    if(*link){GuestRw *r=*link;*link=r->next;r->deleted=true;
        for(RwWaiter *w=r->waiters;w;w=w->next)if(!w->done){w->done=true;w->result=SCE_KERNEL_ERROR_WAIT_DELETE;condvarWakeAll(&w->condition);}
        release_rw(r);result=0;}
    mutexUnlock(&rw_guard);return result;
}
