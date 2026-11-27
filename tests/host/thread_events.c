#define TEST_THREADED_MUTEX
#include <assert.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
static bool fail_allocation;
static void *checked_calloc(size_t n,size_t size) { return fail_allocation ? NULL : calloc(n,size); }
#define calloc checked_calloc
#include "../../source/thread_events.c"
#undef calloc
static atomic_int next_uid=1;
SceUID SceSysmem_get_next_uid(void) { return atomic_fetch_add(&next_uid,1); }
void log_printf(const char *format,...) { (void)format; }
static _Thread_local VitaThreadEventFrame *frames;
static int calls[16],call_count;
static int record(SceInt32 type,SceUID thread,SceInt32 arg,void *common)
{
    assert((type==4 || type==8) && thread>0 && !arg);
    calls[call_count++]=(int)(intptr_t)common;return 0;
}
static SceUID self,victim,newborn;
static int mutate(SceInt32 type,SceUID thread,SceInt32 arg,void *common)
{
    (void)type;(void)thread;(void)arg;(void)common;
    calls[call_count++]=3;
    assert(!thread_events_unregister(self) && !thread_events_unregister(victim));
    newborn=thread_events_register("new",VITA_THREAD_EVENT_PROCESS_ALL,4,record,(void *)5);
    assert(newborn>0);return 17; /* Nonzero return is diagnostic; the event continues. */
}
static pthread_mutex_t gate=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed=PTHREAD_COND_INITIALIZER;
static int entered,released;
static int blocking(SceInt32 type,SceUID thread,SceInt32 arg,void *common)
{
    assert(type==8 && (thread==11 || thread==12) && !arg && common==&entered);
    assert(!pthread_mutex_lock(&gate));++entered;assert(!pthread_cond_broadcast(&changed));
    while (!released) assert(!pthread_cond_wait(&changed,&gate));
    assert(!pthread_mutex_unlock(&gate));return 1;
}
static void *dispatch(void *argument)
{
    thread_events_dispatch(8,(SceUID)(intptr_t)argument,&frames);assert(!frames);return NULL;
}
static jmp_buf escaped;
static int nonreturning(SceInt32 type,SceUID thread,SceInt32 arg,void *common)
{
    (void)type;(void)thread;(void)arg;(void)common;
    assert(!thread_events_unregister(self));thread_events_abandon(&frames);longjmp(escaped,1);
}
int main(void)
{
    alarm(15);thread_events_finish();thread_events_init();
    assert((uint32_t)thread_events_register(NULL,42,4,record,NULL)==SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    assert((uint32_t)thread_events_register("bad",42,4,NULL,NULL)==SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    assert((uint32_t)thread_events_register("bad",0,4,record,NULL)==SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    assert((uint32_t)thread_events_register("bad",42,1,record,NULL)==SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    assert((uint32_t)thread_events_register("bad",42,0,record,NULL)==SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    assert((uint32_t)thread_events_register("12345678901234567890123456789012",42,4,record,NULL)==SCE_KERNEL_ERROR_UID_NAME_TOO_LONG);
    fail_allocation=true;
    assert((uint32_t)thread_events_register("OOM",42,4,record,NULL)==SCE_KERNEL_ERROR_NO_MEMORY && !events);
    fail_allocation=false;
    SceUID a=thread_events_register("all",VITA_THREAD_EVENT_PROCESS_ALL,4,record,(void *)1);
    SceUID b=thread_events_register("specific",42,12,record,(void *)2);assert(a>0 && b>a);
    thread_events_dispatch(4,41,&frames);assert(call_count==1 && calls[0]==1 && !frames);
    thread_events_dispatch(8,42,&frames);assert(call_count==2 && calls[1]==2);
    thread_events_dispatch(4,42,&frames);assert(call_count==4 && calls[2]==1 && calls[3]==2);
    assert(!thread_events_unregister(a) && !thread_events_unregister(b));
    assert((uint32_t)thread_events_unregister(a)==SCE_KERNEL_ERROR_UNKNOWN_THREAD_EVENT_ID);
    call_count=0;
    self=thread_events_register("mutator",42,4,mutate,NULL);
    victim=thread_events_register("victim",42,4,record,(void *)4);
    thread_events_dispatch(4,42,&frames);
    assert(call_count==1 && calls[0]==3 && !frames);
    thread_events_dispatch(4,42,&frames);assert(call_count==2 && calls[1]==5);
    assert(!thread_events_unregister(newborn));

    a=thread_events_register("concurrent",VITA_THREAD_EVENT_PROCESS_ALL,8,blocking,&entered);
    pthread_t first,second;
    assert(!pthread_create(&first,NULL,dispatch,(void *)11) && !pthread_create(&second,NULL,dispatch,(void *)12));
    assert(!pthread_mutex_lock(&gate));
    while (entered!=2) assert(!pthread_cond_wait(&changed,&gate));
    assert(!thread_events_unregister(a));
    thread_events_dispatch(8,13,&frames); /* Unregister removes future dispatch. */
    assert(!events);released=1;assert(!pthread_cond_broadcast(&changed));assert(!pthread_mutex_unlock(&gate));
    assert(!pthread_join(first,NULL) && !pthread_join(second,NULL));
    self=thread_events_register("exit inside callback",42,4,nonreturning,NULL);
    if (!setjmp(escaped)) { thread_events_dispatch(4,42,&frames);assert(0); }
    assert(!frames && !events);
    thread_events_finish();pthread_mutex_destroy(&event_lock);
    puts("Thread events: selectors, callback mutation, concurrent unregister and nonreturning cleanup passed.");
}
