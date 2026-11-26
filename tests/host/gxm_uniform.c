#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "../../source/gxm_uniform.c"

static uint16_t read_half(const unsigned char *buffer, size_t offset)
{
    uint16_t half;
    memcpy(&half, buffer + offset, sizeof(half));
    return half;
}

int main(void)
{
    unsigned char buffer[64];
    float values[] = {1.0f, -2.0f, 0.5f, 4.0f};
    SceGxmProgramParameter parameter = {
        .category = SCE_GXM_PARAMETER_CATEGORY_UNIFORM,
        .type = SCE_GXM_PARAMETER_TYPE_F16,
        .component_count = 3,
        .array_size = 2,
        .resource_index = 1,
    };

    /* Partial upload crosses a padded half3 array boundary. */
    memset(buffer, 0xa5, sizeof(buffer));
    assert(sceGxmSetUniformDataF(buffer, &parameter, 1, 4, values) == 0);
    assert(read_half(buffer, 6) == 0x3c00);
    assert(read_half(buffer, 8) == 0xc000);
    assert(read_half(buffer, 12) == 0x3800);
    assert(read_half(buffer, 14) == 0x4400);
    assert(buffer[4] == 0xa5 && buffer[10] == 0xa5 && buffer[16] == 0xa5);

    /* Scalar arrays use 32-bit slots, with padding preserved. */
    parameter.component_count = 1;
    parameter.array_size = 4;
    memset(buffer, 0xa5, sizeof(buffer));
    assert(sceGxmSetUniformDataF(buffer, &parameter, 0, 4, values) == 0);
    assert(read_half(buffer, 4) == 0x3c00 && read_half(buffer, 8) == 0xc000);
    assert(read_half(buffer, 12) == 0x3800 && read_half(buffer, 16) == 0x4400);
    assert(buffer[6] == 0xa5 && buffer[18] == 0xa5);

    parameter.type = SCE_GXM_PARAMETER_TYPE_F32;
    parameter.component_count = 3;
    parameter.array_size = 2;
    memset(buffer, 0xa5, sizeof(buffer));
    assert(sceGxmSetUniformDataF(buffer, &parameter, 2, 2, values) == 0);
    assert(memcmp(buffer + 12, &values[0], 4) == 0);
    assert(memcmp(buffer + 20, &values[1], 4) == 0);
    assert(buffer[16] == 0xa5 && buffer[24] == 0xa5);
    parameter.array_size = 1;
    assert(sceGxmSetUniformDataF(buffer, &parameter, 3, 0, values) == 0);
    assert((uint32_t)sceGxmSetUniformDataF(buffer, &parameter, 2, 2, values) ==
           SCE_GXM_ERROR_INVALID_VALUE);
    assert((uint32_t)sceGxmSetUniformDataF(buffer, &parameter, UINT32_MAX, 1, values) ==
           SCE_GXM_ERROR_INVALID_VALUE);
    assert((uint32_t)sceGxmSetUniformDataF(buffer, NULL, 0, 1, values) ==
           SCE_GXM_ERROR_INVALID_POINTER);
    parameter.component_count = 0;
    assert((uint32_t)sceGxmSetUniformDataF(buffer, &parameter, 0, 1, values) ==
           SCE_GXM_ERROR_INVALID_VALUE);
    parameter.component_count = 3;
    parameter.type = SCE_GXM_PARAMETER_TYPE_C10;
    assert((uint32_t)sceGxmSetUniformDataF(buffer, &parameter, 0, 1, values) ==
           SCE_GXM_ERROR_UNSUPPORTED);

    assert(float_to_half(0.0f) == 0);
    assert(float_to_half(-0.0f) == 0x8000);
    assert(float_to_half(INFINITY) == 0x7c00);
    assert(float_to_half(-INFINITY) == 0xfc00);
    assert((float_to_half(NAN) & 0x7fff) > 0x7c00);
    assert(float_to_half(65504.0f) == 0x7bff);
    assert(float_to_half(65520.0f) == 0x7c00);
    assert(float_to_half(0x1p-24f) == 1);
    assert(float_to_half(0x1p-25f) == 0);
    assert(float_to_half(1.0f + 0x1p-11f) == 0x3c00);
    assert(float_to_half(1.0f + 3 * 0x1p-11f) == 0x3c02);

    /* Compare a broad binary32 sample against the host compiler's conversion. */
    uint32_t bits = 0x12345678;
    for (unsigned int i = 0; i < 100000; i++) {
        bits = bits * 1664525U + 1013904223U;
        float value;
        memcpy(&value, &bits, sizeof(value));
        _Float16 reference = (_Float16)value;
        uint16_t expected;
        memcpy(&expected, &reference, sizeof(expected));
        if (!isnan(value))
            assert(float_to_half(value) == expected);
    }
    puts("GXM uniform regression checks passed.");
    return 0;
}
