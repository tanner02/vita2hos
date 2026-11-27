#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>

unsigned int test_compilations, test_outputs;
#include "../../source/uam_compiler_iface_c.cpp"

int main()
{
    char buffer[8];
    std::memset(buffer, 0x55, sizeof(buffer));
    uint32_t size = UINT32_MAX;
    assert(!uam_compiler_compile_glsl(pipeline_stage_vertex, "invalid", buffer, &size));
    assert(size == 0 && test_compilations == 1 && test_outputs == 0);
    for (char byte : buffer) assert(byte == 0x55);
    size = UINT32_MAX;
    assert(!uam_compiler_compile_glsl(pipeline_stage_vertex, nullptr, buffer, &size));
    assert(size == 0);
    assert(!uam_compiler_compile_glsl(pipeline_stage_vertex, "valid", nullptr, &size));
    assert(!uam_compiler_compile_glsl(pipeline_stage_vertex, "valid", buffer, nullptr));
    assert(test_compilations == 1 && test_outputs == 0);
    assert(uam_compiler_compile_glsl(pipeline_stage_vertex, "valid", buffer, &size));
    assert(size == 4 && !std::memcmp(buffer, "DKSH", 4));
    assert(test_compilations == 2 && test_outputs == 1);
    std::puts("Shader compiler failure-contract checks passed.");
}
