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
