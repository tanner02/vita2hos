#include <psp2/io/fcntl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/threadmgr/cond.h>
#include <psp2/kernel/threadmgr/signal.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/error.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stddef.h>
static SceUID report=-1,mutex=-1,condition=-1,parent;
static SceClibMspace heap;
static atomic_uint phase;
static int io_failed;
static void record(const char *s)
{ unsigned n=0;while(s[n])++n;if(report>=0 && sceIoWrite(report,s,n)!=(int)n)io_failed=1; }
static void result(const char *s,uint32_t value)
{ char hex[]=" 0x00000000\n";for(unsigned i=0;i<8;i++)hex[3+i]="0123456789abcdef"[(value>>(28-i*4))&15];record(s);record(hex); }
static int worker(SceSize args,void *argp)
{
    (void)args;(void)argp;
    unsigned timeout=1000000;
    int ret=sceKernelLockMutex(mutex,1,&timeout);if(ret)return 40;
    atomic_store(&phase,1);timeout=1000000;
    ret=sceKernelWaitCond(condition,&timeout);
    if(ret)return 41;
    if(sceKernelUnlockMutex(mutex,1))return 42;
    atomic_store(&phase,2);
    for(unsigned i=0;i<128;i++) {
        timeout=1000000;if(sceKernelWaitSignal(0,0,&timeout))return 43;
        unsigned char *p=sceClibMspaceMalloc(heap,128);if(!p)return 44;
        for(unsigned j=0;j<128;j++)p[j]=(unsigned char)(i+j);
        for(unsigned j=0;j<128;j++)if(p[j]!=(unsigned char)(i+j))return 45;
        sceClibMspaceFree(heap,p);
        if(sceKernelSendSignal(parent))return 46;
    }
    return 0;
}
int module_start(SceSize args,void *argp)
{
    (void)args;(void)argp;
    SceUID block=-1,thread=-1;int started=0,status=0,worker_status=-1;
    report=sceIoOpen("app0:/kernel-probe.txt",SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0600);
    if(report<0){sceKernelExitProcess(20);return 20;}
    record("kernel_probe v1\n");
#define CHECK(expr,code) do{int check_result=(expr);if(check_result){result("FAIL " #expr,(uint32_t)check_result);status=(code);goto done;}}while(0)
    parent=sceKernelGetThreadId();if(parent<0){status=21;goto done;}
    CHECK(sceKernelSendSignal(parent),22);
    if((uint32_t)sceKernelSendSignal(parent)!=SCE_KERNEL_ERROR_ALREADY_SENT){status=23;goto done;}
    unsigned timeout=0;CHECK(sceKernelWaitSignal(0,0,&timeout),24);
    timeout=1000;if((uint32_t)sceKernelWaitSignal(0,0,&timeout)!=SCE_KERNEL_ERROR_WAIT_TIMEOUT || timeout){status=25;goto done;}
    record("PASS retained_duplicate_and_timeout_signal\n");
    block=sceKernelAllocMemBlock("kernel_probe_heap",SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,65536,NULL);
    if(block<0){status=26;goto done;}void *base=NULL;
    CHECK(sceKernelGetMemBlockBase(block,&base),27);
    heap=sceClibMspaceCreate(base,65536);if(!heap){status=28;goto done;}
    void *chunk=sceClibMspaceMalloc(heap,1024);
    if(!chunk || (uintptr_t)chunk<(uintptr_t)base || (uintptr_t)chunk+1024>(uintptr_t)base+65536 || sceClibMspaceMalloc(heap,1024*1024)){status=29;goto done;}
    sceClibMspaceFree(heap,chunk);record("PASS mspace_pool_bounds_and_exhaustion\n");
    mutex=sceKernelCreateMutex("kernel_probe_mutex",0,0,NULL);if(mutex<0){status=30;goto done;}
    condition=sceKernelCreateCond("kernel_probe_condition",0,mutex,NULL);if(condition<0){status=31;goto done;}
    thread=sceKernelCreateThread("kernel_probe_worker",worker,0x10000100,16384,0,0,NULL);if(thread<0){status=32;goto done;}
    CHECK(sceKernelStartThread(thread,0,NULL),33);started=1;
    for(unsigned i=0;i<1000 && !atomic_load(&phase);i++)sceKernelDelayThread(1000);
    if(atomic_load(&phase)!=1){status=34;goto done;}
    timeout=1000000;CHECK(sceKernelLockMutex(mutex,1,&timeout),35);
    int signaled=sceKernelSignalCond(condition);int unlocked=sceKernelUnlockMutex(mutex,1);
    if(signaled || unlocked){status=36;goto done;}
    for(unsigned i=0;i<128;i++) {
        CHECK(sceKernelSendSignal(thread),37);timeout=1000000;
        CHECK(sceKernelWaitSignal(0,0,&timeout),38);
    }
    record("PASS 128_signal_mspace_roundtrips\n");
done:
    if(started) {
        unsigned join_timeout=3000000;
        int joined=sceKernelWaitThreadEnd(thread,&worker_status,&join_timeout);
        if(joined) {
            result("FAIL join_unverified",joined);record("Close title: worker lifetime unverified\n");sceIoClose(report);
            for(;;)sceKernelDelayThread(1000000);
        }
        if(worker_status){result("FAIL worker_status",worker_status);status=47;}
        else if(!status && atomic_load(&phase)==2)record("PASS named_condition_release_and_reacquire\n");
    }
    if(thread>=0 && sceKernelDeleteThread(thread))status=48;
    if(condition>=0 && sceKernelDeleteCond(condition))status=49;
    if(mutex>=0 && sceKernelDeleteMutex(mutex))status=50;
    if(heap){sceClibMspaceDestroy(heap);heap=NULL;}
    if(block>=0 && sceKernelFreeMemBlock(block))status=51;
    if(io_failed)status=52;
    result("exit_status",status);record(status ? "FAIL kernel_probe\n" : "PASS kernel_probe\n");
    if(sceIoClose(report)<0 || io_failed)status=52;
    sceKernelExitProcess(status);return status;
}
