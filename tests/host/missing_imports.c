#define TEST_THREADED_MUTEX
#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <psp2/kernel/error.h>
#include "missing_imports.h"
typedef uint64_t (*Stub)(uint32_t,uint32_t,uint32_t,uint32_t);
static pthread_mutex_t log_guard=PTHREAD_MUTEX_INITIALIZER;
static atomic_uint reports;
static char last_log[384];
void log_printf(const char *format,...)
{
    pthread_mutex_lock(&log_guard);va_list args;va_start(args,format);
    vsnprintf(last_log,sizeof(last_log),format,args);va_end(args);
    atomic_fetch_add(&reports,1);pthread_mutex_unlock(&log_guard);
}
static void check(Stub fn)
{
    uint64_t result=fn(0x11111111,0x22223333,0x44444444,0x55555555);
    assert((uint32_t)result==SCE_KERNEL_ERROR_MODULEMGR_NO_FUNC_NID);
    assert((uint32_t)(result>>32)==0x22223333);
}
static void *worker(void *arg)
{for(unsigned i=0;i<100;i++)check((Stub)(uintptr_t)arg);return NULL;}
int main(void)
{
    char a=0,b=0;
    uintptr_t first=missing_import_bind(&a,42,99,0x81001000);assert(first);
    assert(first==missing_import_bind(&a,42,99,0x81001000));
    pthread_t threads[16];
    for(unsigned i=0;i<16;i++)assert(!pthread_create(&threads[i],NULL,worker,(void *)first));
    for(unsigned i=0;i<16;i++)assert(!pthread_join(threads[i],NULL));
    assert(reports==1 && strstr(last_log,"library=0x0000002a nid=0x00000063 stub=0x81001000 return_address=0x"));
    assert(strstr(last_log,"r0=0x11111111 r1=0x22223333 r2=0x44444444 r3=0x55555555"));
    uintptr_t other=missing_import_bind(&b,42,99,0x81001000);assert(other && other!=first);
    check((Stub)other);assert(reports==2);
    missing_import_release(&a);check((Stub)first);assert(reports==2);
    uintptr_t reused=missing_import_bind(&a,77,88,0x81002000);assert(reused);
    check((Stub)reused);assert(reports==3 && strstr(last_log,"library=0x0000004d nid=0x00000058"));
    missing_import_release(&a);missing_import_release(&b);
    for(unsigned i=0;i<MISSING_IMPORT_SLOTS;i++)assert(missing_import_bind(&a,2,i,0x81000000+i*12));
    assert(!missing_import_bind(&b,2,1234,0x82000000));
    missing_import_release(&a);assert(missing_import_bind(&b,2,1234,0x82000000));missing_import_release(&b);
    assert(!missing_import_bind(NULL,1,1,1));assert(!missing_import_stub_address(MISSING_IMPORT_SLOTS));
    assert((uint32_t)missing_import_report(MISSING_IMPORT_SLOTS,0,0,0,0,0)==SCE_KERNEL_ERROR_MODULEMGR_NO_FUNC_NID);
    assert(reports==3);
    puts("Missing import diagnostics: first-use logging, concurrent calls, legacy register-pair result, owner reuse and bounded exhaustion passed.");
}
