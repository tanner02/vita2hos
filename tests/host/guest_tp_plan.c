#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
static int fail_at=-1, allocations;
static void *allocate(size_t size) { return allocations++ == fail_at ? NULL : malloc(size); }
#define malloc allocate
#include "../../source/guest_tp.c"
#include "../../source/guest_tp_plan.c"
#undef malloc
static void put32(unsigned char *p,uint32_t value)
{ for(int i=0;i<4;++i)p[i]=(unsigned char)(value>>(i*8)); }
static void put64(unsigned char *p,uint64_t value) { put32(p,value);put32(p+4,value>>32); }
int main(void)
{
    unsigned char plan[65]={0};memcpy(plan,"V2HOSTP1",8);
    put32(plan+8,1);put32(plan+12,2);put64(plan+16,0x123456789abcdef0);put64(plan+24,99);
    put32(plan+40,0xee1d0f70); /* Segment 0, offset 0, ARM. */
    put32(plan+52,8);put32(plan+56,0x0f70ee1d);put32(plan+60,GUEST_TP_THUMB);
    uint32_t code[4]={0xee1d0f70,0,0x0f70ee1d,0};
    segment_info_t segment={.p_type=PT_LOAD,.p_flags=PF_X,.p_filesz=sizeof(code),
        .p_memsz=sizeof(code),.rx_addr=0x81000000,.rw_addr=(uintptr_t)code};
    uint64_t tag=0x123456789abcdef0;
    assert(guest_tp_plan_apply(&segment,1,tag,99,plan,64)==2);
    assert(code[0]==0xee1d0f50 && code[2]==0x0f50ee1d);
    code[0]=0xee1d0f70;code[2]=0x0f70ee1d;
    assert(guest_tp_plan_apply(&segment,1,tag^((uint64_t)1<<40),99,plan,64)<0);
    assert(guest_tp_plan_apply(&segment,1,tag,100,plan,64)<0);
    assert(guest_tp_plan_apply(&segment,1,tag,99+((uint64_t)1<<40),plan,64)<0);
    assert(guest_tp_plan_apply(&segment,1,tag,99,plan,63)<0);
    assert(guest_tp_plan_apply(&segment,1,tag,99,plan,65)<0);
    plan[0]='X';assert(guest_tp_plan_apply(&segment,1,tag,99,plan,64)<0);plan[0]='V';
    put32(plan+8,2);assert(guest_tp_plan_apply(&segment,1,tag,99,plan,64)<0);put32(plan+8,1);
    put32(plan+12,0);assert(guest_tp_plan_apply(&segment,1,tag,99,plan,64)<0);put32(plan+12,2);
    put32(plan+12,UINT32_MAX);assert(guest_tp_plan_apply(&segment,1,tag,99,plan,64)<0);put32(plan+12,2);
    put32(plan+60,99);assert(guest_tp_plan_apply(&segment,1,tag,99,plan,64)<0);put32(plan+60,1);
    assert(code[0]==0xee1d0f70 && code[2]==0x0f70ee1d);
    for(int i=0;i<2;++i){allocations=0;fail_at=i;
        assert(guest_tp_plan_apply(&segment,1,tag,99,plan,64)<0 && code[0]==0xee1d0f70);}
    fail_at=-1;
    char base[]="/tmp/vita2hos-tp-plan-XXXXXX";int descriptor=mkstemp(base);assert(descriptor>=0);
    assert(close(descriptor)==0 && unlink(base)==0);
    assert(guest_tp_plan_file(&segment,1,tag,99,base)==0);
    char filename[128];snprintf(filename,sizeof(filename),"%s.tlsplan",base);
    FILE *file=fopen(filename,"wb");assert(file && fwrite(plan,1,64,file)==64 && fclose(file)==0);
    for(int i=0;i<4;++i){allocations=0;fail_at=i;
        assert(guest_tp_plan_file(&segment,1,tag,99,base)<0 && code[0]==0xee1d0f70);}
    fail_at=-1;
    assert(guest_tp_plan_file(&segment,1,tag,99,base)==2);
    assert(unlink(filename)==0);
    assert(guest_tp_plan_file(&segment,1,tag,99,"/dev/null/invalid")<0);
    assert(guest_tp_plan_file(&segment,1,tag,99,"")<0);
    puts("Source-bound thread-pointer plan parsing, file loading and failure checks passed.");
}
