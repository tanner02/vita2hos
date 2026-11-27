#pragma once
#include "spirv_recompiler.h"
#include <map>
namespace shader::usse {
struct AttributeInformation { bool is_integer, regformat; };
using AttributeInformationMap = std::map<int, AttributeInformation>;
inline void get_attribute_informations(const SceGxmProgram &, AttributeInformationMap &output)
{
    if (test_conversion_mode == 2) throw std::runtime_error("attribute parser failure");
    if (test_conversion_mode == 1) return;
    output.emplace(0, AttributeInformation{false, false});
    output.emplace(4, AttributeInformation{true, false});
    output.emplace(8, AttributeInformation{false, true});
}
}
