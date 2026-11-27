#include <uam/compiler_iface.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <unistd.h>

#include "uam_compiler_iface_c.h"
#include "config.h"

#ifndef UAM_TEMP_FILE_TEMPLATE
#define UAM_TEMP_FILE_TEMPLATE VITA2HOS_ROOT_PATH "/.uam-XXXXXX"
#endif

extern "C" {

bool uam_compiler_compile_glsl(pipeline_stage stage, const char *glsl_source, void *buffer,
                               uint32_t capacity, uint32_t *size)
{
    if (size)
        *size = 0;
    if (!glsl_source || !buffer || !size || !capacity)
        return false;
    DekoCompiler compiler{ stage };
    bool rc = compiler.CompileGlsl(glsl_source);
    if (!rc)
        return false;

    /* The installed UAM memory writer has no capacity parameter. Stage its
     * file output, validate the complete DKSH, then copy only a bounded result. */
    char temporary[] = UAM_TEMP_FILE_TEMPLATE;
    int fd = mkstemp(temporary);
    if (fd < 0) return false;
    struct Cleanup {
        const char *path;
        ~Cleanup() { unlink(path); }
    } cleanup{temporary};
    if (close(fd) != 0) return false;
    compiler.OutputDksh(temporary);
    struct FileCloser { void operator()(FILE *file) const { fclose(file); } };
    std::unique_ptr<FILE, FileCloser> file(fopen(temporary, "rb"));
    if (!file || fseek(file.get(), 0, SEEK_END) != 0) return false;
    long length = ftell(file.get());
    if (length < 24 || (uint64_t)length > capacity || fseek(file.get(), 0, SEEK_SET) != 0)
        return false;
    std::unique_ptr<void, decltype(&free)> data(malloc((size_t)length), free);
    if (!data || fread(data.get(), 1, (size_t)length, file.get()) != (size_t)length)
        return false;
    uint32_t header[6];
    memcpy(header, data.get(), sizeof(header));
    if (memcmp(data.get(), "DKSH", 4) || header[1] < sizeof(header) ||
        header[1] > header[2] || header[2] > (uint32_t)length ||
        header[3] != (uint32_t)length - header[2])
        return false;
    memcpy(buffer, data.get(), (size_t)length);
    *size = (uint32_t)length;
    return true;
}
}
