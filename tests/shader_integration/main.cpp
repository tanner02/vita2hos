#include <gxm/types.h>
#include <shader/spirv_recompiler.h>
#include <spirv_glsl.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "vita3k_shader_recompiler_iface_c.h"

static void require(bool value, const char *message)
{ if (!value) throw std::runtime_error(message); }

static GxmUniformLayout check_layout(const SceGxmProgram *program, const std::string &name)
{
    GxmUniformLayout layout;
    require(gxp_uniform_layout_c(program, &layout), "uniform layout extraction failed");
    uint32_t *words = nullptr, count = 0;
    require(convert_gxp_to_spirv_c(&words, &count, program, name.c_str(),
        false, false, true, false, nullptr, 0, false, false, nullptr), "SPIR-V translation failed");
    std::vector<uint32_t> spirv(words, words + count);
    free(words);
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
        require(argc == 2, "expected GXP fixture directory");
        unsigned checked = 0;
        unsigned named_checked = 0;
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
            ++checked;
        }
        require(checked >= 7, "expected seven pinned real GXP fixtures");
        require(named_checked >= 2, "expected derived named-buffer coverage");
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
