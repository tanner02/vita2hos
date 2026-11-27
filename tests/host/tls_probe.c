#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include "../hardware/tls_probe/main.c"

static jmp_buf exited;
static unsigned char thread_storage[2][0x800+16] __attribute__((aligned(16)));
static unsigned current_thread, delays, creates;
static int exit_status, worker_status, wrong_pointer, bad_worker_seed, failed_write;
static unsigned write_calls;
static SceKernelThreadEntry worker_function;
static char output[2048];static size_t output_size;
SceUID sceIoOpen(const char *path,int flags,SceMode mode)
{ (void)flags;(void)mode;assert(!strcmp(path,"app0:/tls-probe.txt"));return 7; }
SceSSize sceIoWrite(SceUID fd,const void *data,SceSize size)
{
    assert(fd==7 && output_size+size<sizeof(output));
    ++write_calls;
    if(failed_write==1 || (failed_write==2 && write_calls==1))return -1;
    memcpy(output+output_size,data,size);output_size+=size;output[output_size]=0;return size;
}
int sceIoClose(SceUID fd){assert(fd==7);return 0;}
void *sceKernelGetTLSAddr(int key){assert(key==0);return thread_storage[current_thread];}
void *probe_read_tp_arm(void)
{ return wrong_pointer ? (void *)1 : thread_storage[current_thread]+0x800; }
void *probe_read_tp_thumb(void){return probe_read_tp_arm();}
int sceKernelDelayThread(SceUInt delay){assert(delay==1000);++delays;return 0;}
SceUID sceKernelCreateThread(const char *name,SceKernelThreadEntry entry,int priority,
    SceSize stack,SceUInt attributes,int affinity,const SceKernelThreadOptParam *option)
{
    (void)attributes;(void)affinity;(void)option;
    assert(!strcmp(name,"tls_probe_worker") && priority==0x10000100 && stack==16384);
    worker_function=entry;++creates;return 2;
}
int sceKernelStartThread(SceUID thread,SceSize args,void *argp)
{
    assert(thread==2 && !args && !argp);current_thread=1;
    if(bad_worker_seed)memset(thread_storage[1]+0x800,0,4);
    worker_status=worker_function(0,NULL);current_thread=0;return 0;
}
int sceKernelWaitThreadEnd(SceUID thread,int *status,SceUInt *timeout)
{ assert(thread==2 && !timeout);*status=worker_status;return 0; }
int sceKernelDeleteThread(SceUID thread){assert(thread==2);return 0;}
int sceKernelExitProcess(int status){exit_status=status;longjmp(exited,1);}

static void run_probe(void)
{
    memset(thread_storage,0,sizeof(thread_storage));
    for(int i=0;i<2;++i){uint32_t seed=0x13579bdf;memcpy(thread_storage[i]+0x800,&seed,4);}
    current_thread=delays=creates=0;output_size=0;output[0]=0;exit_status=-1;
    write_calls=0;worker_status=-1;worker_function=NULL;
    report=-1;report_failed=0;main_tls=0;
    if(setjmp(exited)==0)module_start(0,NULL);
}
int main(void)
{
    run_probe();assert(exit_status==0 && delays==64 && creates==1);
    assert(strstr(output,"PASS main_tls\n") && strstr(output,"PASS worker_tls\n"));
    assert(strstr(output,"PASS thread_isolation\nPASS tls_probe\n"));
    uint32_t first,second;memcpy(&first,thread_storage[0]+0x804,4);memcpy(&second,thread_storage[1]+0x804,4);
    assert(first==0x77777777 && second==0x33333333);
    wrong_pointer=1;run_probe();assert(exit_status==2 && creates==0 && delays==0);
    assert(strstr(output,"FAIL main 0x00000002\nFAIL tls_probe\n"));
    wrong_pointer=0;bad_worker_seed=1;run_probe();assert(exit_status==24);
    assert(strstr(output,"FAIL worker 0x00000004\n") && !strstr(output,"PASS tls_probe\n"));
    bad_worker_seed=0;failed_write=1;run_probe();assert(exit_status==26);
    failed_write=2;run_probe();assert(exit_status==26 && strstr(output,"FAIL tls_probe\n"));
    assert(!strstr(output,"PASS tls_probe\n"));
    puts("Owned TLS probe host model: pass path, absent-plan safety, worker failure and report errors passed.");
}
