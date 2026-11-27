#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

enum SceGxmParameterType {
    SCE_GXM_PARAMETER_TYPE_F32, SCE_GXM_PARAMETER_TYPE_F16, SCE_GXM_PARAMETER_TYPE_C10,
    SCE_GXM_PARAMETER_TYPE_U32, SCE_GXM_PARAMETER_TYPE_S32, SCE_GXM_PARAMETER_TYPE_U16,
    SCE_GXM_PARAMETER_TYPE_S16, SCE_GXM_PARAMETER_TYPE_U8, SCE_GXM_PARAMETER_TYPE_S8
};
constexpr int SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE = 0;
struct SceGxmProgramParameter { int category; uint32_t resource_index, array_size; };
struct SceGxmProgram {
    bool fragment = false;
    unsigned parameter_count = 1;
    SceGxmProgramParameter parameters[1] = {{SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE,8,1}};
    bool is_fragment() const { return fragment; }
    const SceGxmProgramParameter *program_parameters() const { return parameters; }
};
struct SceGxmVertexAttribute {};
constexpr unsigned SCE_GXM_MAX_TEXTURE_UNITS = 16;
constexpr int SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR = 0;
constexpr int SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR = 0;
struct FeatureState {
    bool support_shader_interlock, support_texture_barrier, direct_fragcolor;
    bool spirv_shader, use_mask_bit;
};
namespace shader {
enum class Target { SpirVOpenGL, GLSLOpenGL };
struct GeneratedShader { std::vector<uint32_t> spirv; std::string glsl; };
struct Hints {
    std::vector<SceGxmVertexAttribute> *attributes;
    int color_format, vertex_textures[16], fragment_textures[16];
};
extern int test_conversion_mode;
inline GeneratedShader convert_gxp(const SceGxmProgram &, const std::string &,
    const FeatureState &, Target, const Hints &, bool, bool,
    const std::function<bool(const std::string &, const std::string &)> &)
{
    if (test_conversion_mode == 1) return {};
    if (test_conversion_mode == 2) throw std::runtime_error("translation failure");
    return {{0x07230203, 123}, "shader source"};
}
}
