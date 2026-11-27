#ifndef TEST_UAM_COMPILER_IFACE_H
#define TEST_UAM_COMPILER_IFACE_H
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include "glsl_frontend.h"
extern unsigned int test_compilations, test_outputs;
extern int test_output_mode;
extern std::string test_output_path;
class DekoCompiler {
public:
    explicit DekoCompiler(pipeline_stage) {}
    bool CompileGlsl(const char *source)
    {
        ++test_compilations;
        return std::strcmp(source, "valid") == 0;
    }
    void OutputDksh(const char *path)
    {
        ++test_outputs;
        test_output_path = path;
        FILE *file = std::fopen(path, "wb");
        if (!file) return;
        uint32_t header[6] = {0, 24, 24, 8, 24, 1};
        std::memcpy(header, test_output_mode == 2 ? "BAD!" : "DKSH", 4);
        if (test_output_mode != 1) {
            std::fwrite(header, 1, sizeof(header), file);
            if (test_output_mode != 3) std::fwrite("12345678", 1, 8, file);
        }
        std::fclose(file);
    }
};
#endif
