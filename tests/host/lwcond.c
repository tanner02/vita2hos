#define TEST_THREADED_MUTEX
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <unistd.h>
#include "../../source/modules/SceLwMutex.c"
#include "../../source/module.c"
DECLARE_LIBRARY(SceLibKernel,0xcae9ace6);

SceUID SceSysmem_get_next_uid(void)
{ static atomic_int next=1;return atomic_fetch_add(&next,1); }
static _Thread_local VitaThreadInfo current;
VitaThreadInfo *SceKernelThreadMgr_get_thread_info(void) { return &current; }
static LwWorkLayout mirror(SceKernelLwMutexWork *work)
{ LwWorkLayout state;memcpy(&state,work,sizeof(state));return state; }
static SceUID cond_uid(SceKernelLwCondWork *work)
{ SceUID uid;memcpy(&uid,work,sizeof(uid));return uid; }

typedef struct {
    SceKernelLwMutexWork *mutex;
    SceKernelLwCondWork *cond;
    SceUID uid;
    int priority,result,count_after,order;
    bool timed,owned_after;
    unsigned int timeout;
    atomic_bool returned;
} Worker;
static atomic_int sequence;
static void *worker(void *argument)
{
    Worker *w=argument;
    current.uid=w->uid=SceSysmem_get_next_uid();current.priority=w->priority;
    assert(!sceKernelLockLwMutex(w->mutex,1,NULL));
    w->result=sceKernelWaitLwCond(w->cond,w->timed ? &w->timeout : NULL);
    mutexLock(&lw_lock);
    LwWorkLayout state=mirror(w->mutex);
    w->owned_after=state.owner==(uint32_t)current.uid;
    w->count_after=state.count;
    mutexUnlock(&lw_lock);
    if (!w->result) {
        assert(w->owned_after && w->count_after==1);
        w->order=atomic_fetch_add(&sequence,1);
        assert(!sceKernelUnlockLwMutex(w->mutex,1));
    }
    atomic_store(&w->returned,true);
    return NULL;
}
static void pending(SceKernelLwCondWork *cond,SceKernelLwMutexWork *mutex,int expected)
{
    uint64_t start=armGetSystemTick();
    for (;;) {
        mutexLock(&lw_lock);
        LwWaiter *waiters=cond ? find_lwcond(cond)->waiters : find_lwmutex(mutex)->waiters;
        int count=0;
        for (LwWaiter *w=waiters;w;w=w->next) count+=!w->done;
        mutexUnlock(&lw_lock);
        if (count==expected) return;
        assert(armTicksToNs(armGetSystemTick()-start)<UINT64_C(2000000000));
        sched_yield();
    }
}
static void create(SceKernelLwMutexWork *mutex,SceKernelLwCondWork *cond,unsigned attr)
{
    assert(!sceKernelCreateLwMutex(mutex,"mutex",0,0,NULL));
    assert(!sceKernelCreateLwCond(cond,"condition",attr,mutex,NULL));
}
static void destroy(SceKernelLwMutexWork *mutex,SceKernelLwCondWork *cond)
{ assert(!sceKernelDeleteLwCond(cond));assert(!sceKernelDeleteLwMutex(mutex)); }
static void queue(unsigned attr)
{
    SceKernelLwMutexWork mutex;SceKernelLwCondWork cond;create(&mutex,&cond,attr);
    Worker w[3]={ {.mutex=&mutex,.cond=&cond,.priority=100},
                  {.mutex=&mutex,.cond=&cond,.priority=64},
                  {.mutex=&mutex,.cond=&cond,.priority=64} };
    pthread_t threads[3];atomic_store(&sequence,0);
    for (unsigned i=0;i<3;++i) {
        assert(!pthread_create(&threads[i],NULL,worker,&w[i]));pending(&cond,NULL,i+1);
    }
    const unsigned fifo[]={0,1,2},priority[]={1,2,0};
    const unsigned *order=attr ? priority : fifo;
    for (unsigned i=0;i<3;++i) {
        assert(!sceKernelLockLwMutex(&mutex,1,NULL));
        assert(!sceKernelSignalLwCond(&cond));
        pending(NULL,&mutex,1);
        assert(!atomic_load(&w[order[i]].returned));
        assert(!sceKernelUnlockLwMutex(&mutex,1));
        assert(!pthread_join(threads[order[i]],NULL));
        assert(!w[order[i]].result && w[order[i]].order==(int)i);
        pending(&cond,NULL,2-i);
    }
    destroy(&mutex,&cond);
}
int main(void)
{
    alarm(20);mutexInit(&lw_lock);current.uid=SceSysmem_get_next_uid();current.priority=160;
    SceKernelLwMutexWork mutex={0};
    struct { SceKernelLwCondWork work;uint32_t guard; } storage={.guard=0x13579bdf};
    SceKernelLwCondWork *cond=&storage.work;
    assert((uint32_t)sceKernelCreateLwCond(NULL,"bad",0,&mutex,NULL)==SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    assert((uint32_t)sceKernelCreateLwCond(cond,"bad",0,&mutex,NULL)==SCE_KERNEL_ERROR_UNKNOWN_LW_MUTEX_ID);
    assert(!sceKernelCreateLwMutex(&mutex,"test",0,0,NULL));
    assert((uint32_t)sceKernelCreateLwCond(cond,NULL,0,&mutex,NULL)==SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    assert((uint32_t)sceKernelCreateLwCond(cond,"bad",1,&mutex,NULL)==SCE_KERNEL_ERROR_ILLEGAL_ATTR);
    SceKernelLwCondOptParam option={0};
    assert((uint32_t)sceKernelCreateLwCond(cond,"bad",0,&mutex,&option)==SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);
    _Alignas(8) unsigned char unaligned[sizeof(*cond)+8];
    assert((uint32_t)sceKernelCreateLwCond((void *)(unaligned+1),"bad",0,&mutex,NULL)==SCE_KERNEL_ERROR_ILLEGAL_ALIGNMENT);
    option.size=sizeof(option);
    assert(!sceKernelCreateLwCond(cond,"test",0,&mutex,&option) && storage.guard==0x13579bdf);
    assert(cond_uid(cond)>0);
    assert((uint32_t)sceKernelCreateLwCond(cond,"duplicate",0,&mutex,NULL)==SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    assert((uint32_t)sceKernelWaitLwCond(cond,NULL)==SCE_KERNEL_ERROR_LW_MUTEX_NOT_OWNED);
    assert(!sceKernelSignalLwCond(cond)); /* Signals are not stored for a future wait. */
    assert(!sceKernelLockLwMutex(&mutex,1,NULL));unsigned timeout=0;
    assert((uint32_t)sceKernelWaitLwCond(cond,&timeout)==SCE_KERNEL_ERROR_WAIT_TIMEOUT);
    assert(!timeout && !mirror(&mutex).count);
    destroy(&mutex,cond);assert(cond_uid(cond)==-1 && storage.guard==0x13579bdf);
    assert((uint32_t)sceKernelSignalLwCond(cond)==SCE_KERNEL_ERROR_UNKNOWN_LW_COND_ID);

    queue(0);queue(SCE_KERNEL_ATTR_THREAD_PRIO);
    create(&mutex,cond,0);
    Worker broadcast[3];pthread_t broadcast_threads[3];
    for (unsigned i=0;i<3;++i) {
        broadcast[i]=(Worker){.mutex=&mutex,.cond=cond,.priority=160};
        assert(!pthread_create(&broadcast_threads[i],NULL,worker,&broadcast[i]));
        pending(cond,NULL,i+1);
    }
    assert(!sceKernelLockLwMutex(&mutex,1,NULL));
    assert(!sceKernelSignalLwCondAll(cond));pending(NULL,&mutex,3);
    assert(!sceKernelUnlockLwMutex(&mutex,1));
    for (unsigned i=0;i<3;++i)
        assert(!pthread_join(broadcast_threads[i],NULL) && !broadcast[i].result);
    destroy(&mutex,cond);
    create(&mutex,cond,0);
    Worker a={.mutex=&mutex,.cond=cond,.priority=160};
    Worker b=a;pthread_t ta,tb;
    assert(!pthread_create(&ta,NULL,worker,&a));pending(cond,NULL,1);
    assert(!pthread_create(&tb,NULL,worker,&b));pending(cond,NULL,2);
    assert(!sceKernelLockLwMutex(&mutex,1,NULL));
    assert(!sceKernelSignalLwCondTo(cond,b.uid));pending(NULL,&mutex,1);
    assert(!sceKernelUnlockLwMutex(&mutex,1));assert(!pthread_join(tb,NULL) && !b.result);
    pending(cond,NULL,1);
    assert(!sceKernelSignalLwCondAll(cond));assert(!pthread_join(ta,NULL) && !a.result);
    destroy(&mutex,cond);

    create(&mutex,cond,0);
    a=(Worker){.mutex=&mutex,.cond=cond,.priority=160,.timed=true,.timeout=100000};
    assert(!pthread_create(&ta,NULL,worker,&a));pending(cond,NULL,1);
    assert(!pthread_join(ta,NULL));
    assert((uint32_t)a.result==SCE_KERNEL_ERROR_WAIT_TIMEOUT && !a.timeout && !a.owned_after);
    a=(Worker){.mutex=&mutex,.cond=cond,.priority=160,.timed=true,.timeout=100000};
    assert(!pthread_create(&ta,NULL,worker,&a));pending(cond,NULL,1);
    assert(!sceKernelLockLwMutex(&mutex,1,NULL));
    assert(!sceKernelSignalLwCond(cond));pending(NULL,&mutex,1);
    assert(!pthread_join(ta,NULL)); /* Signal does not bypass a contended mutex. */
    assert((uint32_t)a.result==SCE_KERNEL_ERROR_WAIT_TIMEOUT && !a.owned_after);
    assert(!sceKernelUnlockLwMutex(&mutex,1));destroy(&mutex,cond);

    create(&mutex,cond,0);
    a=(Worker){.mutex=&mutex,.cond=cond,.priority=160};
    assert(!pthread_create(&ta,NULL,worker,&a));pending(cond,NULL,1);
    assert(!sceKernelDeleteLwCond(cond));
    assert(!sceKernelCreateLwCond(cond,"replacement",0,&mutex,NULL));SceUID replacement=cond_uid(cond);
    assert(!pthread_join(ta,NULL));
    assert((uint32_t)a.result==SCE_KERNEL_ERROR_WAIT_DELETE_LW_COND && !a.owned_after);
    assert(cond_uid(cond)==replacement);destroy(&mutex,cond);

    create(&mutex,cond,0);
    a=(Worker){.mutex=&mutex,.cond=cond,.priority=160};
    assert(!pthread_create(&ta,NULL,worker,&a));pending(cond,NULL,1);
    assert(!sceKernelDeleteLwMutex(&mutex));
    assert(!sceKernelCreateLwMutex(&mutex,"replacement",0,0,NULL));replacement=mirror(&mutex).uid;
    assert(!pthread_join(ta,NULL));
    assert((uint32_t)a.result==SCE_KERNEL_ERROR_WAIT_DELETE_LW_MUTEX && !a.owned_after);
    assert(mirror(&mutex).uid==replacement);
    assert((uint32_t)sceKernelWaitLwCond(cond,NULL)==SCE_KERNEL_ERROR_WAIT_DELETE_LW_MUTEX);
    destroy(&mutex,cond);

    create(&mutex,cond,0);
    a=(Worker){.mutex=&mutex,.cond=cond,.priority=160};
    assert(!pthread_create(&ta,NULL,worker,&a));pending(cond,NULL,1);
    assert(!sceKernelLockLwMutex(&mutex,1,NULL));
    assert(!sceKernelSignalLwCond(cond));pending(NULL,&mutex,1);
    assert(!sceKernelDeleteLwCond(cond) && !sceKernelDeleteLwMutex(&mutex));
    create(&mutex,cond,0);replacement=mirror(&mutex).uid;
    assert(!pthread_join(ta,NULL));
    assert((uint32_t)a.result==SCE_KERNEL_ERROR_WAIT_DELETE_LW_MUTEX && !a.owned_after);
    assert(mirror(&mutex).uid==replacement);destroy(&mutex,cond);

    assert(!sceKernelCreateLwMutex(&mutex,"recursive",SCE_KERNEL_MUTEX_ATTR_RECURSIVE,3,NULL));
    assert(!sceKernelCreateLwCond(cond,"recursive",0,&mutex,NULL));timeout=0;
    assert((uint32_t)sceKernelWaitLwCond(cond,&timeout)==SCE_KERNEL_ERROR_WAIT_TIMEOUT);
    assert(mirror(&mutex).count==2 && mirror(&mutex).owner==(uint32_t)current.uid);
    assert(!sceKernelUnlockLwMutex(&mutex,2));destroy(&mutex,cond);

    const uint32_t nids[]={0x48C7EAE6,0x721F6CB3,0x3AC63B9A,0xE5241A0C,0xFC1A48EB,0xE1878282};
    for (unsigned i=0;i<sizeof(nids)/sizeof(*nids);++i) assert(module_get_func_export(0xcae9ace6,nids[i]));
    assert(!lw_conditions && !lw_mutexes);pthread_mutex_destroy(&lw_lock);
    puts("Lightweight conditions: atomic mutex release, signal ordering, reacquisition, timeouts and deletion passed.");
}
