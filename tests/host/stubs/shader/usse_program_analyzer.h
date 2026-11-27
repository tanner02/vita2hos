#pragma once
#include "spirv_recompiler.h"
#include <map>
namespace shader::usse {
struct AttributeInformation { uint16_t location; bool is_integer, regformat; SceGxmParameterType gxm_type; uint8_t component_count; };
using AttributeInformationMap = std::map<int, AttributeInformation>;
inline void get_attribute_informations(const SceGxmProgram &, AttributeInformationMap &output)
{
    if (test_conversion_mode == 2) throw std::runtime_error("attribute parser failure");
    if (test_conversion_mode == 1) return;
    output.emplace(0, AttributeInformation{2, false, false,SCE_GXM_PARAMETER_TYPE_F32,4});
    output.emplace(4, AttributeInformation{0, true, false,SCE_GXM_PARAMETER_TYPE_S16,4});
    output.emplace(8, AttributeInformation{5, false, true,SCE_GXM_PARAMETER_TYPE_F16,4});
}
}
