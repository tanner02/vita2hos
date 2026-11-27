#ifndef VITA3K_SHADER_RECOMPILER_IFACE_C_H
#define VITA3K_SHADER_RECOMPILER_IFACE_C_H
#include "gxm/uniform_layout.h"

#ifdef __cplusplus
extern "C" {
#endif

bool gxp_uniform_layout_c(const SceGxmProgram *program, GxmUniformLayout *layout);
typedef struct GxmVertexInput {
    /* UINT32_MAX means an unreferenced attribute in a named shader. */
    uint32_t location;
    bool integer_input;
} GxmVertexInput;
/* Named inputs use shader declaration locations; stripped shaders use the
 * attribute hint index and float conversion. Failure preserves output. */
bool gxp_vertex_input_c(const SceGxmProgram *program, uint32_t reg_index,
                        uint32_t hint_index, GxmVertexInput *input);

bool convert_gxp_to_spirv_c(uint32_t **spirv, uint32_t *num_instr, const SceGxmProgram *program,
                            const char *shader_name, bool support_shader_interlock,
                            bool support_texture_barrier, bool direct_fragcolor, bool spirv_shader,
                            const SceGxmVertexAttribute *hint_attributes,
                            uint32_t num_hint_attributes, bool maskupdate, bool force_shader_debug,
                            bool (*dumper)(const char *ext, const char *dump));

bool convert_gxp_to_glsl_c(char **glsl, const SceGxmProgram *program, const char *shader_name,
                           bool support_shader_interlock, bool support_texture_barrier,
                           bool direct_fragcolor, bool spirv_shader,
                           const SceGxmVertexAttribute *hint_attributes,
                           uint32_t num_hint_attributes, bool maskupdate, bool force_shader_debug,
                           bool (*dumper)(const char *ext, const char *dump));

#ifdef __cplusplus
}
#endif

#endif
