#include <assert.h>
#include <stdio.h>
#include "../../source/module.c"
#include "../../source/load_exports.c"

EXPORT(TestLibrary, 1, int, test_export, void) { return 0; }
DECLARE_LIBRARY(TestLibrary, 2);
static unsigned char memory[256];
static void put32(unsigned char *p, uint32_t v)
{
    for (int i = 0; i < 4; ++i) p[i] = v >> (i * 8);
}
static const void *read_memory(void *context, uint32_t address, size_t size, bool executable)
{
    assert(context == memory);
    if (address < 0x1000 || (uint64_t)address + size > 0x1100) return NULL;
    if (executable && (address < 0x1080 || (uint64_t)address + size > 0x10a0)) return NULL;
    return memory + address - 0x1000;
}
static void record(unsigned char *p, uint32_t library)
{
    memset(p, 0, 32);
    p[0] = 32; p[6] = 1;
    put32(p + 8, 1); put32(p + 16, library);
    put32(p + 24, 0x1000); put32(p + 28, 0x1010);
}
int main(void)
{
    ModuleGuestExports exports = {0};
    unsigned char records[64]; int owner;
    put32(memory, 11); put32(memory + 4, 12);
    put32(memory + 16, 0x1081); put32(memory + 20, 0x10c1);
    record(records, 100);
    assert(load_exports_register(&exports, &owner, records, 32, read_memory, memory) == 0);
    assert(module_guest_exports_find(&exports, 100, MODULE_SYMBOL_FUNCTION, 11, NULL) == (void *)0x1081);
    assert(module_guest_exports_find(&exports, 100, MODULE_SYMBOL_VARIABLE, 12, NULL) == (void *)0x10c1);
    ModuleGuestLibrary *original = exports.libraries;
    assert(load_exports_register(&exports, &owner, records, 32, read_memory, memory) < 0);
    assert(exports.libraries == original);
    record(records, 101); record(records + 32, 102);
    records[32] = 31;
    assert(load_exports_register(&exports, &owner, records, 64, read_memory, memory) < 0);
    bool owned = true;
    module_guest_exports_find(&exports, 101, MODULE_SYMBOL_FUNCTION, 11, &owned);
    assert(!owned && exports.libraries == original);
    record(records, 101); record(records + 32, 100);
    assert(load_exports_register(&exports, &owner, records, 64, read_memory, memory) < 0);
    module_guest_exports_find(&exports, 101, MODULE_SYMBOL_FUNCTION, 11, &owned);
    assert(!owned && exports.libraries == original);
    record(records, 101);
    put32(records + 12, 1); /* TLS is not an ordinary variable. */
    ModuleGuestExports tls_exports={0};
    assert(load_exports_register(&tls_exports, &owner, records, 32, read_memory, memory) == 0);
    assert(module_guest_exports_find(&tls_exports, 101, MODULE_SYMBOL_FUNCTION, 11, NULL));
    assert(tls_exports.libraries->count == 2); /* Trailing TLS entry is not published. */
    module_guest_exports_clear(&tls_exports);
    put32(records + 24, 0x10f8); /* Ordinary entries fit; trailing TLS entry does not. */
    assert(load_exports_register(&exports, &owner, records, 32, read_memory, memory) < 0);
    put32(records + 24, 0x1000);
    put32(records + 12, UINT32_MAX);
    assert(load_exports_register(&exports, &owner, records, 32, read_memory, memory) < 0);
    record(records, 101);
    put32(records + 8, UINT32_MAX);
    assert(load_exports_register(&exports, &owner, records, 32, read_memory, memory) < 0);
    record(records, 101);
    put32(records + 24, 0x10fc); /* Table straddles the mapped range. */
    assert(load_exports_register(&exports, &owner, records, 32, read_memory, memory) < 0);
    record(records, 101);
    put32(memory + 16, 0x10c0); /* Readable data is not executable. */
    assert(load_exports_register(&exports, &owner, records, 32, read_memory, memory) < 0);
    put32(memory + 16, 0x1082); /* Misaligned ARM target. */
    assert(load_exports_register(&exports, &owner, records, 32, read_memory, memory) < 0);
    put32(memory + 16, 0x1080);
    record(records + 32, 102);
    assert(load_exports_register(&exports, &owner, records, 64, read_memory, memory) == 0);
    assert(load_exports_register(&exports, &owner, records, 33, read_memory, memory) < 0);
    module_guest_exports_clear(&exports);
    record(records, 0); /* Lifecycle table is deliberately not registered. */
    assert(load_exports_register(&exports, &owner, records, 32, read_memory, memory) == 0);
    assert(exports.libraries == NULL);
    LoadLifecycle lifecycle = {0};
    put32(memory, NID_MODULE_START); put32(memory + 4, NID_MODULE_STOP);
    record(records, 0); records[6] = 2; put32(records + 8, 0);
    put32(memory + 16, 0x1081); put32(memory + 20, 0x1084);
    assert(load_exports_lifecycle(&lifecycle, records, 32, read_memory, memory) == 0);
    assert(lifecycle.start == (void *)0x1081 && lifecycle.stop == (void *)0x1084 && !lifecycle.exit);
    LoadLifecycle saved = lifecycle;
    put32(memory + 4, NID_MODULE_START); /* Ambiguous duplicate start. */
    assert(load_exports_lifecycle(&lifecycle, records, 32, read_memory, memory) < 0);
    assert(memcmp(&lifecycle, &saved, sizeof(saved)) == 0);
    put32(memory + 4, NID_MODULE_EXIT);
    put32(memory + 20, 0x10c0); /* Lifecycle entry in non-executable data. */
    assert(load_exports_lifecycle(&lifecycle, records, 32, read_memory, memory) < 0);
    put32(memory + 20, 0x1084);
    assert(load_exports_lifecycle(&lifecycle, records, 32, read_memory, memory) == 0);
    assert(lifecycle.exit == (void *)0x1084 && !lifecycle.stop);
    records[6] = 1; put32(records + 8, 1); /* Lifecycle function listed as a variable. */
    assert(load_exports_lifecycle(&lifecycle, records, 32, read_memory, memory) < 0);
    record(records, 0); records[6] = 0; put32(records + 8, 0);
    assert(load_exports_lifecycle(&lifecycle, records, 32, read_memory, memory) == 0);
    assert(!lifecycle.start && !lifecycle.stop && !lifecycle.exit);
    assert(load_exports_lifecycle(&lifecycle, records, 31, read_memory, memory) < 0);
    put32(records + 12, 1);
    assert(load_exports_lifecycle(&lifecycle, records, 32, read_memory, memory) < 0);
    record(records, 0); put32(records + 24, 0x10fc);
    assert(load_exports_lifecycle(&lifecycle, records, 32, read_memory, memory) < 0);
    memset(memory, 0, sizeof(memory));
    put32(memory, 0x30); put32(memory+4, 0x32505350); put32(memory+8, 6);
    put32(memory+0x2c, 0x10c0); put32(memory+192, 0x38);
    ModuleGuestExports parameters={0};
    assert(load_process_parameters_register(&parameters, &owner, 0x1000, read_memory, memory) == 0);
    assert(module_guest_exports_find(&parameters, 0x5ad9c136, MODULE_SYMBOL_VARIABLE,
        0xdf084dfa, NULL) == (void *)0x10c0);
    module_guest_exports_clear(&parameters);
    put32(memory+0x14,0x1080);put32(memory+0x18,0x1084);put32(memory+0x24,0x1088);
    put32(memory+128,0x10000103);put32(memory+132,0x40000);put32(memory+136,0x210000);
    LoadProcessParameters settings;
    assert(load_process_parameters_read(&settings,0x1000,read_memory,memory)==0);
    assert(settings.has_main_priority && settings.main_priority==0x10000103);
    assert(settings.has_main_stack_size && settings.main_stack_size==0x40000);
    assert(settings.has_preload_disabled && settings.preload_disabled==0x210000);
    assert(settings.libc_address==0x10c0);
    assert(settings.version==6);
    put32(memory+0x18,0x10fe);
    assert(load_process_parameters_read(&settings,0x1000,read_memory,memory)<0);
    assert(settings.main_priority==0x10000103 && settings.main_stack_size==0x40000);
    put32(memory+0x18,0x1100);
    assert(load_process_parameters_read(&settings,0x1000,read_memory,memory)<0);
    put32(memory+0x14,0);put32(memory+0x18,0);put32(memory+0x24,0);
    assert(load_process_parameters_read(&settings,0x1000,read_memory,memory)==0);
    assert(!settings.has_main_priority && !settings.has_main_stack_size && !settings.has_preload_disabled);
    put32(memory+0x2c, 0x10fc); put32(memory+252, 8);
    assert(load_process_parameters_register(&parameters, &owner, 0x1000, read_memory, memory) < 0);
    assert(!parameters.libraries);
    put32(memory+0x2c, 0); /* Optional libc parameters. */
    assert(load_process_parameters_register(&parameters, &owner, 0x1000, read_memory, memory) == 0);
    put32(memory, 0x1000);
    assert(load_process_parameters_register(&parameters, &owner, 0x1000, read_memory, memory) < 0);
    put32(memory, 0x30); put32(memory+8, 7);
    assert(load_process_parameters_register(&parameters, &owner, 0x1000, read_memory, memory) < 0);
    put32(memory+8, 0);
    assert(load_process_parameters_read(&settings,0x1000,read_memory,memory)==0 && !settings.version);
    assert(load_process_parameters_register(&parameters, &owner, 0x1000, read_memory, memory) == 0);
    put32(memory+4, 0);
    assert(load_process_parameters_register(&parameters, &owner, 0x1000, read_memory, memory) < 0);
    memset(memory, 0, sizeof(memory)); record(records, 0);
    records[6]=0; put32(records+8, 1);
    put32(memory, NID_PROCESS_PARAM); put32(memory+16, 0x10c0);
    uint32_t process_address=0;
    assert(load_exports_metadata(&lifecycle, &process_address, records, 32, read_memory, memory) == 0);
    assert(process_address == 0x10c0 && !lifecycle.start);
    put32(records+8, 2); put32(memory+4, NID_PROCESS_PARAM); put32(memory+20, 0x10c4);
    assert(load_exports_metadata(&lifecycle, &process_address, records, 32, read_memory, memory) < 0);
    assert(process_address == 0x10c0); /* Failed decode does not publish partial metadata. */
    put32(records+8, 0); records[6]=1; put32(memory+16, 0x1080);
    assert(load_exports_metadata(&lifecycle, &process_address, records, 32, read_memory, memory) < 0);
    puts("guest export decoding tests passed");
}
