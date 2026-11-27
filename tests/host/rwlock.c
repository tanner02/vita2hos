#define TEST_THREADED_MUTEX
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <unistd.h>
#include "../../source/modules/SceRWLock.c"
#include "../../source/module.c"
DECLARE_LIBRARY(SceLibKernel,0xcae9ace6);
DECLARE_LIBRARY(SceThreadmgr,0x859a24b1);
static _Thread_local VitaThreadInfo current;
VitaThreadInfo *SceKernelThreadMgr_get_thread_info(void){return &current;}
SceUID SceSysmem_get_next_uid(void){static atomic_int next=1;return atomic_fetch_add(&next,1);}
typedef struct {SceUID uid;bool write,timed,allow_deleted;unsigned timeout;int priority,result;atomic_bool acquired,release;} Worker;
static void *worker(void *arg)
{
    Worker *w=arg;current.uid=SceSysmem_get_next_uid();atomic_init(&current.priority,w->priority);
    w->result=w->write ? sceKernelLockWriteRWLock(w->uid,w->timed ? &w->timeout : NULL) : sceKernelLockReadRWLock(w->uid,w->timed ? &w->timeout : NULL);
    if(!w->result){atomic_store(&w->acquired,true);while(!atomic_load(&w->release))sched_yield();int result=unlock_rw(w->uid,w->write);assert(!result || (w->allow_deleted && (uint32_t)result==SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID));}
    return NULL;
}
static void pending(SceUID uid,int expected)
{
    uint64_t start=armGetSystemTick();
    for(;;){mutexLock(&rw_guard);int n=0;for(RwWaiter *w=find_rw(uid)->waiters;w;w=w->next)n+=!w->done;mutexUnlock(&rw_guard);
        if(n==expected)return;
        assert(armTicksToNs(armGetSystemTick()-start)<UINT64_C(2000000000));sched_yield();}
}
static void acquired(Worker *w)
{uint64_t start=armGetSystemTick();while(!atomic_load(&w->acquired)){assert(armTicksToNs(armGetSystemTick()-start)<UINT64_C(2000000000));sched_yield();}}
int main(void)
{
    alarm(20);mutexInit(&rw_guard);current.uid=SceSysmem_get_next_uid();atomic_init(&current.priority,160);
    assert((uint32_t)sceKernelCreateRWLock(NULL,0,NULL)==SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    assert((uint32_t)sceKernelCreateRWLock("bad",1,NULL)==SCE_KERNEL_ERROR_ILLEGAL_ATTR);
    SceKernelRWLockOptParam option={0};assert((uint32_t)sceKernelCreateRWLock("bad",0,&option)==SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);
    SceUID uid=sceKernelCreateRWLock("rw",0,NULL);assert(uid>0);
    assert(!sceKernelLockReadRWLock(uid,NULL));assert(!sceKernelLockReadRWLock(uid,NULL));
    assert((uint32_t)sceKernelTryLockWriteRWLock(uid)==SCE_KERNEL_ERROR_RW_LOCK_RECURSIVE);
    assert(!sceKernelUnlockReadRWLock(uid));assert(!sceKernelUnlockReadRWLock(uid));
    assert((uint32_t)sceKernelUnlockWriteRWLock(uid)==SCE_KERNEL_ERROR_RW_LOCK_FAILED_TO_UNLOCK);
    assert(!sceKernelLockWriteRWLock(uid,NULL));
    assert((uint32_t)sceKernelTryLockWriteRWLock(uid)==SCE_KERNEL_ERROR_RW_LOCK_RECURSIVE);
    assert((uint32_t)sceKernelUnlockReadRWLock(uid)==SCE_KERNEL_ERROR_RW_LOCK_NOT_OWNED);
    Worker timeout={.uid=uid,.timed=true,.timeout=1000,.priority=100};pthread_t thread;
    assert(!pthread_create(&thread,NULL,worker,&timeout));assert(!pthread_join(thread,NULL));
    assert((uint32_t)timeout.result==SCE_KERNEL_ERROR_WAIT_TIMEOUT && !timeout.timeout);
    Worker w[4];pthread_t t[4];
    for(int i=0;i<4;i++){w[i]=(Worker){.uid=uid,.write=i==2,.priority=100};assert(!pthread_create(&t[i],NULL,worker,&w[i]));pending(uid,i+1);}
    assert(!sceKernelUnlockWriteRWLock(uid));acquired(&w[0]);acquired(&w[1]);
    assert(!atomic_load(&w[2].acquired) && !atomic_load(&w[3].acquired));
    atomic_store(&w[0].release,true);atomic_store(&w[1].release,true);
    assert(!pthread_join(t[0],NULL));assert(!pthread_join(t[1],NULL));acquired(&w[2]);assert(!atomic_load(&w[3].acquired));
    atomic_store(&w[2].release,true);assert(!pthread_join(t[2],NULL));acquired(&w[3]);
    atomic_store(&w[3].release,true);assert(!pthread_join(t[3],NULL));
    assert(!sceKernelLockWriteRWLock(uid,NULL));
    for(int i=0;i<2;i++){w[i]=(Worker){.uid=uid,.write=i==1,.priority=100};assert(!pthread_create(&t[i],NULL,worker,&w[i]));pending(uid,i+1);}
    assert(!sceKernelDeleteRWLock(uid));
    for(int i=0;i<2;i++){assert(!pthread_join(t[i],NULL));assert((uint32_t)w[i].result==SCE_KERNEL_ERROR_WAIT_DELETE);}
    assert((uint32_t)sceKernelTryLockReadRWLock(uid)==SCE_KERNEL_ERROR_UNKNOWN_RW_LOCK_ID);
    uid=_sceKernelCreateRWLock("recursive",SCE_KERNEL_MUTEX_ATTR_RECURSIVE,NULL);assert(uid>0);
    assert(!sceKernelLockWriteRWLock(uid,NULL) && !sceKernelLockWriteRWLock(uid,NULL));
    assert(!sceKernelUnlockWriteRWLock(uid) && !sceKernelUnlockWriteRWLock(uid));assert(!sceKernelDeleteRWLock(uid));
    uid=sceKernelCreateRWLock("priority",SCE_KERNEL_ATTR_THREAD_PRIO,NULL);assert(uid>0);assert(!sceKernelLockWriteRWLock(uid,NULL));
    for(int i=0;i<2;i++){w[i]=(Worker){.uid=uid,.write=true,.priority=i ? 64 : 150};assert(!pthread_create(&t[i],NULL,worker,&w[i]));pending(uid,i+1);}
    assert(!sceKernelUnlockWriteRWLock(uid));acquired(&w[1]);assert(!atomic_load(&w[0].acquired));
    atomic_store(&w[1].release,true);assert(!pthread_join(t[1],NULL));acquired(&w[0]);atomic_store(&w[0].release,true);assert(!pthread_join(t[0],NULL));assert(!sceKernelDeleteRWLock(uid));
    /* Delete immediately after a grant: a completed waiter owns the record
     * until removing its stack node, even if it has not resumed yet. */
    for(unsigned i=0;i<128;i++) {
        uid=sceKernelCreateRWLock("grant/delete",0,NULL);assert(uid>0);
        assert(!sceKernelLockWriteRWLock(uid,NULL));
        w[0]=(Worker){.uid=uid,.write=(i&1)!=0,.priority=100,.allow_deleted=true};
        assert(!pthread_create(&t[0],NULL,worker,&w[0]));pending(uid,1);
        assert(!sceKernelUnlockWriteRWLock(uid));assert(!sceKernelDeleteRWLock(uid));
        atomic_store(&w[0].release,true);assert(!pthread_join(t[0],NULL));assert(!w[0].result);
    }
    const uint32_t public_nids[]={0x8667951D,0xFA670F0F,0x67A187BB};
    const uint32_t raw_nids[]={0xB1877F5E,0x7EB9E8B5,0x80191FAA,0xEFDDA456,0x206CBB66,0x3EF91145,0xB4151397,0xE73649CA};
    for(unsigned i=0;i<3;i++)assert(module_get_func_export(0xcae9ace6,public_nids[i]));
    for(unsigned i=0;i<8;i++)assert(module_get_func_export(0x859a24b1,raw_nids[i]));
    assert(!guest_rwlocks);pthread_mutex_destroy(&rw_guard);
    puts("Reader/writer locks: shared reads, exclusive writes, recursion, FIFO/priority, timeout and deletion passed.");
}
