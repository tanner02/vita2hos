#include <cstring>
#include <cstdlib>
#include <limits>
#include <shader/spirv_recompiler.h>
#include <shader/usse_translator_types.h>
#include <shader/gxp_parser.h>
#include <shader/usse_program_analyzer.h>

#include "vita3k_shader_recompiler_iface_c.h"

extern "C" {

bool gxp_vertex_input_c(const SceGxmProgram *program, uint32_t reg_index,
                        uint32_t hint_index, GxmVertexInput *input)
{
    if (!program || !input || program->is_fragment()) return false;
    try {
        shader::usse::AttributeInformationMap inputs;
        shader::usse::get_attribute_informations(*program, inputs);
        const auto found = inputs.find(reg_index);
        GxmVertexInput result{};
        if (inputs.empty()) {
            result.location = hint_index;
        } else if (found == inputs.end()) {
            result.location = UINT32_MAX;
            result.integer_input = true;
        } else {
            const auto &info = found->second;
            result.location = info.location;
            result.integer_input = info.is_integer || info.regformat;
            result.register_format = info.regformat;
            for (unsigned i = 0; i < program->parameter_count; ++i) {
                const auto &parameter = program->program_parameters()[i];
                if (parameter.category != SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE ||
                    parameter.resource_index != reg_index) continue;
                if (!parameter.array_size || parameter.array_size > 64) return false;
                result.array_elements = parameter.array_size;
                break;
            }
            if (info.regformat) {
                switch (info.gxm_type) {
                case SCE_GXM_PARAMETER_TYPE_U8:
                case SCE_GXM_PARAMETER_TYPE_S8:
                case SCE_GXM_PARAMETER_TYPE_C10: result.register_component_bytes = 1; break;
                case SCE_GXM_PARAMETER_TYPE_U16:
                case SCE_GXM_PARAMETER_TYPE_S16:
                case SCE_GXM_PARAMETER_TYPE_F16: result.register_component_bytes = 2; break;
                case SCE_GXM_PARAMETER_TYPE_F32:
                case SCE_GXM_PARAMETER_TYPE_U32:
                case SCE_GXM_PARAMETER_TYPE_S32: result.register_component_bytes = 4; break;
                default: return false;
                }
                // The analyzer records vector width; the translator also folds
                // array elements into its unsigned register input.
                for (unsigned i = 0; i < program->parameter_count; ++i) {
                    const auto &parameter = program->program_parameters()[i];
                    if (parameter.category != SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE ||
                        parameter.resource_index != reg_index) continue;
                    const uint64_t components = uint64_t(info.component_count) * parameter.array_size;
                    const uint64_t elements = info.gxm_type == SCE_GXM_PARAMETER_TYPE_C10 ?
                        (components * 10 + 7) / 8 : components;
                    if (!elements || elements > 64) return false;
                    result.register_components = static_cast<uint32_t>(elements);
                    break;
                }
                if (!result.register_components) return false;
            }
        }
        *input = result;
        return true;
    } catch (...) {
        return false;
    }
}

bool gxp_uniform_layout_c(const SceGxmProgram *program, GxmUniformLayout *layout)
{
    if (!layout) return false;
    *layout = {};
    if (!program) return false;
    try {
        uint32_t counts[GXM_UNIFORM_LAYOUT_SLOTS] = {};
        bool seen[GXM_UNIFORM_LAYOUT_SLOTS] = {};
        for (const auto &buffer : shader::get_program_input(*program).uniform_buffers) {
            // Match spirv_recompiler.cpp's convert_buffer_idx_to_host and map order.
            // Internal literal/texture buffers are outside this storage container.
            if (buffer.index >= GXM_UNIFORM_LAYOUT_SLOTS) continue;
            unsigned slot = buffer.index < 14 ? buffer.index + 1 : 0;
            if (!seen[slot]) {
                counts[slot] = buffer.size;
                seen[slot] = true;
            }
        }
        return gxm_uniform_layout_build(counts, layout);
    } catch (...) {
        return false;
    }
}

static shader::GeneratedShader
convert_gxp_internal(const SceGxmProgram *program, const char *shader_name,
                     bool support_shader_interlock, bool support_texture_barrier,
                     bool direct_fragcolor, bool spirv_shader,
                     const SceGxmVertexAttribute *hint_attributes, uint32_t num_hint_attributes,
                     bool maskupdate, bool force_shader_debug,
                     bool (*dumper)(const char *ext, const char *dump), shader::Target target)
{
    shader::GeneratedShader shader;
    shader::Hints hints;
    FeatureState features;
    features.support_shader_interlock = support_shader_interlock;
    features.support_texture_barrier = support_texture_barrier;
    features.direct_fragcolor = direct_fragcolor;
    features.spirv_shader = spirv_shader;
    features.use_mask_bit = false;

    std::vector<SceGxmVertexAttribute> hint_attribs;
    for (uint32_t i = 0; i < num_hint_attributes; i++)
        hint_attribs.push_back(hint_attributes[i]);

    hints.attributes = &hint_attribs;
    /* TODO: Fill this properly */
    hints.color_format = SCE_GXM_COLOR_FORMAT_U8U8U8U8_ABGR;
    std::fill_n(hints.vertex_textures, SCE_GXM_MAX_TEXTURE_UNITS,
                SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR);
    std::fill_n(hints.fragment_textures, SCE_GXM_MAX_TEXTURE_UNITS,
                SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR);

    std::function<bool(const std::string &ext, const std::string &dump)> dumper_f =
        [dumper](const std::string &ext, const std::string &dump) {
            if (dumper)
                return dumper(ext.c_str(), dump.c_str());
            return true;
        };

    return shader::convert_gxp(*program, std::string(shader_name), features, target, hints,
                               maskupdate, force_shader_debug, dumper_f);
}

bool convert_gxp_to_spirv_c(uint32_t **spirv, uint32_t *num_instr, const SceGxmProgram *program,
                            const char *shader_name, bool support_shader_interlock,
                            bool support_texture_barrier, bool direct_fragcolor, bool spirv_shader,
                            const SceGxmVertexAttribute *hint_attributes,
                            uint32_t num_hint_attributes, bool maskupdate, bool force_shader_debug,
                            bool (*dumper)(const char *ext, const char *dump))
{
    if (!spirv || !num_instr)
        return false;
    *spirv = nullptr;
    *num_instr = 0;
    if (!program || !shader_name || (num_hint_attributes && !hint_attributes))
        return false;

    try {
        auto shader = convert_gxp_internal(program, shader_name, support_shader_interlock,
                                      support_texture_barrier, direct_fragcolor, spirv_shader,
                                      hint_attributes, num_hint_attributes, maskupdate,
                                      force_shader_debug, dumper, shader::Target::SpirVOpenGL);

        if (shader.spirv.empty() || shader.spirv.size() > std::numeric_limits<uint32_t>::max() ||
            shader.spirv.size() > std::numeric_limits<size_t>::max() / sizeof(uint32_t))
            return false;
        auto *output = static_cast<uint32_t *>(malloc(sizeof(uint32_t) * shader.spirv.size()));
        if (!output)
            return false;
        memcpy(output, shader.spirv.data(), sizeof(uint32_t) * shader.spirv.size());
        *spirv = output;
        *num_instr = static_cast<uint32_t>(shader.spirv.size());

        return true;
    } catch (...) {
        // Recompiler exceptions must not cross the C runtime boundary.
        return false;
    }
}

bool convert_gxp_to_glsl_c(char **glsl, const SceGxmProgram *program, const char *shader_name,
                           bool support_shader_interlock, bool support_texture_barrier,
                           bool direct_fragcolor, bool spirv_shader,
                           const SceGxmVertexAttribute *hint_attributes,
                           uint32_t num_hint_attributes, bool maskupdate, bool force_shader_debug,
                           bool (*dumper)(const char *ext, const char *dump))
{
    if (!glsl)
        return false;
    *glsl = nullptr;
    if (!program || !shader_name || (num_hint_attributes && !hint_attributes))
        return false;

    try {
        auto shader = convert_gxp_internal(program, shader_name, support_shader_interlock,
                                      support_texture_barrier, direct_fragcolor, spirv_shader,
                                      hint_attributes, num_hint_attributes, maskupdate,
                                      force_shader_debug, dumper, shader::Target::GLSLOpenGL);

        if (shader.glsl.empty() || shader.glsl.size() == std::numeric_limits<size_t>::max())
            return false;
        auto *output = static_cast<char *>(malloc(shader.glsl.size() + 1));
        if (!output)
            return false;
        memcpy(output, shader.glsl.c_str(), shader.glsl.size() + 1);
        *glsl = output;

        return true;
    } catch (...) {
        return false;
    }
}
}
