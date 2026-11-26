#include <assert.h>
#include <stdio.h>

#include "../../source/modules/SceLibc.c"
#include "../../source/module.c"

static int compare_ints(const void *left, const void *right)
{
    int a = *(const int *)left, b = *(const int *)right;
    return (a > b) - (a < b);
}

int main(void)
{
    void *(*alloc)(size_t) = (void *(*)(size_t))module_get_func_export(0xBE43BB07, 0x775A0CB2);
    void *(*resize)(void *, size_t) =
        (void *(*)(void *, size_t))module_get_func_export(0xBE43BB07, 0x006B54BA);
    void (*release)(void *) = (void (*)(void *))module_get_func_export(0xBE43BB07, 0x5B9BB802);
    void *(*move)(void *, const void *, size_t) =
        (void *(*)(void *, const void *, size_t))module_get_func_export(0xBE43BB07, 0xAF5C218D);
    long (*parse)(const char *, char **, int) =
        (long (*)(const char *, char **, int))module_get_func_export(0xBE43BB07, 0x181827ED);
    void (*sort)(void *, size_t, size_t, int (*)(const void *, const void *)) =
        (void (*)(void *, size_t, size_t, int (*)(const void *, const void *)))
            module_get_func_export(0xBE43BB07, 0xA7CBE4A6);
    char *data = alloc(8);
    char *end;
    assert(data);
    memcpy(data, "abcdef", 7);
    move(data + 1, data, 5);
    assert(memcmp(data, "aabcde", 6) == 0);
    data = resize(data, 32);
    assert(data && memcmp(data, "aabcde", 6) == 0);
    release(data);
    assert(parse("0x2a!", &end, 0) == 42 && *end == '!');
    int values[] = {3, 1, 4, 2};
    sort(values, 4, sizeof(int), compare_ints);
    for (int i = 0; i < 4; i++)
        assert(values[i] == i + 1);
    assert(module_get_func_export(0xBE43BB07, 0xdeadbeef) == NULL);
    puts("SceLibc export regression checks passed.");
    return 0;
}
