#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
static bool fail_allocation;
static void *allocate(size_t size) { return fail_allocation ? NULL : malloc(size); }
#define malloc allocate
#include "../../source/guest_tp.c"
#undef malloc
int main(void)
{
    uint32_t memory[8]={0};
    segment_info_t segment={.p_type=PT_LOAD,.p_flags=PF_X,.p_filesz=sizeof(memory),
        .p_memsz=sizeof(memory),.rw_addr=(uintptr_t)memory,.rx_addr=0x81000000};
    GuestTpSite sites[]={{0,0,0xee1d0f70,GUEST_TP_ARM},{0,8,0x0f70ee1d,GUEST_TP_THUMB}};
    memory[0]=sites[0].expected;memory[2]=sites[1].expected;
    assert(guest_tp_patch(&segment,1,sites,2)==0);
    assert(memory[0]==0xee1d0f50 && memory[2]==0x0f50ee1d);
    assert(guest_tp_patch(&segment,1,sites,2)<0); /* Original-word matching is strict. */
    for(unsigned condition=0;condition<15;++condition)for(unsigned reg=0;reg<16;++reg){
        uint32_t arm=condition<<28 | 0x0e1d0f70 | reg<<12;
        sites[0]=(GuestTpSite){0,0,arm,GUEST_TP_ARM};memory[0]=arm;
        int result=guest_tp_patch(&segment,1,sites,1);
        if(reg==13 || reg==15)assert(result<0 && memory[0]==arm);
        else assert(result==0 && memory[0]==(arm^0x20));
    }
    for(unsigned reg=0;reg<16;++reg){
        uint32_t thumb=0x0f70ee1d | reg<<28;
        sites[0]=(GuestTpSite){0,0,thumb,GUEST_TP_THUMB};memory[0]=thumb;
        int result=guest_tp_patch(&segment,1,sites,1);
        if(reg==13 || reg==15)assert(result<0 && memory[0]==thumb);
        else assert(result==0 && memory[0]==(thumb^0x200000));
    }
    sites[0]=(GuestTpSite){0,0,0xee1d0f70,GUEST_TP_ARM};
    sites[1]=(GuestTpSite){0,8,0x0f70ee1d,GUEST_TP_THUMB};
    memory[0]=sites[0].expected;memory[2]=0;
    assert(guest_tp_patch(&segment,1,sites,2)<0 && memory[0]==0xee1d0f70);
    memory[2]=sites[1].expected;fail_allocation=true;
    assert(guest_tp_patch(&segment,1,sites,2)<0 && memory[0]==0xee1d0f70);
    fail_allocation=false;
    GuestTpSite bad[]={{0,1,0xee1d0f70,GUEST_TP_ARM},{0,31,0xee1d0f70,GUEST_TP_ARM},
        {1,0,0xee1d0f70,GUEST_TP_ARM},{0,0,0xee1d0f70,99},{0,0,0xfe1d0f70,GUEST_TP_ARM},
        {0,0,0xef000000,GUEST_TP_ARM},{0,0,0xee1d0f50,GUEST_TP_ARM},
        {0,0,0x0f60ee1d,GUEST_TP_THUMB}};
    for(size_t i=0;i<sizeof(bad)/sizeof(*bad);++i) {
        memory[0]=bad[i].expected;
        assert(guest_tp_patch(&segment,1,&bad[i],1)<0 && memory[0]==bad[i].expected);
    }
    memory[0]=sites[0].expected;
    sites[1]=sites[0];
    assert(guest_tp_patch(&segment,1,sites,2)<0 && memory[0]==0xee1d0f70);
    memory[1]=0x0f70; /* Two valid byte patterns overlap at different boundaries. */
    sites[1]=(GuestTpSite){0,2,0x0f70ee1d,GUEST_TP_THUMB};
    assert(guest_tp_patch(&segment,1,sites,2)<0 && memory[0]==0xee1d0f70);
    segment_info_t aliases[]={segment,segment};
    sites[1]=sites[0];sites[1].segment=1;
    assert(guest_tp_patch(aliases,2,sites,2)<0 && memory[0]==0xee1d0f70);
    uint32_t thumb=0x0f70ee1d, patched;
    memcpy((char *)memory+2,&thumb,4);
    sites[1]=(GuestTpSite){0,2,thumb,GUEST_TP_THUMB};
    assert(guest_tp_patch(&segment,1,&sites[1],1)==0);
    memcpy(&patched,(char *)memory+2,4);assert(patched==0x0f50ee1d);
    memory[0]=sites[0].expected;
    segment.p_flags=0;
    assert(guest_tp_patch(&segment,1,sites,1)<0);segment.p_flags=PF_X;
    segment.p_filesz=2;
    assert(guest_tp_patch(&segment,1,sites,1)<0);segment.p_filesz=sizeof(memory);
    segment.rx_addr=UINT32_MAX-1;
    assert(guest_tp_patch(&segment,1,sites,1)<0);segment.rx_addr=0x81000000;
    segment.rw_addr=UINTPTR_MAX-1;
    assert(guest_tp_patch(&segment,1,sites,1)<0);segment.rw_addr=(uintptr_t)memory;
    assert(guest_tp_patch(&segment,1,NULL,0)==0);
    assert(guest_tp_patch(&segment,1,sites,GUEST_TP_MAX_SITES+1)<0);
    assert(guest_tp_patch(NULL,1,sites,1)<0);
    puts("ARM/Thumb thread-pointer patch encoding, bounds and transaction checks passed.");
}
