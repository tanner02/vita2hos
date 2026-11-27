#define TEST_THREADED_MUTEX
#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <unistd.h>
#include "../../source/modules/SceLwMutex.c"
#include "../../source/module.c"
DECLARE_LIBRARY(SceLibKernel,0xcae9ace6);
DECLARE_LIBRARY(SceThreadmgr,0x859a24b1);
static _Thread_local VitaThreadInfo current;
static atomic_int next_uid=1,order;
static bool fail_uid;
SceUID SceSysmem_get_next_uid(void) { return fail_uid ? -1 : atomic_fetch_add(&next_uid,1); }
VitaThreadInfo *SceKernelThreadMgr_get_thread_info(void) { return current.uid ? &current : NULL; }
void log_printf(const char *format,...) { (void)format; }
typedef struct { SceUID uid;int priority,result,position,unlock_result;unsigned timeout;bool forever,probe; } Worker;
static void *worker(void *argument)
{
    Worker *w=argument;current.uid=SceSysmem_get_next_uid();current.priority=w->priority;
    if (w->probe) { w->result=sceKernelTryLockMutex(w->uid,1);w->unlock_result=sceKernelUnlockMutex(w->uid,1);return NULL; }
    w->result=sceKernelLockMutex(w->uid,1,w->forever ? NULL : &w->timeout);
    if (!w->result) { w->position=atomic_fetch_add(&order,1);assert(!sceKernelUnlockMutex(w->uid,1)); }
    return NULL;
}
static void await_waiters(SceUID uid,unsigned expected)
{
    for (;;) {
        mutexLock(&lw_lock);LwMutex *m=find_named_mutex(uid);assert(m);
        unsigned count=0;for (LwWaiter *w=m->waiters;w;w=w->next) if (!w->done) ++count;
        mutexUnlock(&lw_lock);if (count==expected) return;sched_yield();
    }
}
int main(void)
{
    alarm(20);mutexInit(&lw_lock);current.uid=999;current.priority=160;
    const uint32_t public_nids[]={0xED53334A,0x1D8D7945};
    const uint32_t raw_nids[]={0x92667AE5,0x7FA945AD,0x72FC1F54,0x1A372EC8,0xCB78710D};
    for (unsigned i=0;i<2;++i) assert(module_get_func_export(0xcae9ace6,public_nids[i]));
    for (unsigned i=0;i<5;++i) assert(module_get_func_export(0x859a24b1,raw_nids[i]));
    fail_uid=true;assert((uint32_t)sceKernelCreateMutex("failure",0,0,NULL)==SCE_KERNEL_ERROR_NO_MEMORY && !lw_mutexes);fail_uid=false;
    assert((uint32_t)sceKernelCreateMutex(NULL,0,0,NULL)==SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    assert((uint32_t)sceKernelCreateMutex("bad",0,2,NULL)==SCE_KERNEL_ERROR_ILLEGAL_COUNT);
    SceKernelMutexOptParam option={.size=4};
    assert((uint32_t)sceKernelCreateMutex("bad",0,0,&option)==SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE);
    option.size=sizeof(option);option.ceilingPriority=1;
    assert((uint32_t)sceKernelCreateMutex("ceiling",0,0,&option)==SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    SceUID recursive=_sceKernelCreateMutex("recursive",SCE_KERNEL_MUTEX_ATTR_RECURSIVE,1,NULL);assert(recursive>0);
    assert(!sceKernelLockMutex(recursive,INT_MAX-1,NULL));
    assert((uint32_t)sceKernelTryLockMutex(recursive,1)==SCE_KERNEL_ERROR_MUTEX_LOCK_OVF);
    assert(!sceKernelUnlockMutex(recursive,INT_MAX));
    assert((uint32_t)sceKernelUnlockMutex(recursive,1)==SCE_KERNEL_ERROR_MUTEX_NOT_OWNED);
    assert(!sceKernelDeleteMutex(recursive));
    SceUID uid=sceKernelCreateMutex("fifo",0,1,NULL);assert(uid>0);
    assert((uint32_t)sceKernelTryLockMutex(uid,1)==SCE_KERNEL_ERROR_MUTEX_RECURSIVE);
    Worker probe={.uid=uid,.priority=100,.probe=true};pthread_t thread;
    assert(!pthread_create(&thread,NULL,worker,&probe));assert(!pthread_join(thread,NULL));
    assert((uint32_t)probe.result==SCE_KERNEL_ERROR_MUTEX_FAILED_TO_OWN);
    assert((uint32_t)probe.unlock_result==SCE_KERNEL_ERROR_MUTEX_NOT_OWNED);
    Worker timed={.uid=uid,.priority=100,.timeout=1000};
    assert(!pthread_create(&thread,NULL,worker,&timed));assert(!pthread_join(thread,NULL));
    assert((uint32_t)timed.result==SCE_KERNEL_ERROR_WAIT_TIMEOUT && !timed.timeout);
    Worker a={.uid=uid,.priority=150,.forever=true},b={.uid=uid,.priority=100,.forever=true};pthread_t ta,tb;
    assert(!pthread_create(&ta,NULL,worker,&a));await_waiters(uid,1);
    assert(!pthread_create(&tb,NULL,worker,&b));await_waiters(uid,2);
    assert(!sceKernelUnlockMutex(uid,1));assert(!pthread_join(ta,NULL) && !pthread_join(tb,NULL));
    assert(!a.result && !b.result && a.position==0 && b.position==1);assert(!sceKernelDeleteMutex(uid));
    atomic_store(&order,0);uid=sceKernelCreateMutex("priority",SCE_KERNEL_ATTR_THREAD_PRIO,1,NULL);assert(uid>0);
    a=(Worker){.uid=uid,.priority=150,.forever=true};b=(Worker){.uid=uid,.priority=100,.forever=true};
    assert(!pthread_create(&ta,NULL,worker,&a));await_waiters(uid,1);
    assert(!pthread_create(&tb,NULL,worker,&b));await_waiters(uid,2);
    assert(!sceKernelUnlockMutex(uid,1));assert(!pthread_join(ta,NULL) && !pthread_join(tb,NULL));
    assert(b.position==0 && a.position==1);assert(!sceKernelDeleteMutex(uid));
    uid=sceKernelCreateMutex("delete",0,1,NULL);assert(uid>0);
    a=(Worker){.uid=uid,.priority=100,.forever=true};assert(!pthread_create(&ta,NULL,worker,&a));await_waiters(uid,1);
    assert(!sceKernelDeleteMutex(uid));assert(!pthread_join(ta,NULL));
    assert((uint32_t)a.result==SCE_KERNEL_ERROR_WAIT_DELETE);
    assert((uint32_t)sceKernelUnlockMutex(uid,1)==SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID);
    assert((uint32_t)sceKernelDeleteMutex(uid)==SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID);
    uid=sceKernelCreateMutex("timeout with remaining waiter",0,1,NULL);assert(uid>0);
    a=(Worker){.uid=uid,.priority=100,.timeout=20000};
    b=(Worker){.uid=uid,.priority=120,.forever=true};
    assert(!pthread_create(&ta,NULL,worker,&a));
    assert(!pthread_create(&tb,NULL,worker,&b));
    assert(!pthread_join(ta,NULL));
    assert((uint32_t)a.result==SCE_KERNEL_ERROR_WAIT_TIMEOUT && !a.timeout);
    await_waiters(uid,1); /* Timed-out stack waiter has left the queue. */
    assert(!sceKernelUnlockMutex(uid,1));assert(!pthread_join(tb,NULL) && !b.result);
    assert(!sceKernelDeleteMutex(uid));
    /* A deleting thread may win before or after a contending try-lock, but
     * neither side may access a reclaimed record or the wrong error group. */
    for (unsigned iteration=0;iteration<128;++iteration) {
        uid=sceKernelCreateMutex("try/delete race",0,1,NULL);assert(uid>0);
        probe=(Worker){.uid=uid,.priority=100,.probe=true};
        assert(!pthread_create(&thread,NULL,worker,&probe));
        assert(!sceKernelDeleteMutex(uid));assert(!pthread_join(thread,NULL));
        assert((uint32_t)probe.result==SCE_KERNEL_ERROR_MUTEX_FAILED_TO_OWN ||
               (uint32_t)probe.result==SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID);
        assert((uint32_t)probe.unlock_result==SCE_KERNEL_ERROR_MUTEX_NOT_OWNED ||
               (uint32_t)probe.unlock_result==SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID);
    }
    SceKernelLwMutexWork lightweight={0};
    assert(!sceKernelCreateLwMutex(&lightweight,"separate kind",0,0,NULL));
    LwWorkLayout layout;memcpy(&layout,&lightweight,sizeof(layout));
    assert((uint32_t)sceKernelTryLockMutex(layout.uid,1)==SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID);
    assert((uint32_t)sceKernelDeleteMutex(layout.uid)==SCE_KERNEL_ERROR_UNKNOWN_MUTEX_ID);
    assert(!sceKernelDeleteLwMutex(&lightweight));
    assert(!lw_mutexes);
    puts("Named mutexes: NIDs, recursive ownership/overflow, contention, FIFO/priority ordering, timeout and deletion lifetime passed.");
}
