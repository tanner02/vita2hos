#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include "../../source/modules/SceFios2Kernel.c"
#include "../../source/module.c"
static SceUID current_pid=100;
SceUID sceKernelGetProcessId(void) { return current_pid; }
void log_printf(const char *format,...) { (void)format; }
int main(void)
{
    int (*list)(SceUID,SceUInt8,SceUInt8,SceFiosGetListSyscallArgs *)=
        (void *)module_get_func_export(0x8757B742,0x9379E2D5);
    int (*scheduler)(int,const char *,SceUInt64 *)=
        (void *)module_get_func_export(0x8757B742,0xB02E0B26);
    assert(list && scheduler);
    int flags=MAP_PRIVATE|MAP_ANONYMOUS;
#ifdef MAP_32BIT
    flags|=MAP_32BIT;
#endif
    uint32_t *memory=mmap((void *)0x20000000,4096,PROT_READ|PROT_WRITE,flags,-1,0);
    assert(memory!=MAP_FAILED && (uintptr_t)memory+4095<=UINT32_MAX);
    memset(memory,0xa5,4096);
    uint32_t *count=memory+140,*packet=memory+512;
    uint32_t count_address=(uint32_t)(uintptr_t)count;
    uint32_t ids_address=(uint32_t)(uintptr_t)memory;
    SceFiosGetListSyscallArgs *args=(void *)packet;
    /* The real initializer: no ID array, zero capacity, count pointer at +8.
     * Reserved words are unspecified stack contents and must be ignored. */
    packet[0]=0;packet[1]=0;packet[2]=count_address;packet[3]=0;
    packet[4]=0xdeadbeef;packet[5]=0xcafebabe;
    assert(!list(current_pid,0,255,args) && !*count);
    const uint32_t capacities[]={0,1,128,129,UINT32_MAX};
    for (unsigned i=0;i<sizeof(capacities)/sizeof(capacities[0]);++i) {
        packet[0]=ids_address;packet[1]=capacities[i];
        packet[3]=(capacities[i]>128 ? 128 : capacities[i])*4;
        uint32_t before[6];memcpy(before,packet,sizeof(before));*count=0x5a5a5a5a;
        assert(!list(0,17,200,args) && !*count); /* Self selector. */
        assert(!memcmp(before,packet,sizeof(before)));
        for (unsigned j=0;j<128;++j) assert(memory[j]==0xa5a5a5a5);
        assert(count[-1]==0xa5a5a5a5 && count[1]==0xa5a5a5a5);
    }
    packet[1]=1;packet[3]=4;*count=123;
    assert((uint32_t)list(100,0,255,NULL)==SCE_KERNEL_ERROR_ILLEGAL_ADDR && *count==123);
    assert((uint32_t)list(100,200,17,args)==SCE_KERNEL_ERROR_INVALID_ARGUMENT && *count==123);
    assert((uint32_t)list(101,0,255,args)==SCE_KERNEL_ERROR_UNKNOWN_UID && *count==123);
    current_pid=(SceUID)SCE_KERNEL_ERROR_UNKNOWN_UID;
    assert((uint32_t)list(0,0,255,args)==SCE_KERNEL_ERROR_UNKNOWN_UID && *count==123);
    current_pid=100;packet[0]=0;
    assert((uint32_t)list(100,0,255,args)==SCE_KERNEL_ERROR_ILLEGAL_ADDR && *count==123);
    packet[0]=ids_address+1;
    assert((uint32_t)list(100,0,255,args)==SCE_KERNEL_ERROR_ILLEGAL_ALIGNMENT && *count==123);
    packet[0]=ids_address;packet[2]=0;
    assert((uint32_t)list(100,0,255,args)==SCE_KERNEL_ERROR_ILLEGAL_ADDR && *count==123);
    packet[2]=count_address+1;
    assert((uint32_t)list(100,0,255,args)==SCE_KERNEL_ERROR_ILLEGAL_ALIGNMENT && *count==123);
    packet[2]=count_address;packet[3]=3;
    assert((uint32_t)list(100,0,255,args)==SCE_KERNEL_ERROR_ILLEGAL_SIZE && *count==123);
    packet[0]=0;packet[1]=0;packet[2]=(uint32_t)(uintptr_t)(packet+3);packet[3]=0x12345678;
    assert(!list(100,0,255,args) && !packet[3]); /* Copy-in before aliased output. */
    SceUInt64 options=UINT64_C(0x12345678abcdef01);
    assert((uint32_t)scheduler(2,"host0:",NULL)==SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    assert(scheduler(2,"host0:",&options)==1 && scheduler(2,"host9:/file",&options)==1);
    assert(!scheduler(0,"host0:",&options) && !scheduler(1,"host0:",&options));
    assert(!scheduler(-1,"host0:",&options) && !scheduler(2,NULL,&options));
    const char *paths[]={"","h","hos","host","host:","host0","host10:","HOST0:","hosta:","host0/","app0:/file"};
    for (unsigned i=0;i<sizeof(paths)/sizeof(paths[0]);++i) assert(!scheduler(2,paths[i],&options));
    assert(options==UINT64_C(0x12345678abcdef01));
    assert(!munmap(memory,4096));
    puts("FIOS initial discovery: packed 32-bit query ABI, empty state, process/range/buffer checks, aliasing and scheduler selection passed.");
}
