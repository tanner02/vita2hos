#include <cassert>
#include <cstdlib>
#include <cstring>
#include <shader/spirv_recompiler.h>
int shader::test_conversion_mode;
static bool fail_allocation;
static void *test_malloc(size_t size) { return fail_allocation ? nullptr : std::malloc(size); }
#define malloc test_malloc
#include "../../source/vita3k_shader_recompiler_iface_c.cpp"
#undef malloc

int main()
{
    SceGxmProgram program;
    GxmVertexInput input{};
    input.location = 99;
    input.integer_input = true;
    assert(gxp_vertex_input_c(&program, 0, 7, &input) && !input.integer_input && input.location == 2);
    assert(gxp_vertex_input_c(&program, 4, 7, &input) && input.integer_input && input.location == 0);
    assert(gxp_vertex_input_c(&program, 8, 7, &input) && input.integer_input && input.location == 5);
    assert(input.register_format && input.register_component_bytes == 2 && input.register_components == 4);
    program.parameters[0].array_size = 2;
    assert(gxp_vertex_input_c(&program, 8, 7, &input) && input.register_components == 8);
    program.parameters[0].array_size = 0;
    assert(!gxp_vertex_input_c(&program, 8, 7, &input) && input.register_components == 8);
    program.parameters[0].array_size = UINT32_MAX;
    assert(!gxp_vertex_input_c(&program, 8, 7, &input) && input.register_components == 8);
    program.parameters[0].array_size = 1;
    program.parameters[0].resource_index = 0;
    program.parameters[0].array_size = 3;
    assert(gxp_vertex_input_c(&program, 0, 7, &input) && !input.register_format && input.array_elements == 3);
    program.parameters[0].array_size = 0;
    assert(!gxp_vertex_input_c(&program, 0, 7, &input) && input.array_elements == 3);
    program.parameters[0].array_size = UINT32_MAX;
    assert(!gxp_vertex_input_c(&program, 0, 7, &input) && input.array_elements == 3);
    program.parameters[0].resource_index = 8;
    program.parameters[0].array_size = 1;
    assert(gxp_vertex_input_c(&program, 99, 7, &input) && input.location == UINT32_MAX);
    shader::test_conversion_mode = 1;
    assert(gxp_vertex_input_c(&program, 0, 7, &input) && !input.integer_input && input.location == 7);
    shader::test_conversion_mode = 2;
    assert(!gxp_vertex_input_c(&program, 0, 8, &input) && !input.integer_input && input.location == 7);
    program.fragment = true;
    assert(!gxp_vertex_input_c(&program, 0, 8, &input) && input.location == 7);
    program.fragment = false;
    assert(!gxp_vertex_input_c(nullptr, 0, 8, &input) && input.location == 7);
    assert(!gxp_vertex_input_c(&program, 0, 8, nullptr));
    shader::test_conversion_mode = 0;
    GxmUniformLayout layout;
    assert(gxp_uniform_layout_c(&program, &layout));
    assert(layout.bytes[0] == 12 && layout.bytes[1] == 4 && layout.bytes[4] == 20);
    assert(layout.offsets[4] == 32 && layout.size == 64);
    shader::test_conversion_mode = 1;
    assert(gxp_uniform_layout_c(&program, &layout) && layout.size == 0);
    shader::test_conversion_mode = 2;
    assert(!gxp_uniform_layout_c(&program, &layout) && layout.size == 0);
    assert(!gxp_uniform_layout_c(nullptr, &layout));
    assert(!gxp_uniform_layout_c(&program, nullptr));
    shader::test_conversion_mode = 0;
    uint32_t *spirv = nullptr, count = 0;
    char *glsl = nullptr;
    auto convert_spirv = [&] { return convert_gxp_to_spirv_c(&spirv, &count, &program,
        "test", false, false, false, false, nullptr, 0, false, false, nullptr); };
    auto convert_glsl = [&] { return convert_gxp_to_glsl_c(&glsl, &program,
        "test", false, false, false, false, nullptr, 0, false, false, nullptr); };
    assert(convert_spirv() && count == 2 && spirv[0] == 0x07230203);
    std::free(spirv);
    assert(convert_glsl() && !std::strcmp(glsl, "shader source"));
    std::free(glsl);
    for (int mode = 0; mode <= 2; ++mode) {
        shader::test_conversion_mode = mode;
        fail_allocation = mode == 0;
        spirv = reinterpret_cast<uint32_t *>(1); count = 99;
        glsl = reinterpret_cast<char *>(1);
        assert(!convert_spirv() && !spirv && count == 0);
        assert(!convert_glsl() && !glsl);
    }
    assert(!convert_gxp_to_glsl_c(&glsl, nullptr, "test", false, false, false,
        false, nullptr, 0, false, false, nullptr) && !glsl);
    assert(!convert_gxp_to_spirv_c(&spirv, &count, &program, "test", false, false,
        false, false, nullptr, 1, false, false, nullptr) && !spirv && count == 0);
}
