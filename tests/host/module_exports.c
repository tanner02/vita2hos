#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

static bool fail_allocation;
static void *test_malloc(size_t size) { return fail_allocation ? NULL : malloc(size); }
#define malloc test_malloc
#include "../../source/module.c"
#undef malloc

EXPORT(TestLibrary, 0x11223344, int, test_export, int value)
{
    return value + 1;
}

DECLARE_LIBRARY(TestLibrary, 0x12345678);
static int guest_function(int value) { return value + 10; }

int main(void)
{
    int (*function)(int) = (int (*)(int))module_get_func_export(0x12345678, 0x11223344);
    assert(function != NULL);
    assert(function(41) == 42);
    assert(module_get_func_export(0x12345678, 0xffffffff) == NULL);
    assert(module_get_func_export(0xffffffff, 0x11223344) == NULL);
    ModuleGuestExports exports = {0};
    int owner1, owner2, variable = 23;
    ModuleGuestSymbol symbols[] = {
        {0x11223344, MODULE_SYMBOL_FUNCTION, (void *)guest_function},
        {0x11223344, MODULE_SYMBOL_VARIABLE, &variable},
    };
    fail_allocation = true;
    assert(module_guest_exports_register(&exports, &owner1, 0x12345678, symbols, 2) != 0);
    assert(exports.libraries == NULL);
    fail_allocation = false;
    assert(module_guest_exports_register(&exports, &owner1, 0x12345678, symbols, 2) == 0);
    /* Caller storage can change or disappear after registration. */
    symbols[0].address = NULL;
    module_set_guest_exports(&exports);
    function = (int (*)(int))module_get_func_export(0x12345678, 0x11223344);
    assert(function && function(2) == 12);
    assert(module_get_var_export(0x12345678, 0x11223344) == &variable);
    assert(module_get_var_export(0x12345678, 0xffffffff) == NULL);
    assert(module_guest_exports_register(&exports, &owner2, 0x12345678, NULL, 0) != 0);
    assert(module_guest_exports_register(&exports, &owner2, 0x9999, symbols, 2) != 0);
    assert(module_guest_exports_register(&exports, &owner2, 0x9999, symbols, SIZE_MAX) != 0);
    symbols[0].address = (void *)guest_function;
    symbols[0].kind = (ModuleSymbolKind)99;
    assert(module_guest_exports_register(&exports, &owner2, 0x9999, symbols, 1) != 0);
    symbols[0].kind = MODULE_SYMBOL_FUNCTION;
    symbols[1] = symbols[0];
    assert(module_guest_exports_register(&exports, &owner2, 0x9999, symbols, 2) != 0);
    assert(module_guest_exports_register(&exports, &owner1, 0x7777, NULL, 0) == 0);
    module_guest_exports_remove(&exports, &owner2);
    assert(module_get_var_export(0x12345678, 0x11223344) == &variable);
    module_guest_exports_remove(&exports, &owner1);
    assert(exports.libraries == NULL);
    assert(module_get_var_export(0x12345678, 0x11223344) == NULL);
    function = (int (*)(int))module_get_func_export(0x12345678, 0x11223344);
    assert(function(2) == 3);
    /* Even an empty guest-owned namespace must not mix in native symbols. */
    assert(module_guest_exports_register(&exports, &owner1, 0x12345678, NULL, 0) == 0);
    assert(module_get_func_export(0x12345678, 0x11223344) == NULL);
    module_guest_exports_clear(&exports);
    assert(module_get_func_export(0x12345678, 0x11223344) != NULL);
    module_guest_exports_clear(&exports);
    puts("Module export regression checks passed.");
    return 0;
}
