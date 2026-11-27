#include <gxm/types.h>
#include <shader/spirv_recompiler.h>
#include <spirv_glsl.hpp>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <string_view>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "vita3k_shader_recompiler_iface_c.h"
#include "gxm/texture_binding.h"

static void require(bool value, const char *message)
{ if (!value) throw std::runtime_error(message); }

static unsigned check_vertex_inputs(SceGxmProgram *program)
{
    if (program->is_fragment()) return 0;
    unsigned checked = 0;
    auto *parameters = const_cast<SceGxmProgramParameter *>(program->program_parameters());
    auto *varyings = const_cast<SceGxmProgramVertexVaryings *>(program->vertex_varyings());
    for (unsigned i = 0; i < program->parameter_count; ++i) {
        auto &parameter = parameters[i];
        if (parameter.category != SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE) continue;
        require(parameter.resource_index < 64, "unexpected fixture attribute register");
        const auto original_type = parameter.type;
        const auto original_regs = varyings->untyped_pa_regs;
        const uint64_t bit = uint64_t{1} << parameter.resource_index;
        bool integer = true;
        // Metadata-only derivatives: neither guest instructions nor GPU code run.
        varyings->untyped_pa_regs &= ~bit;
        parameter.type = SCE_GXM_PARAMETER_TYPE_F32;
        require(gxp_vertex_attribute_integer_c(program, parameter.resource_index, &integer) && !integer,
                "floating vertex input requires numeric fetch conversion");
        parameter.type = SCE_GXM_PARAMETER_TYPE_S16;
        require(gxp_vertex_attribute_integer_c(program, parameter.resource_index, &integer) && integer,
                "integer vertex input requires integer fetch");
        parameter.type = SCE_GXM_PARAMETER_TYPE_F32;
        varyings->untyped_pa_regs |= bit;
        require(gxp_vertex_attribute_integer_c(program, parameter.resource_index, &integer) && integer,
                "register-format vertex input requires integer fetch");
        parameter.type = original_type;
        varyings->untyped_pa_regs = original_regs;
        ++checked;
    }
    return checked;
}

static bool check_sampler_bindings(SceGxmProgram *program)
{
    auto *parameters = const_cast<SceGxmProgramParameter *>(program->program_parameters());
    for (unsigned i = 0; i < program->parameter_count; ++i) {
        if (parameters[i].category != SCE_GXM_PARAMETER_CATEGORY_UNIFORM) continue;
        const auto original = parameters[i];
        const std::string sampler_name = std::string(program->is_fragment() ? "fragTex_" : "vertTex_") +
            (reinterpret_cast<const char *>(&parameters[i]) + parameters[i].name_offset);
        parameters[i].category = SCE_GXM_PARAMETER_CATEGORY_SAMPLER;
        parameters[i].array_size = 1;
        // Declaration-only derivatives: no texture-sampling instructions are
        // added and these controlled copies are never used for rendering.
        for (unsigned unit = 0; unit < SCE_GXM_MAX_TEXTURE_UNITS; ++unit) {
            parameters[i].resource_index = unit;
            uint32_t *words = nullptr, count = 0;
            require(convert_gxp_to_spirv_c(&words, &count, program, "sampler-bindings",
                false, false, false, false, nullptr, 0, false, false, nullptr),
                "sampler binding translation failed");
            std::vector<uint32_t> spirv(words, words + count);
            free(words);
            spirv_cross::Compiler reflection(spirv);
            const auto resources = reflection.get_shader_resources();
            const auto expected = gxm_texture_binding_index(!program->is_fragment(), unit);
            bool found = false;
            for (const auto &sampler : resources.sampled_images) {
                if (reflection.get_name(sampler.id) == sampler_name &&
                    reflection.get_decoration(sampler.id, spv::DecorationBinding) == expected)
                    found = true;
            }
            require(found, "runtime sampler slot disagrees with translated SPIR-V");
        }
        parameters[i] = original;
        return true;
    }
    return false;
}

static GxmUniformLayout check_layout(const SceGxmProgram *program, const std::string &name, unsigned misalignment = 0)
{
    GxmUniformLayout layout;
    require(gxp_uniform_layout_c(program, &layout), "uniform layout extraction failed");
    uint32_t *words = nullptr, count = 0;
    require(convert_gxp_to_spirv_c(&words, &count, program, name.c_str(),
        false, false, true, false, nullptr, 0, false, false, nullptr), "SPIR-V translation failed");
    std::vector<uint32_t> spirv(words, words + count);
    free(words);
    if (misalignment) {
        bool first = false, second = false;
        for (size_t i = 5; i < spirv.size();) {
            const unsigned length = spirv[i] >> 16;
            require(length && length <= spirv.size() - i, "invalid SPIR-V instruction span");
            if ((spirv[i] & 0xffff) == spv::OpVectorShuffle && length == 9) {
                bool a = true, b = true;
                for (unsigned j = 0; j < 4; ++j) {
                    const unsigned expected_a = j < misalignment ? j : 4 + j - misalignment;
                    const unsigned expected_b = j < misalignment ? 8 - misalignment + j : j;
                    a &= spirv[i + 5 + j] == expected_a;
                    b &= spirv[i + 5 + j] == expected_b;
                }
                first |= a; second |= b;
            }
            i += length;
        }
        require(first && second, "misaligned uniform shuffles missing or incorrect");
    }
    spirv_cross::Compiler reflection(spirv);
    const auto resources = reflection.get_shader_resources();
    bool found_fragment_control = false;
    for (const auto &resource : resources.uniform_buffers) {
        if (reflection.get_decoration(resource.id, spv::DecorationBinding) != 3) continue;
        found_fragment_control = true;
        const auto &type = reflection.get_type(resource.base_type_id);
        const char *members[] = {"back_disabled", "front_disabled", "writing_mask",
                                 "use_raw_image", "res_multiplier"};
        require(type.member_types.size() == 5, "fragment control block has unexpected members");
        for (unsigned i = 0; i < 5; ++i) {
            require(reflection.get_member_name(type.self, i) == members[i],
                    "fragment control uniform order disagrees with runtime");
            require(reflection.type_struct_member_offset(type, i) == i * sizeof(float),
                    "fragment control uniform offset disagrees with runtime");
        }
    }
    require(!program->is_fragment() || found_fragment_control, "fragment control block missing");
    bool found = false;
    for (const auto &resource : resources.storage_buffers) {
        const auto &type = reflection.get_type(resource.base_type_id);
        require(type.member_types.size() <= 15, "too many uniform container members");
        unsigned member = 0;
        for (unsigned slot = 0; slot < 15; ++slot) {
            if (!layout.bytes[slot]) continue;
            require(member < type.member_types.size(), "missing uniform member");
            require(reflection.get_member_name(type.self, member) == "buffer" + std::to_string(slot),
                "uniform member order disagrees with SPIR-V");
            require(reflection.type_struct_member_offset(type, member) == layout.offsets[slot],
                "uniform member offset disagrees with SPIR-V");
            require(reflection.get_declared_struct_member_size(type, member) ==
                (layout.bytes[slot] + 15U) / 16U * 16U, "uniform member size disagrees with SPIR-V");
            ++member;
        }
        require(member == type.member_types.size(), "unreported uniform member");
        require(reflection.get_declared_struct_size(type) == layout.size,
            "uniform container size disagrees with SPIR-V");
        found = true;
    }
    require(found || layout.size == 0, "uniform storage container missing");
    char *glsl = nullptr;
    require(convert_gxp_to_glsl_c(&glsl, program, name.c_str(), false, false, true,
        false, nullptr, 0, false, false, nullptr), "runtime GLSL translation failed");
    require(glsl && std::string_view(glsl).find("#version") != std::string_view::npos,
        "missing GLSL version declaration");
    free(glsl);
    std::cout << name << ": " << layout.size << " uniform bytes verified\n";
    return layout;
}

int main(int argc, char **argv)
{
    try {
        if ((argc == 3 || argc == 4) && std::string_view(argv[1]) == "--single") {
            const auto size = std::filesystem::file_size(argv[2]);
            require(size >= sizeof(SceGxmProgram) && size <= 64 * 1024 * 1024,
                    "invalid corpus program size");
            std::ifstream input(argv[2], std::ios::binary);
            std::vector<char> data((std::istreambuf_iterator<char>(input)), {});
            require(!input.bad() && data.size() == size, "corpus program read failed");
            const auto *program = reinterpret_cast<const SceGxmProgram *>(data.data());
            require(program->magic == 0x00505847 && program->size == size,
                    "invalid corpus program header");
            auto span = [&](size_t field, uint32_t relative, uint32_t count, size_t stride) {
                if (!count) return;
                require(field <= size && relative <= size - field &&
                        count <= (size - field - relative) / stride, "corpus table out of bounds");
            };
            span(offsetof(SceGxmProgram, parameters_offset), program->parameters_offset,
                 program->parameter_count, sizeof(SceGxmProgramParameter));
            span(offsetof(SceGxmProgram, primary_program_offset), program->primary_program_offset,
                 program->primary_program_instr_count, 8);
            span(offsetof(SceGxmProgram, secondary_program_offset), program->secondary_program_offset,
                 program->secondary_program_instr_count, 8);
            for (uint32_t i = 0; i < program->parameter_count; ++i) {
                const auto *parameter = &program->program_parameters()[i];
                int64_t name = reinterpret_cast<const char *>(parameter) - data.data();
                name += parameter->name_offset;
                require(name >= 0 && static_cast<uint64_t>(name) < size &&
                        std::memchr(data.data() + name, 0, size - name), "corpus name out of bounds");
            }
            GxmUniformLayout layout;
            require(gxp_uniform_layout_c(program, &layout), "uniform layout extraction failed");
            char *glsl = nullptr;
            // Match runtime flags; vertex input hints are intentionally absent.
            require(convert_gxp_to_glsl_c(&glsl, program, "corpus", false, false, false,
                        false, nullptr, 0, false, false, nullptr), "runtime GLSL translation failed");
            require(glsl && *glsl, "empty GLSL translation");
            if (argc == 4) {
                std::ofstream output(argv[3], std::ios::binary | std::ios::trunc);
                output.write(glsl, std::strlen(glsl));
                output.close();
                require(bool(output), "GLSL output write failed");
            }
            std::cout << "TRANSLATED stage=" << (program->is_fragment() ? "fragment" : "vertex")
                      << " uniform_bytes=" << layout.size << " glsl_bytes=" << std::strlen(glsl) << '\n';
            free(glsl);
            return 0;
        }
        require(argc == 2, "expected fixture directory or --single <program.gxp> [output.glsl]");
        unsigned checked = 0;
        unsigned named_checked = 0;
        unsigned misaligned_checked = 0;
        unsigned vertex_inputs_checked = 0;
        bool sampler_checked[2] = {};
        for (const auto &entry : std::filesystem::directory_iterator(argv[1])) {
            if (entry.path().extension() != ".gxp") continue;
            std::ifstream input(entry.path(), std::ios::binary);
            std::vector<char> data((std::istreambuf_iterator<char>(input)), {});
            require(data.size() >= sizeof(SceGxmProgram), "truncated GXP fixture");
            const auto *program = reinterpret_cast<const SceGxmProgram *>(data.data());
            check_layout(program, entry.path().filename().string());
            // Controlled derivatives exercise named slots without adding opaque
            // fixture blobs. These validate container declarations, not rendering.
            auto *mutable_program = reinterpret_cast<SceGxmProgram *>(data.data());
            vertex_inputs_checked += check_vertex_inputs(mutable_program);
            const unsigned stage = program->is_fragment() ? 0 : 1;
            if (!sampler_checked[stage]) sampler_checked[stage] = check_sampler_bindings(mutable_program);
            auto *parameters = const_cast<SceGxmProgramParameter *>(mutable_program->program_parameters());
            for (unsigned i = 0; i < program->parameter_count; ++i) {
                if (parameters[i].category != SCE_GXM_PARAMETER_CATEGORY_UNIFORM) continue;
                auto original = parameters[i].container_index;
                for (unsigned guest_slot : {0U, 13U}) {
                    parameters[i].container_index = guest_slot;
                    auto layout = check_layout(program, entry.path().stem().string() + "-named-" + std::to_string(guest_slot));
                    require(layout.bytes[guest_slot + 1] > 0, "named uniform was not extracted");
                    ++named_checked;
                }
                parameters[i].container_index = original;
                break;
            }
            // Exercise each secondary-register lane using owned pinned fixtures.
            // Translation/reflection only: derivative shader instructions are not executed.
            auto *containers = const_cast<SceGxmProgramParameterContainer *>(program->container());
            for (unsigned i = 0; i < program->container_count; ++i) {
                if (!containers[i].size_in_f32 || containers[i].container_index >= 15) continue;
                const auto original = containers[i].base_sa_offset;
                for (unsigned lane : {1U, 2U, 3U}) {
                    containers[i].base_sa_offset = (original & ~3U) + lane;
                    check_layout(program, entry.path().stem().string() + "-lane-" +
                                 std::to_string(lane), lane);
                    ++misaligned_checked;
                }
                containers[i].base_sa_offset = original;
                break;
            }
            ++checked;
        }
        require(checked >= 7, "expected seven pinned real GXP fixtures");
        require(vertex_inputs_checked >= 1, "expected real vertex input metadata coverage");
        require(misaligned_checked >= 3, "expected all misaligned uniform lanes");
        require(sampler_checked[0] && sampler_checked[1], "expected both stages' sampler binding coverage");
        require(named_checked >= 2, "expected derived named-buffer coverage");
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
