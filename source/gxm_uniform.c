#include <stdint.h>
#include <string.h>

#include "gxm/program_parameter.h"
#include "module.h"

/* IEEE binary32 to binary16, rounding to nearest with ties to even. */
static uint16_t float_to_half(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    uint32_t sign = (bits >> 16) & 0x8000;
    uint32_t exponent = (bits >> 23) & 0xff;
    uint32_t mantissa = bits & 0x7fffff;

    if (exponent == 255)
        return sign | 0x7c00 | (mantissa ? 0x200 : 0);
    if (exponent > 142)
        return sign | 0x7c00;
    if (exponent < 102)
        return sign;

    if (exponent < 113) {
        mantissa |= 0x800000;
        uint32_t shift = 126 - exponent;
        uint32_t result = mantissa >> shift;
        uint32_t remainder = mantissa & ((1U << shift) - 1);
        uint32_t halfway = 1U << (shift - 1);
        return sign | (result + (remainder > halfway ||
                                 (remainder == halfway && (result & 1))));
    }

    uint32_t result = ((exponent - 112) << 10) | (mantissa >> 13);
    uint32_t remainder = mantissa & 0x1fff;
    result += remainder > 0x1000 || (remainder == 0x1000 && (result & 1));
    return sign | result;
}

EXPORT(SceGxm, 0x65DD0C84, int, sceGxmSetUniformDataF, void *uniformBuffer,
       const SceGxmProgramParameter *parameter, unsigned int componentOffset,
       unsigned int componentCount, const float *sourceData)
{
    if (!uniformBuffer || !parameter || !sourceData)
        return SCE_GXM_ERROR_INVALID_POINTER;
    if (parameter->category != SCE_GXM_PARAMETER_CATEGORY_UNIFORM ||
        parameter->component_count == 0 || parameter->component_count > 4 ||
        parameter->array_size == 0 || parameter->resource_index < 0)
        return SCE_GXM_ERROR_INVALID_VALUE;

    if (parameter->type != SCE_GXM_PARAMETER_TYPE_F32 &&
        parameter->type != SCE_GXM_PARAMETER_TYPE_F16)
        return SCE_GXM_ERROR_UNSUPPORTED;

    const uint64_t total = (uint64_t)parameter->component_count * parameter->array_size;
    if (componentOffset > total || componentCount > total - componentOffset)
        return SCE_GXM_ERROR_INVALID_VALUE;

    const uint32_t scalar_size = parameter->type == SCE_GXM_PARAMETER_TYPE_F16 ? 2 : 4;
    uint32_t stride = scalar_size * parameter->component_count;
    if (parameter->component_count == 1)
        stride = 4; /* Scalar array elements occupy individual 32-bit slots. */
    else if (parameter->array_size > 1)
        stride = (stride + 7) & ~7U;

    const uint64_t base = (uint64_t)parameter->resource_index * 4;
    const uint64_t end = base + (uint64_t)(parameter->array_size - 1) * stride +
                         parameter->component_count * scalar_size;
    if (end > SIZE_MAX)
        return SCE_GXM_ERROR_INVALID_VALUE;

    for (uint32_t i = 0; i < componentCount; i++) {
        uint64_t component = (uint64_t)componentOffset + i;
        size_t offset = (size_t)(base + (component / parameter->component_count) * stride +
                                (component % parameter->component_count) * scalar_size);
        if (scalar_size == 2) {
            uint16_t half = float_to_half(sourceData[i]);
            memcpy((char *)uniformBuffer + offset, &half, sizeof(half));
        } else {
            memcpy((char *)uniformBuffer + offset, &sourceData[i], sizeof(float));
        }
    }
    return 0;
}
