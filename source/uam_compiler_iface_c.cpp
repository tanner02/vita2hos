#include <uam/compiler_iface.h>

#include "uam_compiler_iface_c.h"

extern "C" {

bool uam_compiler_compile_glsl(pipeline_stage stage, const char *glsl_source, void *buffer,
                               uint32_t *size)
{
    if (size)
        *size = 0;
    if (!glsl_source || !buffer || !size)
        return false;
    DekoCompiler compiler{ stage };
    bool rc = compiler.CompileGlsl(glsl_source);
    if (!rc)
        return false;

    compiler.OutputDkshToMemory(buffer, size);

    return rc;
}
}
