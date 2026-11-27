#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>

unsigned int test_compilations, test_outputs;
int test_output_mode;
uint32_t test_code_size = 8;
std::string test_output_path;
#define UAM_TEMP_FILE_TEMPLATE "/tmp/vita2hos-uam-XXXXXX"
#include "../../source/uam_compiler_iface_c.cpp"

int main()
{
    char buffer[64];
    std::memset(buffer, 0x55, sizeof(buffer));
    uint32_t size = UINT32_MAX;
    assert(!uam_compiler_compile_glsl(pipeline_stage_vertex, "invalid", buffer, sizeof(buffer), &size));
    assert(size == 0 && test_compilations == 1 && test_outputs == 0);
    for (char byte : buffer) assert(byte == 0x55);
    size = UINT32_MAX;
    assert(!uam_compiler_compile_glsl(pipeline_stage_vertex, nullptr, buffer, sizeof(buffer), &size));
    assert(size == 0);
    assert(!uam_compiler_compile_glsl(pipeline_stage_vertex, "valid", nullptr, sizeof(buffer), &size));
    assert(!uam_compiler_compile_glsl(pipeline_stage_vertex, "valid", buffer, sizeof(buffer), nullptr));
    assert(test_compilations == 1 && test_outputs == 0);
    assert(uam_compiler_compile_glsl(pipeline_stage_vertex, "valid", buffer, sizeof(buffer), &size));
    assert(size == 32 && !std::memcmp(buffer, "DKSH", 4));
    assert(test_compilations == 2 && test_outputs == 1);
    assert(access(test_output_path.c_str(), F_OK) != 0);
    std::memset(buffer, 0x55, sizeof(buffer));
    size = 123;
    assert(!uam_compiler_compile_glsl(pipeline_stage_vertex, "valid", buffer, 31, &size));
    assert(size == 0 && access(test_output_path.c_str(), F_OK) != 0);
    for (char byte : buffer) assert(byte == 0x55);
    assert(uam_compiler_compile_glsl(pipeline_stage_vertex, "valid", buffer, 32, &size));
    assert(size == 32);
    for (size_t i = 32; i < sizeof(buffer); ++i) assert(buffer[i] == 0x55);
    for (test_output_mode = 1; test_output_mode <= 3; ++test_output_mode) {
        std::memset(buffer, 0x55, sizeof(buffer));
        size = 123;
        assert(!uam_compiler_compile_glsl(pipeline_stage_vertex, "valid", buffer, sizeof(buffer), &size));
        assert(size == 0 && access(test_output_path.c_str(), F_OK) != 0);
        for (char byte : buffer) assert(byte == 0x55);
    }
    unsigned int calls = test_compilations;
    assert(!uam_compiler_compile_glsl(pipeline_stage_vertex, "valid", buffer, 0, &size));
    assert(size == 0 && test_compilations == calls);
    void *allocated = reinterpret_cast<void *>(1);
    test_output_mode = 0;
    assert(!uam_compiler_compile_glsl_alloc(pipeline_stage_vertex, "invalid", &allocated, &size));
    assert(allocated == nullptr && size == 0);
    test_code_size = 70000; /* Exceeds the runtime's previous shared 64 KiB pool. */
    assert(uam_compiler_compile_glsl_alloc(pipeline_stage_vertex, "valid", &allocated, &size));
    assert(allocated && size == 70024 && !std::memcmp(allocated, "DKSH", 4));
    assert(static_cast<char *>(allocated)[size - 1] == '8');
    std::free(allocated);
    assert(access(test_output_path.c_str(), F_OK) != 0);
    for (test_output_mode = 1; test_output_mode <= 3; ++test_output_mode) {
        allocated = reinterpret_cast<void *>(1);
        size = UINT32_MAX;
        assert(!uam_compiler_compile_glsl_alloc(pipeline_stage_vertex, "valid", &allocated, &size));
        assert(allocated == nullptr && size == 0);
    }
    std::puts("Shader compiler failure-contract checks passed.");
}
