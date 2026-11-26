#include <assert.h>
#include <stdio.h>

#include "../../source/modules/SceLibm.c"
#include "../../source/module.c"

int main(void)
{
    float (*sine)(float) = (float (*)(float))module_get_func_export(0xCDAE3C7D, 0x7F00B590);
    double (*root)(double) = (double (*)(double))module_get_func_export(0xCDAE3C7D, 0xDA227FCC);
    float (*power)(float, float) =
        (float (*)(float, float))module_get_func_export(0xCDAE3C7D, 0x6DEA815A);
    double (*split)(double, double *) =
        (double (*)(double, double *))module_get_func_export(0xCDAE3C7D, 0x1167B5D2);
    float (*decompose)(float, int *) =
        (float (*)(float, int *))module_get_func_export(0xCDAE3C7D, 0x0A6879AC);
    float (*scale)(float, int) =
        (float (*)(float, int))module_get_func_export(0xCDAE3C7D, 0x0E61E016);
    float (*copy_sign)(float, float) =
        (float (*)(float, float))module_get_func_export(0xCDAE3C7D, 0x16EB9E63);
    assert(fabsf(sine(1.57079632679f) - 1.0f) < 1e-6f);
    assert(root(81.0) == 9.0);
    assert(power(2.0f, 5.0f) == 32.0f);
    assert(isnan(root(-1.0)));
    assert(isinf(root(INFINITY)));
    assert(signbit(copy_sign(0.0f, -1.0f)));
    double integer;
    assert(split(-3.25, &integer) == -0.25 && integer == -3.0);
    int exponent;
    assert(decompose(12.0f, &exponent) == 0.75f && exponent == 4);
    assert(scale(0.75f, exponent) == 12.0f);
    assert(module_get_func_export(0xCDAE3C7D, 0xffffffff) == NULL);
    puts("SceLibm export regression checks passed.");
    return 0;
}
