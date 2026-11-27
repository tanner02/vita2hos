#define TEST_FIXED_MEMORY
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include "../../source/modules/SceProcessmgr.c"
#include "../../source/module.c"
DECLARE_LIBRARY(SceThreadmgr,0x859a24b1);
static SceUID next_process_uid=100;
SceUID SceSysmem_get_next_uid(void) { return next_process_uid++; }
static SceUID expected_pid;
static void *read_process_id(void *unused)
{
    (void)unused;
    for (int i=0;i<1000;++i) assert(sceKernelGetProcessId()==expected_pid);
    return NULL;
}

int main(void)
{
    const void *(*get)(void)=(void *)module_get_func_export(0x2DD91812,0x2BE3E066);assert(get);
    SceUID (*get_pid)(void)=(void *)module_get_func_export(0x859a24b1,0x9DCB4B7A);
    assert(get_pid && (uint32_t)get_pid()==SCE_KERNEL_ERROR_UNKNOWN_UID);
    assert(!get());
    segment_info_t segment={.p_type=PT_LOAD,.p_flags=PF_R|PF_X,
        .rx_addr=0x81000000,.rw_addr=0x90000000,.p_memsz=256,.p_filesz=256};
    LoadImage images[2]={{.prepared=true,.bound=true,.segments=&segment,.segment_count=1},
                        {.prepared=true,.bound=true}};
    images[0].parameters=(LoadProcessParameters){.guest_address=0x81000080,.structure_size=0x30,.version=6};
    images[1].parameters=(LoadProcessParameters){.guest_address=0x82000000,.structure_size=0x30,.version=6};
    LoadProcess process={.images=images,.count=2,.bound=true};
    assert(SceProcessmgr_init(NULL)<0 && !get());
    assert(!SceProcessmgr_init(&process) && get()==(void *)0x81000080);
    expected_pid=get_pid();assert(expected_pid==100);
    pthread_t readers[8];
    for (int i=0;i<8;++i) assert(!pthread_create(&readers[i],NULL,read_process_id,NULL));
    for (int i=0;i<8;++i) assert(!pthread_join(readers[i],NULL));
    images[0].parameters.guest_address=0x81000040;
    assert(SceProcessmgr_init(&process)<0 && get()==(void *)0x81000080);
    assert(get_pid()==expected_pid);
    SceProcessmgr_finish();assert(!get() && (uint32_t)get_pid()==SCE_KERNEL_ERROR_UNKNOWN_UID);
    assert(!SceProcessmgr_init(&process) && get()==(void *)0x81000040);
    assert(get_pid()==101 && get_pid()!=expected_pid);
    SceProcessmgr_finish();
    images[0].parameters.guest_address=0x90000080;
    assert(SceProcessmgr_init(&process)<0 && !get()); /* Hidden write alias. */
    images[0].parameters.guest_address=0x810000f0;
    assert(SceProcessmgr_init(&process)<0 && !get()); /* Truncated structure. */
    images[0].parameters.guest_address=0x81000081;
    assert(SceProcessmgr_init(&process)<0 && !get());
    images[0].parameters.guest_address=0;images[0].parameters.structure_size=0;
    assert(!SceProcessmgr_init(&process) && !get()); /* No synthesized placeholder. */
    SceProcessmgr_finish();
    images[0].parameters=(LoadProcessParameters){.guest_address=0x81000080,.structure_size=12};
    assert(!SceProcessmgr_init(&process) && get()==(void *)0x81000080); /* Legacy params. */
    SceProcessmgr_finish();
    segment.p_flags=PF_R|PF_W;segment.rx_addr=segment.rw_addr=0x81879000;
    images[0].parameters.guest_address=0x81879010;
    assert(!SceProcessmgr_init(&process) && get()==(void *)0x81879010);
    SceProcessmgr_finish();assert(!get());
    next_process_uid=-1;
    assert((uint32_t)SceProcessmgr_init(&process)==SCE_KERNEL_ERROR_NO_MEMORY);
    assert(!get() && (uint32_t)get_pid()==SCE_KERNEL_ERROR_UNKNOWN_UID);
    images[0].bound=false;assert(SceProcessmgr_init(&process)<0 && !get());
    puts("Process queries: main parameters, stable UID across readers, reinitialization, allocation failure and teardown passed.");
}
