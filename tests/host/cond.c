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
DECLARE_LIBRARY(SceThreadmgr,0x859a24b1);
SceUID SceSysmem_get_next_uid(void)
{ static atomic_int next=1;return atomic_fetch_add(&next,1); }
static _Thread_local VitaThreadInfo current;
VitaThreadInfo *SceKernelThreadMgr_get_thread_info(void) { return &current; }
typedef struct { SceUID mutex,cond,thread;int priority,result;unsigned timeout;bool timed;atomic_bool returned; } Worker;
static void *worker(void *arg)
{
    Worker *w=arg;current.uid=w->thread=SceSysmem_get_next_uid();current.priority=w->priority;
    assert(!sceKernelLockMutex(w->mutex,1,NULL));
    w->result=sceKernelWaitCond(w->cond,w->timed ? &w->timeout : NULL);
    if (!w->result) assert(!sceKernelUnlockMutex(w->mutex,1));
    atomic_store(&w->returned,true);return NULL;
}
static void pending(SceUID uid,bool cond,int count)
{
    uint64_t start=armGetSystemTick();
    for (;;) {
        mutexLock(&lw_lock);
        LwWaiter *w=cond ? find_named_cond(uid)->waiters : find_named_mutex(uid)->waiters;
        int n=0;for (;w;w=w->next) n+=!w->done;
        mutexUnlock(&lw_lock);
        if (n==count) return;
        assert(armTicksToNs(armGetSystemTick()-start)<UINT64_C(2000000000));sched_yield();
    }
}
static void create(SceUID *m,SceUID *c,unsigned attr)
{ *m=sceKernelCreateMutex("mutex",0,0,NULL);assert(*m>0);*c=sceKernelCreateCond("cond",attr,*m,NULL);assert(*c>0); }
static void destroy(SceUID m,SceUID c)
{ assert(!sceKernelDeleteCond(c));assert(!sceKernelDeleteMutex(m)); }
static void queue(unsigned attr)
{
    SceUID m,c;create(&m,&c,attr);pthread_t t[3];Worker w[3];
    for (int i=0;i<3;i++) {w[i]=(Worker){.mutex=m,.cond=c,.priority=i ? 64 : 150};assert(!pthread_create(&t[i],NULL,worker,&w[i]));pending(c,true,i+1);}
    int fifo[]={0,1,2},prio[]={1,2,0};int *order=attr ? prio : fifo;
    for (int i=0;i<3;i++) {
        assert(!sceKernelLockMutex(m,1,NULL));assert(!sceKernelSignalCond(c));pending(m,false,1);
        assert(!atomic_load(&w[order[i]].returned));assert(!sceKernelUnlockMutex(m,1));
        assert(!pthread_join(t[order[i]],NULL));assert(!w[order[i]].result);pending(c,true,2-i);
    }
    destroy(m,c);
}
int main(void)
{
    alarm(25);mutexInit(&lw_lock);current.uid=SceSysmem_get_next_uid();current.priority=160;
    SceUID m,c;create(&m,&c,0);
    assert((uint32_t)sceKernelCreateCond(NULL,0,m,NULL)==SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    assert((uint32_t)sceKernelCreateCond("bad",1,m,NULL)==SCE_KERNEL_ERROR_ILLEGAL_ATTR);
    SceKernelCondOptParam opt={0};assert((uint32_t)sceKernelCreateCond("bad",0,m,&opt)==SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);
    assert((uint32_t)sceKernelCreateCond("bad",0,c,NULL)==SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID);
    assert((uint32_t)sceKernelWaitCond(m,NULL)==SCE_KERNEL_ERROR_UNKNOWN_COND_ID);
    assert((uint32_t)sceKernelDeleteCond(m)==SCE_KERNEL_ERROR_UNKNOWN_COND_ID);
    assert((uint32_t)sceKernelWaitCond(c,NULL)==SCE_KERNEL_ERROR_MUTEX_NOT_OWNED);
    assert((uint32_t)sceKernelSignalCondTo(c,0)==SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID);
    assert(!sceKernelSignalCond(c));assert(!sceKernelLockMutex(m,1,NULL));unsigned timeout=0;
    assert((uint32_t)_sceKernelWaitCond(c,&timeout)==SCE_KERNEL_ERROR_WAIT_TIMEOUT);destroy(m,c);
    queue(0);queue(SCE_KERNEL_ATTR_THREAD_PRIO);
    /* Targeted wake and broadcast, both requiring mutex reacquisition. */
    create(&m,&c,0);Worker a={.mutex=m,.cond=c,.priority=160},b=a;pthread_t ta,tb;
    assert(!pthread_create(&ta,NULL,worker,&a));pending(c,true,1);
    assert(!pthread_create(&tb,NULL,worker,&b));pending(c,true,2);
    assert(!sceKernelSignalCondTo(c,b.thread));assert(!pthread_join(tb,NULL));assert(!b.result);
    pending(c,true,1);assert(!sceKernelSignalCondAll(c));assert(!pthread_join(ta,NULL));assert(!a.result);destroy(m,c);
    for (int mode=0;mode<7;mode++) {
        create(&m,&c,0);a=(Worker){.mutex=m,.cond=c,.priority=160,.timed=mode<2,.timeout=20000};
        assert(!pthread_create(&ta,NULL,worker,&a));pending(c,true,1);
        if (mode==1 || mode>=4) {assert(!sceKernelLockMutex(m,1,NULL));assert(!sceKernelSignalCond(c));pending(m,false,1);}
        if (mode==2 || mode>=5) assert(!sceKernelDeleteCond(c));
        if (mode==5) assert(!sceKernelUnlockMutex(m,1));
        if (mode==3 || mode==4 || mode==6) assert(!sceKernelDeleteMutex(m));
        assert(!pthread_join(ta,NULL));
        uint32_t expected=mode<2 ? SCE_KERNEL_ERROR_WAIT_TIMEOUT : mode==2 ? SCE_KERNEL_ERROR_WAIT_DELETE_COND : SCE_KERNEL_ERROR_WAIT_DELETE_MUTEX;
        assert((uint32_t)a.result==(mode==5 ? 0 : expected));
        if (mode<2) {assert(!a.timeout);if (mode==1) assert(!sceKernelUnlockMutex(m,1));}
        if (mode!=2 && mode<5) assert(!sceKernelDeleteCond(c));
        if (mode<3 || mode==5) assert(!sceKernelDeleteMutex(m));
    }
    /* Recursive waits drop exactly one count, as in the pinned reference. */
    m=sceKernelCreateMutex("recursive",SCE_KERNEL_MUTEX_ATTR_RECURSIVE,3,NULL);assert(m>0);
    c=_sceKernelCreateCond("recursive",0,m,NULL);assert(c>0);timeout=0;
    assert((uint32_t)sceKernelWaitCond(c,&timeout)==SCE_KERNEL_ERROR_WAIT_TIMEOUT);
    assert(find_named_mutex(m)->count==2);assert(!sceKernelUnlockMutex(m,2));destroy(m,c);
    SceKernelLwMutexWork lw;assert(!sceKernelCreateLwMutex(&lw,"lw",0,0,NULL));
    assert((uint32_t)sceKernelCreateCond("wrong kind",0,find_lwmutex(&lw)->uid,NULL)==SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID);
    assert(!sceKernelDeleteLwMutex(&lw));
    const uint32_t public_nids[]={0x50572FDA,0xC88D44AD};
    const uint32_t raw_nids[]={0xCC14FA59,0x040795C7,0x879E6EBD,0x6ED2E2DC,0xC2E7AC22,0x1269F4EC};
    for (unsigned i=0;i<2;i++) assert(module_get_func_export(0xcae9ace6,public_nids[i]));
    for (unsigned i=0;i<6;i++) assert(module_get_func_export(0x859a24b1,raw_nids[i]));
    assert(!lw_conditions && !lw_mutexes);pthread_mutex_destroy(&lw_lock);
    puts("Named conditions: UID isolation, wake ordering, timeouts, deletion and reacquisition passed.");
}
