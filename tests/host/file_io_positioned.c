#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <setjmp.h>
#define TEST_THREADED_RWLOCK
#define TEST_THREADED_MUTEX
#define TEST_PROCESS_EXIT
static off_t checked_seek(int,off_t,int);
#define lseek checked_seek
#include "../../source/modules/SceLibKernel.c"
#undef lseek

static int fail_seek, seek_calls;
static off_t checked_seek(int fd,off_t offset,int whence)
{
    if (fail_seek && ++seek_calls==fail_seek) { errno=EIO;return -1; }
    return lseek(fd,offset,whence);
}
void log_printf(const char *format, ...) { (void)format; }
static jmp_buf process_exit;
static int prepared_exit;
void SceKernelThreadMgr_prepare_current_exit(int status) { prepared_exit=status; }
void threadExit(void) { assert(prepared_exit==37);longjmp(process_exit,1); }
SceUID SceSysmem_get_next_uid(void) { static SceUID next=1;return next++; }
VitaThreadInfo *SceKernelThreadMgr_get_thread_info(void)
{ static _Thread_local VitaThreadInfo info;return &info; }

static SceUID shared;
static pthread_barrier_t start;
static unsigned char pattern[128];
static void *positioned_reader(void *unused)
{
    (void)unused;pthread_barrier_wait(&start);
    for (unsigned i=0;i<1024;++i) {
        unsigned char byte=0;
        assert(sceIoPread(shared,&byte,1,17)==1 && byte==pattern[17]);
    }
    return NULL;
}
static void *positioned_writer(void *unused)
{
    (void)unused;pthread_barrier_wait(&start);
    for (unsigned i=0;i<1024;++i) assert(sceIoPwrite(shared,&pattern[63],1,63)==1);
    return NULL;
}
int main(void)
{
    alarm(20);
    assert(!SceLibKernel_init());
    if (!setjmp(process_exit)) { sceKernelExitProcess(37);assert(0); }
    assert(SceLibKernel_get_process_exit_res()==37 && g_process_exit_event);
    assert(SceLibKernel_process_exit_requested());
    char path[]="/tmp/vita2hos-positioned-XXXXXX";
    int native=mkstemp(path);assert(native>=0 && !close(native));
    SceUID fd=sceIoOpen(path,SCE_O_RDWR,0600);assert(fd>0);
    assert(sceIoWrite(fd,"abcdef",6)==6 && sceIoLseek(fd,2,SCE_SEEK_SET)==2);
    char bytes[4]={0};
    assert(sceIoPread(fd,bytes,3,3)==3 && !memcmp(bytes,"def",3));
    assert(sceIoLseek(fd,0,SCE_SEEK_CUR)==2);
    assert(sceIoPwrite(fd,"XY",2,0)==2 && sceIoLseek(fd,0,SCE_SEEK_CUR)==2);
    assert(sceIoRead(fd,bytes,2)==2 && !memcmp(bytes,"cd",2));
    assert(sceIoPread(fd,bytes,3,5)==1 && bytes[0]=='f');
    assert(sceIoPread(fd,bytes,3,6)==0 && sceIoLseek(fd,0,SCE_SEEK_CUR)==4);
    assert(!sceIoPread(fd,NULL,0,0) && !sceIoPwrite(fd,NULL,0,0));
    assert((uint32_t)sceIoPread(-1,bytes,1,0)==SCE_ERROR_ERRNO_EBADF);
    assert((uint32_t)sceIoPwrite(-1,bytes,1,0)==SCE_ERROR_ERRNO_EBADF);
    assert((uint32_t)sceIoPread(fd,NULL,1,0)==0x8001000e);
    assert((uint32_t)sceIoPwrite(fd,NULL,1,0)==0x8001000e);
    assert((uint32_t)sceIoPread(fd,bytes,1,-1)==SCE_ERROR_ERRNO_EINVAL);
    assert((uint32_t)sceIoPwrite(fd,bytes,UINT32_MAX,0)==SCE_ERROR_ERRNO_EINVAL);
    for (int phase=1;phase<=3;++phase) {
        assert(sceIoLseek(fd,2,SCE_SEEK_SET)==2);
        seek_calls=0;fail_seek=phase;
        assert((uint32_t)sceIoPread(fd,bytes,1,0)==0x80010005);
        fail_seek=0;
        if (phase<3) assert(sceIoLseek(fd,0,SCE_SEEK_CUR)==2);
        assert(sceIoLseek(fd,2,SCE_SEEK_SET)==2); /* No lock was leaked. */
    }
    SceOff large=(INT64_C(1)<<32)+3;
    assert(sceIoPwrite(fd,"Z",1,large)==1 && sceIoPread(fd,bytes,1,large)==1 && bytes[0]=='Z');
    assert(sceIoLseek(fd,0,SCE_SEEK_CUR)==2);
    assert(!sceIoClose(fd));
    fd=sceIoOpen(path,SCE_O_RDONLY,0);assert(fd>0);
    assert(sceIoLseek(fd,3,SCE_SEEK_SET)==3);
    assert((uint32_t)sceIoPwrite(fd,"!",1,0)==SCE_ERROR_ERRNO_EBADF);
    assert(sceIoLseek(fd,0,SCE_SEEK_CUR)==3 && !sceIoClose(fd));
    shared=sceIoOpen(path,SCE_O_RDWR|SCE_O_TRUNC,0600);assert(shared>0);
    for (unsigned i=0;i<sizeof(pattern);++i) pattern[i]=(unsigned char)(i+1);
    assert(sceIoWrite(shared,pattern,sizeof(pattern))==sizeof(pattern));
    assert(sceIoLseek(shared,0,SCE_SEEK_SET)==0);
    assert(!pthread_barrier_init(&start,NULL,3));
    pthread_t reader,writer;
    assert(!pthread_create(&reader,NULL,positioned_reader,NULL));
    assert(!pthread_create(&writer,NULL,positioned_writer,NULL));
    pthread_barrier_wait(&start);
    for (unsigned i=0;i<sizeof(pattern);++i) {
        unsigned char byte=0;
        assert(sceIoRead(shared,&byte,1)==1 && byte==pattern[i]);
    }
    assert(!pthread_join(reader,NULL) && !pthread_join(writer,NULL));
    assert(sceIoLseek(shared,0,SCE_SEEK_CUR)==sizeof(pattern));
    assert(!sceIoClose(shared) && !unlink(path));
    assert(!pthread_barrier_destroy(&start));
    vita_opened_files_dict_clear(g_vita_opened_files);
    vita_opened_dirs_dict_clear(g_vita_opened_dirs);
    pthread_rwlock_destroy(&g_vita_opened_files_lock);
    pthread_rwlock_destroy(&g_vita_opened_dirs_lock);
    puts("Positional I/O cursor isolation, partial transfers, failures and concurrent access passed.");
}
