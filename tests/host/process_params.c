#define TEST_FIXED_MEMORY
#include <assert.h>
#include <stdio.h>
#include "../../source/modules/SceProcessmgr.c"
#include "../../source/module.c"
int main(void)
{
    const void *(*get)(void)=(void *)module_get_func_export(0x2DD91812,0x2BE3E066);assert(get);
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
    images[0].parameters.guest_address=0x81000040;
    assert(SceProcessmgr_init(&process)<0 && get()==(void *)0x81000080);
    SceProcessmgr_finish();assert(!get());
    assert(!SceProcessmgr_init(&process) && get()==(void *)0x81000040);
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
    images[0].bound=false;assert(SceProcessmgr_init(&process)<0 && !get());
    puts("Process-param query: main ownership, guest aliases, range bounds, legacy/absent data and teardown passed.");
}
