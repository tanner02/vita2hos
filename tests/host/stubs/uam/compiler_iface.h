#ifndef TEST_UAM_COMPILER_IFACE_H
#define TEST_UAM_COMPILER_IFACE_H
#include <cstdint>
#include <cstring>
#include "glsl_frontend.h"
extern unsigned int test_compilations, test_outputs;
class DekoCompiler {
public:
    explicit DekoCompiler(pipeline_stage) {}
    bool CompileGlsl(const char *source)
    {
        ++test_compilations;
        return std::strcmp(source, "valid") == 0;
    }
    void OutputDkshToMemory(void *buffer, uint32_t *size)
    {
        ++test_outputs;
        std::memcpy(buffer, "DKSH", 4);
        *size = 4;
    }
};
#endif
