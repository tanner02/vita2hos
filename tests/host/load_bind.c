#define TEST_FIXED_MEMORY
#include <assert.h>
#include <stdio.h>
#include <switch.h>
Result jitTransitionToExecutable(Jit *jit);
#include "../../source/module.c"
#include "../../source/load_layout.c"
#include "../../source/load_relocate.c"
#include "../../source/load_var_relocate.c"
#include "../../source/load_bind.c"
#include "../../source/guest_tls.c"
#include "../../source/load_image.c"

EXPORT(TestLibrary, 1, int, test_export, void) { return 0; }
DECLARE_LIBRARY(TestLibrary, 2);
static unsigned transitions,call_reports;
static Result transition_result, close_result;
Result jitTransitionToExecutable(Jit *jit) { (void)jit; ++transitions; return transition_result; }
Result jitClose(Jit *jit) { (void)jit; return close_result; }
Result load_fixed_close(LoadFixedMemory *memory) { (void)memory; return 0; }
void load_fixed_flush(const LoadFixedMemory *memory) { (void)memory; }
void log_printf(const char *format, ...) { if(strstr(format,"UNSUPPORTED_CALL"))++call_reports; }

static void prepare(LoadImage *image, unsigned char *memory, uint32_t provider,
                     uint32_t dependency, uint32_t target)
{
    *image = (LoadImage){.prepared=true, .jit_active=true, .segment_count=1};
    image->segments = calloc(1, sizeof(*image->segments));
    assert(image->segments);
    *image->segments = (segment_info_t){.p_type=PT_LOAD, .p_flags=PF_X,
        .rw_addr=(uintptr_t)memory, .rx_addr=0x1000, .p_memsz=256};
    image->imports = memory;
    image->imports_size = sizeof(sce_module_imports_raw);
    sce_module_imports_raw import = {.size=sizeof(import), .library_nid=dependency,
        .num_syms_funcs=1, .func_nid_table=0x1040, .func_entry_table=0x1044};
    memcpy(memory, &import, sizeof(import));
    uint32_t nid=11, stub=0x1080;
    memcpy(memory+64, &nid, 4); memcpy(memory+68, &stub, 4);
    ModuleGuestSymbol symbols[] = {
        {11, MODULE_SYMBOL_FUNCTION, (void *)(uintptr_t)target},
        {12, MODULE_SYMBOL_VARIABLE, (void *)(uintptr_t)(target + 0x100)},
    };
    assert(module_guest_exports_register(&image->exports, image, provider, symbols, 2) == 0);
}
int main(void)
{
    LoadImage first, second, duplicate;
    unsigned char a[256]={0}, b[256]={0}, c[256]={0};
    ModuleGuestExports process = {0};
    prepare(&first, a, 100, 101, 0x9001);
    prepare(&second, b, 101, 100, 0xa001);
    sce_module_imports_raw first_import;
    memcpy(&first_import, a, sizeof(first_import));
    first_import.num_syms_vars=1; first_import.var_nid_table=0x1048;
    first_import.var_entry_table=0x104c;
    memcpy(a, &first_import, sizeof(first_import));
    uint32_t var_nid=12, reference_address=0x10c0;
    memcpy(a+72, &var_nid, 4); memcpy(a+76, &reference_address, 4);
    uint32_t reference[]={12 << 4, 1 | (R_ARM_ABS32 << 8), 224};
    memcpy(a+192, reference, sizeof(reference));
    assert(transitions == 0); /* Publication does not finalize guest code. */
    assert(load_image_publish(&first, &process) == 0);
    assert(load_image_publish(&second, &process) == 0);
    assert(first.exports.libraries == NULL && second.exports.libraries == NULL);
    assert(load_image_publish(&first, &process) < 0);
    prepare(&duplicate, c, 100, 101, 0xb001);
    assert(load_image_publish(&duplicate, &process) < 0);
    assert(duplicate.exports.libraries && !duplicate.export_registry);
    assert(load_image_close(&duplicate) == 0);
    assert(module_guest_exports_find(&process, 100, MODULE_SYMBOL_FUNCTION, 11, NULL) == (void *)0x9001);
    assert(load_image_bind(&first, &first.exports) < 0);
    first_import.num_syms_tls_vars=1; memcpy(a, &first_import, sizeof(first_import));
    assert(load_image_bind(&first, &process) < 0 && !first.bound && !transitions);
    first_import.num_syms_tls_vars=0; memcpy(a, &first_import, sizeof(first_import));
    var_nid=0xffffffff; memcpy(a+72, &var_nid, 4);
    assert(load_image_bind(&first, &process) < 0 && !first.bound);
    var_nid=12; memcpy(a+72, &var_nid, 4);
    reference_address=0x10fc; memcpy(a+76, &reference_address, 4);
    memcpy(a+252, reference, 4); /* Header fits; full reference table does not. */
    assert(load_image_bind(&first, &process) < 0 && !first.bound);
    reference_address=0x10c0; memcpy(a+76, &reference_address, 4);
    a[192] |= 1; /* Reserved header bits are not accepted. */
    assert(load_image_bind(&first, &process) < 0 && !first.bound);
    a[192] &= ~1;
    assert(load_image_bind(&first, &process) == 0);
    uint32_t variable_value; memcpy(&variable_value, a+224, 4);
    assert(variable_value == 0xa101);
    uint32_t words[3]; memcpy(words, a+128, sizeof(words));
    assert(words[0] == arm_encode_movw(12, 0xa001));
    assert(words[1] == arm_encode_movt(12, 0));
    assert(words[2] == arm_encode_bx(12));
    assert(load_image_bind(&first, &process) < 0); /* Never bind executable code again. */
    unsigned before = transitions;
    b[0] = 1; /* A malformed dependency import must not transition. */
    assert(load_image_bind(&second, &process) < 0 && !second.bound);
    assert(transitions == before);
    b[0] = sizeof(sce_module_imports_raw);
    transition_result = 1;
    assert(load_image_bind(&second, &process) < 0 && !second.bound);
    transition_result = 0;
    assert(load_image_bind(&second, &process) == 0);
    memcpy(words, b+128, sizeof(words));
    assert(words[0] == arm_encode_movw(12, 0x9001));
    close_result = 1;
    first.parameters.guest_address=0x81000020;first.parameters.structure_size=12;
    assert(load_image_close(&first) != 0);
    assert(!first.prepared && !first.bound && !first.segments && !first.entry);
    assert(!first.parameters.guest_address && !first.parameters.structure_size);
    assert(module_guest_exports_find(&process, 100, MODULE_SYMBOL_FUNCTION, 11, NULL) == NULL);
    assert(module_guest_exports_find(&process, 101, MODULE_SYMBOL_FUNCTION, 11, NULL) == (void *)0xa001);
    close_result = 0;
    assert(load_image_close(&first) == 0);
    assert(load_image_close(&second) == 0);
    assert(process.libraries == NULL);
    module_guest_exports_clear(&process);
    assert(load_image_bind(&second, &process) < 0);
    LoadImage missing;unsigned char d[256]={0};
    prepare(&missing,d,102,999,0x9001);
    assert(!load_image_publish(&missing,&process));
    assert(!load_image_bind(&missing,&process));
    uintptr_t trace=missing_import_bind(&missing,999,11,0x1080);assert(trace);
    memcpy(words,d+128,sizeof(words));
    assert(words[0]==arm_encode_movw(12,(uint16_t)trace));
    assert(words[1]==arm_encode_movt(12,(uint16_t)(trace>>16)) && words[2]==arm_encode_bx(12));
    typedef uint64_t (*Trace)(uint32_t,uint32_t,uint32_t,uint32_t);
    close_result=1;assert(load_image_close(&missing)!=0);
    assert((uint32_t)((Trace)trace)(0,7,0,0)==SCE_KERNEL_ERROR_MODULEMGR_NO_FUNC_NID && call_reports==1);
    close_result=0;assert(!load_image_close(&missing));
    trace=missing_import_bind(&missing,999,11,0x1080);
    assert((uint32_t)((Trace)trace)(0,7,0,0)==SCE_KERNEL_ERROR_MODULEMGR_NO_FUNC_NID && call_reports==2);
    missing_import_release(&missing);
    int pool_owner=0;
    for(unsigned i=0;i<MISSING_IMPORT_SLOTS;i++)assert(missing_import_bind(&pool_owner,1000,i,0x2000+i*12));
    LoadImage overflow;unsigned char e[256]={0};prepare(&overflow,e,103,999,0x9001);
    assert(!load_image_publish(&overflow,&process) && !load_image_bind(&overflow,&process));
    memcpy(words,e+128,sizeof(words));
    assert(words[0]==arm_encode_movw(0,(uint16_t)SCE_KERNEL_ERROR_MODULEMGR_NO_FUNC_NID));
    assert(words[1]==arm_encode_movt(0,(uint16_t)(SCE_KERNEL_ERROR_MODULEMGR_NO_FUNC_NID>>16)) && words[2]==arm_encode_ret());
    assert(!load_image_close(&overflow));missing_import_release(&pool_owner);

    /* Standard 16-byte ARM imports can start with MVN/BX followed by NOP/0.
     * Scanning at +8 finds NOP/0/MVN/BX across neighboring entries; this is
     * already covered by their declared metadata, not another import pool. */
    LoadImage adjacent; unsigned char adjacent_memory[256]={0};
    prepare(&adjacent,adjacent_memory,104,104,0xb001);
    sce_module_imports_raw adjacent_import;
    memcpy(&adjacent_import,adjacent_memory,sizeof(adjacent_import));
    adjacent_import.num_syms_funcs=2;
    adjacent_import.func_entry_table=0x1048;
    memcpy(adjacent_memory,&adjacent_import,sizeof(adjacent_import));
    uint32_t adjacent_nids[]={11,11},adjacent_entries[]={0x1080,0x1090};
    const uint32_t initial_stubs[]={0xe3e00000,0xe12fff1e,0xe1a00000,0,
                                    0xe3e00000,0xe12fff1e,0xe1a00000,0};
    const uint32_t unrelated_pattern[]={0xe1a00000,0,0xe3e00000,0xe12fff1e};
    memcpy(adjacent_memory+64,adjacent_nids,sizeof(adjacent_nids));
    memcpy(adjacent_memory+72,adjacent_entries,sizeof(adjacent_entries));
    memcpy(adjacent_memory+128,initial_stubs,sizeof(initial_stubs));
    memcpy(adjacent_memory+192,unrelated_pattern,sizeof(unrelated_pattern));
    assert(!memcmp(adjacent_memory+136,unrelated_pattern,sizeof(unrelated_pattern)));
    assert(!load_image_publish(&adjacent,&process) && !load_image_bind(&adjacent,&process));
    for(unsigned i=0;i<2;++i) {
        memcpy(words,adjacent_memory+128+i*16,sizeof(words));
        assert(words[0]==arm_encode_movw(12,0xb001));
        assert(words[1]==arm_encode_movt(12,0) && words[2]==arm_encode_bx(12));
    }
    assert(memcmp(adjacent_memory+136,unrelated_pattern,sizeof(unrelated_pattern)));
    /* Unlisted matching bytes are preserved; binding follows scoped NIDs. */
    assert(!memcmp(adjacent_memory+192,unrelated_pattern,sizeof(unrelated_pattern)));
    assert(!load_image_close(&adjacent) && process.libraries==NULL);

    puts("Two-phase image binding, dependency publication and teardown checks passed.");
}
