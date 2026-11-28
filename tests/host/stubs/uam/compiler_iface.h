#ifndef TEST_UAM_COMPILER_IFACE_H
#define TEST_UAM_COMPILER_IFACE_H
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include "glsl_frontend.h"
extern unsigned int test_compilations, test_outputs;
extern int test_output_mode;
extern uint32_t test_code_size;
extern std::string test_output_path;
void test_compiler_enter();
void test_compiler_leave();
class DekoCompiler {
public:
    explicit DekoCompiler(pipeline_stage, int optLevel = 3) {
        assert(optLevel == 3); /* Build-time clone fix permits normal optimization. */
        test_compiler_enter();
    }
    ~DekoCompiler() { test_compiler_leave(); }
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
        uint32_t header[6] = {0, 24, 24, test_code_size, 24, 1};
        std::memcpy(header, test_output_mode == 2 ? "BAD!" : "DKSH", 4);
        if (test_output_mode != 1) {
            std::fwrite(header, 1, sizeof(header), file);
            if (test_output_mode != 3) {
                for (uint32_t offset = 0; offset < test_code_size;) {
                    uint32_t count = test_code_size - offset;
                    if (count > 8) count = 8;
                    std::fwrite("12345678", 1, count, file);
                    offset += count;
                }
            }
        }
        std::fclose(file);
    }
};
#endif
