#include <assert.h>
#include <stdio.h>

#include "../../source/module.c"

EXPORT(TestLibrary, 0x11223344, int, test_export, int value)
{
    return value + 1;
}

DECLARE_LIBRARY(TestLibrary, 0x12345678);

int main(void)
{
    int (*function)(int) = (int (*)(int))module_get_func_export(0x12345678, 0x11223344);
    assert(function != NULL);
    assert(function(41) == 42);
    assert(module_get_func_export(0x12345678, 0xffffffff) == NULL);
    assert(module_get_func_export(0xffffffff, 0x11223344) == NULL);
    puts("Module export regression checks passed.");
    return 0;
}
