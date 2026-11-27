#define TEST_FIXED_MEMORY
#define TEST_THREADED_RWLOCK
#define TEST_THREADED_MUTEX
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdbool.h>
static bool fail_allocation,fail_name;
static void *test_calloc(size_t count,size_t size)
{
    if (fail_allocation) { fail_allocation=false;return NULL; }
    return calloc(count,size);
}
static char *test_strdup(const char *text)
{
    if (fail_name) { fail_name=false;return NULL; }
    return strdup(text);
}
#define calloc test_calloc
#define strdup test_strdup
#include "../../source/modules/SceLibKernel.c"
#undef calloc
#undef strdup
#include "../../source/modules/SceProcessmgr.c"
#include "../../source/module.c"
DECLARE_LIBRARY(SceThreadmgr,0x859a24b1);
void log_printf(const char *format,...) { (void)format; }
void SceKernelThreadMgr_prepare_current_exit(int status) { (void)status; }
VitaThreadInfo *SceKernelThreadMgr_get_thread_info(void) { return NULL; }
SceUID SceSysmem_get_next_uid(void) { static atomic_int uid=100;return atomic_fetch_add(&uid,1); }
static void *write_concurrently(void *arg)
{
    (void)arg;SceUID uid=sceKernelGetStdout();assert(uid>=100);
    assert(sceIoWrite(uid,"x",1)==1);assert(!sceIoClose(uid));return NULL;
}
int main(void)
{
    assert(!SceLibKernel_init());
    SceUID (*get_in)(void)=(void *)module_get_func_export(0x2DD91812,0xC1727F59);
    SceUID (*get_out)(void)=(void *)module_get_func_export(0x2DD91812,0xE5AA625C);
    SceUID (*get_err)(void)=(void *)module_get_func_export(0x2DD91812,0xFA5E3ADA);
    assert(get_in && get_out && get_err);
    fail_allocation=true;assert((uint32_t)get_out()==0x8001000c);
    fail_name=true;assert((uint32_t)get_err()==0x8001000c);
    assert(!vita_opened_files_dict_size(g_vita_opened_files));
    int saved[3],input[2],output[2],errors[2];
    for (int i=0;i<3;++i) { saved[i]=dup(i);assert(saved[i]>=0); }
    assert(!pipe(input) && !pipe(output) && !pipe(errors));
    assert(dup2(input[0],0)==0 && dup2(output[1],1)==1 && dup2(errors[1],2)==2);
    close(input[0]);close(output[1]);close(errors[1]);
    SceUID in=get_in(),out=get_out(),err=get_err(),other=get_out();
    assert(in>=100 && out>in && err>out && other>err);
    const char data[]={'a','\0','%','b','\n'};char received[16];
    assert(write(input[1],data,sizeof(data))==sizeof(data));close(input[1]);
    assert(sceIoRead(in,received,sizeof(received))==sizeof(data));
    assert(!memcmp(data,received,sizeof(data)) && !sceIoRead(in,received,1));
    assert((uint32_t)sceIoWrite(in,"x",1)==SCE_ERROR_ERRNO_EBADF);
    assert((uint32_t)sceIoRead(out,received,1)==SCE_ERROR_ERRNO_EBADF);
    assert((uint32_t)sceIoRead(err,received,1)==SCE_ERROR_ERRNO_EBADF);
    assert((uint32_t)sceIoWrite(out,NULL,1)==0x8001000e);
    assert(!sceIoWrite(out,NULL,0));
    assert(sceIoWrite(out,data,sizeof(data))==sizeof(data));
    assert(read(output[0],received,sizeof(received))==sizeof(data));
    assert(!memcmp(data,received,sizeof(data)));
    assert(sceIoWrite(err,"err",3)==3 && read(errors[0],received,16)==3);
    assert(!memcmp(received,"err",3));
    assert(sceIoLseek(out,0,SCE_SEEK_CUR)==(int32_t)0x8001001d);
    assert((uint32_t)sceIoPread(in,received,1,0)==0x8001001d);
    assert((uint32_t)sceIoPwrite(out,"x",1,0)==0x8001001d);
    assert(!sceIoClose(out));
    assert((uint32_t)sceIoWrite(out,"x",1)==SCE_ERROR_ERRNO_EBADF);
    assert(fcntl(STDOUT_FILENO,F_GETFD)>=0); /* Guest close cannot close host stdout. */
    assert(sceIoWrite(other,"ok",2)==2 && read(output[0],received,16)==2);
    pthread_t threads[8];
    for (unsigned i=0;i<8;++i) assert(!pthread_create(&threads[i],NULL,write_concurrently,NULL));
    for (unsigned i=0;i<8;++i) assert(!pthread_join(threads[i],NULL));
    assert(read(output[0],received,16)==8);
    for (int i=0;i<8;++i) assert(received[i]=='x');
    assert(!sceIoClose(in) && !sceIoClose(err) && !sceIoClose(other));
    for (int i=0;i<3;++i) { assert(fcntl(i,F_GETFD)>=0);assert(dup2(saved[i],i)==i);close(saved[i]); }
    close(output[0]);close(errors[0]);
    assert(!vita_opened_files_dict_size(g_vita_opened_files));
    vita_opened_files_dict_clear(g_vita_opened_files);vita_opened_dirs_dict_clear(g_vita_opened_dirs);
    puts("Standard I/O: NID lookup, binary transfers, EOF, access modes, close ownership, concurrent getters and allocation failures passed.");
}
