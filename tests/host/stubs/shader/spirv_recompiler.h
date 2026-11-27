#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

struct SceGxmProgram { bool fragment = false; bool is_fragment() const { return fragment; } };
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
