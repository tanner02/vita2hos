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
    puts("guest export decoding tests passed");
}
