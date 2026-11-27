#include <gxm/types.h>
#include <shader/spirv_recompiler.h>
#include <spirv_glsl.hpp>
#include <algorithm>
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
    uint32_t *words = nullptr, count = 0;
    require(convert_gxp_to_spirv_c(&words, &count, program, "vertex-locations",
        false, false, false, false, nullptr, 0, false, false, nullptr),
        "vertex location translation failed");
    std::vector<uint32_t> spirv(words, words + count);
    free(words);
    spirv_cross::Compiler reflection(spirv);
    const auto resources = reflection.get_shader_resources();
    for (unsigned i = 0; i < program->parameter_count; ++i) {
        auto &parameter = parameters[i];
        if (parameter.category != SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE) continue;
        require(parameter.resource_index < 64, "unexpected fixture attribute register");
        const auto original_type = parameter.type;
        const auto original_regs = varyings->untyped_pa_regs;
        const uint64_t bit = uint64_t{1} << parameter.resource_index;
        GxmVertexInput input;
        require(gxp_vertex_input_c(program, parameter.resource_index, 99, &input),
                "vertex input metadata failed");
        bool found = false;
        const auto *name = reinterpret_cast<const char *>(&parameter) + parameter.name_offset;
        for (const auto &resource : resources.stage_inputs) {
            if (reflection.get_name(resource.id) == name &&
                reflection.get_decoration(resource.id, spv::DecorationLocation) == input.location)
                found = true;
        }
        require(found, "runtime vertex location disagrees with translated SPIR-V");
        // Metadata-only derivatives: neither guest instructions nor GPU code run.
        varyings->untyped_pa_regs &= ~bit;
        parameter.type = SCE_GXM_PARAMETER_TYPE_F32;
        require(gxp_vertex_input_c(program, parameter.resource_index, i, &input) && !input.integer_input,
                "floating vertex input requires numeric fetch conversion");
        parameter.type = SCE_GXM_PARAMETER_TYPE_S16;
        require(gxp_vertex_input_c(program, parameter.resource_index, i, &input) && input.integer_input,
                "integer vertex input requires integer fetch");
        parameter.type = SCE_GXM_PARAMETER_TYPE_F32;
        varyings->untyped_pa_regs |= bit;
        require(gxp_vertex_input_c(program, parameter.resource_index, i, &input) && input.integer_input,
                "register-format vertex input requires integer fetch");
        parameter.type = original_type;
        varyings->untyped_pa_regs = original_regs;
        ++checked;
    }
    return checked;
}

static void check_stripped_inputs(SceGxmProgram *program)
{
    std::vector<SceGxmVertexAttribute> hints;
    for (unsigned i = 0; i < program->parameter_count; ++i) {
        const auto &parameter = program->program_parameters()[i];
        if (parameter.category != SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE) continue;
        SceGxmVertexAttribute hint{};
        hint.regIndex = parameter.resource_index;
        hint.componentCount = 4;
        hints.push_back(hint);
    }
    require(!hints.empty(), "expected vertex fixture attributes");
    // Reverse hint order to distinguish hint locations from register order.
    std::reverse(hints.begin(), hints.end());
    const auto original_count = program->parameter_count;
    program->parameter_count = 0;
    uint32_t *words = nullptr, count = 0;
    require(convert_gxp_to_spirv_c(&words, &count, program, "stripped-inputs",
        false, false, false, false, hints.data(), hints.size(), false, false, nullptr),
        "stripped shader hint translation failed");
    std::vector<uint32_t> spirv(words, words + count);
    free(words);
    spirv_cross::Compiler reflection(spirv);
    const auto resources = reflection.get_shader_resources();
    for (unsigned i = 0; i < hints.size(); ++i) {
        GxmVertexInput input;
        require(gxp_vertex_input_c(program, hints[i].regIndex, i, &input) &&
                !input.integer_input && input.location == i, "stripped metadata fallback failed");
        bool found = false;
        for (const auto &resource : resources.stage_inputs) {
            if (reflection.get_name(resource.id) == "attribute" + std::to_string(i) &&
                reflection.get_decoration(resource.id, spv::DecorationLocation) == input.location &&
                reflection.get_type(resource.base_type_id).basetype == spirv_cross::SPIRType::Float)
                found = true;
        }
        require(found, "stripped runtime location/type disagrees with translated SPIR-V");
    }
    program->parameter_count = original_count;
}

static void check_packed_inputs(SceGxmProgram *program)
{
    auto *parameters = const_cast<SceGxmProgramParameter *>(program->program_parameters());
    auto *varyings = const_cast<SceGxmProgramVertexVaryings *>(program->vertex_varyings());
    for (unsigned i = 0; i < program->parameter_count; ++i) {
        auto &parameter = parameters[i];
        if (parameter.category != SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE) continue;
        const auto original = parameter;
        const auto original_regs = varyings->untyped_pa_regs;
        require(parameter.resource_index < 64, "unexpected packed fixture register");
        varyings->untyped_pa_regs |= uint64_t{1} << parameter.resource_index;
        struct Case { SceGxmParameterType type; unsigned bytes, arrays, elements; };
        for (const Case c : {Case{SCE_GXM_PARAMETER_TYPE_F16,2,2,6},
                            Case{SCE_GXM_PARAMETER_TYPE_S8,1,2,6},
                            Case{SCE_GXM_PARAMETER_TYPE_F32,4,2,6},
                            Case{SCE_GXM_PARAMETER_TYPE_C10,1,1,4}}) {
            parameter.type = c.type;
            parameter.component_count = 3;
            parameter.array_size = c.arrays;
            GxmVertexInput input;
            require(gxp_vertex_input_c(program, parameter.resource_index, i, &input) &&
                    input.register_format && input.integer_input &&
                    input.register_component_bytes == c.bytes && input.register_components == c.elements,
                    "packed runtime input metadata disagrees with expected register layout");
            uint32_t *words = nullptr, count = 0;
            require(convert_gxp_to_spirv_c(&words, &count, program, "packed-inputs",
                false, false, false, false, nullptr, 0, false, false, nullptr),
                "packed input translation failed");
            std::vector<uint32_t> spirv(words, words + count);
            free(words);
            spirv_cross::Compiler reflection(spirv);
            bool found = false;
            const auto *name = reinterpret_cast<const char *>(&parameter) + parameter.name_offset;
            for (const auto &resource : reflection.get_shader_resources().stage_inputs) {
                if (reflection.get_name(resource.id) != name) continue;
                const auto &type = reflection.get_type(resource.type_id);
                const unsigned slots = (c.elements + 3) / 4;
                require(type.basetype == spirv_cross::SPIRType::UInt && type.width == 32 &&
                        type.vecsize == (c.elements < 4 ? c.elements : 4),
                        "packed shader input must contain unsigned register values");
                require(slots == 1 ? type.array.empty() :
                        (type.array.size() == 1 && type.array[0] == slots),
                        "packed shader input array span disagrees with runtime fetch");
                require(reflection.get_decoration(resource.id, spv::DecorationLocation) == input.location,
                        "packed input location disagrees with runtime fetch");
                found = true;
            }
            require(found, "packed input missing from translated SPIR-V");
            char *glsl = nullptr;
            require(convert_gxp_to_glsl_c(&glsl, program, "packed-inputs", false, false,
                false, false, nullptr, 0, false, false, nullptr),
                "packed input runtime GLSL conversion failed");
            require(glsl && std::string_view(glsl).find("#version") != std::string_view::npos,
                "packed input GLSL source missing");
            free(glsl);
        }
        parameter = original;
        varyings->untyped_pa_regs = original_regs;
        return;
    }
    require(false, "expected an attribute for packed input checks");
}

static bool check_c10_locations(SceGxmProgram *program)
{
    if (program->is_fragment()) return false;
    auto *parameters = const_cast<SceGxmProgramParameter *>(program->program_parameters());
    std::vector<unsigned> attributes;
    for (unsigned i = 0; i < program->parameter_count; ++i)
        if (parameters[i].category == SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE) attributes.push_back(i);
    if (attributes.size() < 2) return false;
    auto &first = parameters[attributes[0]];
    const auto original = first;
    auto *varyings = const_cast<SceGxmProgramVertexVaryings *>(program->vertex_varyings());
    const auto original_regs = varyings->untyped_pa_regs;
    require(first.resource_index < 64, "unexpected C10 fixture register");
    varyings->untyped_pa_regs |= uint64_t{1} << first.resource_index;
    first.type = SCE_GXM_PARAMETER_TYPE_C10;
    first.component_count = 4;
    for (unsigned arrays : {1U, 2U, 4U}) {
        first.array_size = arrays;
        GxmVertexInput a, b;
        require(gxp_vertex_input_c(program, first.resource_index, 0, &a) &&
                gxp_vertex_input_c(program, parameters[attributes[1]].resource_index, 1, &b),
                "C10 adjacent input metadata failed");
        const unsigned slots = (a.register_components + 3) / 4;
        require(a.register_components == arrays * 5 && b.location == a.location + slots,
                "C10 packed bytes must reserve locations before the next input");
        uint32_t *words = nullptr, count = 0;
        require(convert_gxp_to_spirv_c(&words, &count, program, "C10-locations", false, false,
            false, false, nullptr, 0, false, false, nullptr), "C10 location translation failed");
        std::vector<uint32_t> spirv(words, words + count);
        free(words);
        spirv_cross::Compiler reflection(spirv);
        uint64_t used = 0;
        bool first_found = false, second_found = false;
        for (const auto &resource : reflection.get_shader_resources().stage_inputs) {
            if (!reflection.has_decoration(resource.id, spv::DecorationLocation)) continue;
            const auto &type = reflection.get_type(resource.type_id);
            unsigned span = type.columns;
            for (unsigned size : type.array) span *= size;
            const unsigned location = reflection.get_decoration(resource.id, spv::DecorationLocation);
            require(span && location < 64 && span <= 64 - location, "invalid fixture input span");
            const uint64_t mask = (~uint64_t{0} >> (64 - span)) << location;
            require(!(used & mask), "translated C10 input overlaps another input location");
            used |= mask;
            const auto name = reflection.get_name(resource.id);
            for (unsigned index : attributes) {
                const auto &parameter = parameters[index];
                if (name != reinterpret_cast<const char *>(&parameter) + parameter.name_offset) continue;
                GxmVertexInput input;
                require(gxp_vertex_input_c(program, parameter.resource_index, index, &input) &&
                        input.location == location, "C10 runtime location disagrees with SPIR-V");
                if (index == attributes[0]) { require(span == slots, "C10 declaration span mismatch"); first_found = true; }
                if (index == attributes[1]) second_found = true;
            }
        }
        require(first_found && second_found, "C10 adjacency reflection coverage missing");
        char *glsl = nullptr;
        require(convert_gxp_to_glsl_c(&glsl, program, "C10-locations", false, false, false,
            false, nullptr, 0, false, false, nullptr), "C10 runtime GLSL conversion failed");
        free(glsl);
    }
    first = original;
    varyings->untyped_pa_regs = original_regs;
    return true;
}

static bool check_array_locations(SceGxmProgram *program)
{
    if (program->is_fragment()) return false;
    auto *parameters = const_cast<SceGxmProgramParameter *>(program->program_parameters());
    std::vector<unsigned> attributes;
    for (unsigned i = 0; i < program->parameter_count; ++i)
        if (parameters[i].category == SCE_GXM_PARAMETER_CATEGORY_ATTRIBUTE) attributes.push_back(i);
    if (attributes.size() < 2) return false;
    auto &first = parameters[attributes[0]];
    const auto original = first;
    auto *varyings = const_cast<SceGxmProgramVertexVaryings *>(program->vertex_varyings());
    const auto original_regs = varyings->untyped_pa_regs;
    require(first.resource_index < 64, "unexpected array fixture register");
    varyings->untyped_pa_regs &= ~(uint64_t{1} << first.resource_index);
    for (auto kind : {SCE_GXM_PARAMETER_TYPE_F32, SCE_GXM_PARAMETER_TYPE_S16}) {
        first.type = kind;
        for (unsigned components : {1U, 2U, 3U, 4U}) {
            first.component_count = components;
            for (unsigned arrays : {2U, 3U, 4U}) {
                first.array_size = arrays;
                GxmVertexInput a, b;
                require(gxp_vertex_input_c(program, first.resource_index, 0, &a) &&
                        gxp_vertex_input_c(program, parameters[attributes[1]].resource_index, 1, &b),
                        "array runtime input metadata failed");
                require(!a.register_format && a.array_elements == arrays &&
                        b.location == a.location + arrays,
                        "non-register array must reserve a location per element");
                uint32_t *words = nullptr, count = 0;
                require(convert_gxp_to_spirv_c(&words, &count, program, "array-locations", false,
                    false, false, false, nullptr, 0, false, false, nullptr), "array translation failed");
                std::vector<uint32_t> spirv(words, words + count);
                free(words);
                spirv_cross::Compiler reflection(spirv);
                bool found = false;
                uint64_t used = 0;
                for (const auto &resource : reflection.get_shader_resources().stage_inputs) {
                    if (!reflection.has_decoration(resource.id, spv::DecorationLocation)) continue;
                    const auto &type = reflection.get_type(resource.type_id);
                    const unsigned location = reflection.get_decoration(resource.id, spv::DecorationLocation);
                    unsigned span = type.columns;
                    for (auto size : type.array) span *= size;
                    require(span && location < 64 && span <= 64-location, "array input span invalid");
                    const uint64_t mask = (~uint64_t{0} >> (64-span)) << location;
                    require(!(used & mask), "non-register array overlaps adjacent input");
                    used |= mask;
                    if (reflection.get_name(resource.id) !=
                        reinterpret_cast<const char *>(&first) + first.name_offset) continue;
                    require(location == a.location && type.vecsize == components && span == arrays &&
                        type.array.size() == 1 && type.array[0] == arrays &&
                        type.basetype == (kind == SCE_GXM_PARAMETER_TYPE_F32 ?
                            spirv_cross::SPIRType::Float : spirv_cross::SPIRType::Int),
                        "array runtime metadata disagrees with reflected declaration");
                    found = true;
                }
                require(found, "array input missing from SPIR-V");
                char *glsl = nullptr;
                require(convert_gxp_to_glsl_c(&glsl, program, "array-locations", false, false,
                    false, false, nullptr, 0, false, false, nullptr), "array GLSL conversion failed");
                free(glsl);
            }
        }
    }
    first = original;
    varyings->untyped_pa_regs = original_regs;
    return true;
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
        bool stripped_checked = false;
        bool c10_locations_checked = false;
        bool array_locations_checked = false;
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
            if (!c10_locations_checked) c10_locations_checked = check_c10_locations(mutable_program);
            if (!array_locations_checked) array_locations_checked = check_array_locations(mutable_program);
            if (!program->is_fragment() && !stripped_checked) {
                check_stripped_inputs(mutable_program);
                check_packed_inputs(mutable_program);
                stripped_checked = true;
            }
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
        require(c10_locations_checked, "expected C10 input followed by another attribute");
        require(array_locations_checked, "expected non-register array adjacency coverage");
        require(misaligned_checked >= 3, "expected all misaligned uniform lanes");
        require(sampler_checked[0] && sampler_checked[1], "expected both stages' sampler binding coverage");
        require(named_checked >= 2, "expected derived named-buffer coverage");
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
