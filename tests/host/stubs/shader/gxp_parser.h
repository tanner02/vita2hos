#pragma once
#include "spirv_recompiler.h"
namespace shader {
struct TestUniformBuffer { uint32_t index, size; };
struct TestProgramInput { std::vector<TestUniformBuffer> uniform_buffers; };
inline TestProgramInput get_program_input(const SceGxmProgram &)
{
    if (test_conversion_mode == 2) throw std::runtime_error("parser failure");
    if (test_conversion_mode == 1) return {};
    return {{{3, 5}, {14, 3}, {0, 1}, {3, 99}, {15, 99}}};
}
}
