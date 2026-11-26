#include <errno.h>
#include <stdlib.h>

#include "log.h"
#include "util.h"

int util_load_file(const char *filename, void **data, uint32_t *size)
{
    FILE *fp;
    void *buffer;
    long length;

    *data = NULL;
    *size = 0;

    fp = fopen(filename, "rb");
    if (!fp) {
        LOG("util_load_file: could not open file \"%s\". Error: %s", filename, strerror(errno));
        return -1;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        LOG("util_load_file: could not seek to end of file");
        fclose(fp);
        return -1;
    }

    length = ftell(fp);
    if (length < 0 || (uintmax_t)length > UINT32_MAX || fseek(fp, 0, SEEK_SET) != 0) {
        LOG("util_load_file: invalid file size or could not rewind file");
        fclose(fp);
        return -1;
    }

    /* Keep successful empty-file loads independent of malloc(0) behavior. */
    buffer = malloc(length ? (size_t)length : 1);
    if (!buffer) {
        LOG("util_load_file: could not allocate memory");
        fclose(fp);
        return -1;
    }

    if (fread(buffer, 1, (size_t)length, fp) != (size_t)length || ferror(fp)) {
        LOG("util_load_file: could not read the file");
        free(buffer);
        fclose(fp);
        return -1;
    }

    if (fclose(fp) != 0) {
        free(buffer);
        return -1;
    }

    *data = buffer;
    *size = (uint32_t)length;

    return 0;
}

int util_write_binary_file(const char *filename, const void *data, uint32_t size)
{
    FILE *fp;
    int ret;

    fp = fopen(filename, "wb");
    if (!fp)
        return -1;

    ret = fwrite(data, 1, size, fp) == size ? 0 : -1;
    if (fclose(fp) != 0)
        ret = -1;

    return ret;
}

int util_write_text_file(const char *filename, const char *data)
{
    FILE *fp;
    int ret;

    fp = fopen(filename, "w");
    if (!fp)
        return -1;

    ret = fputs(data, fp) == EOF ? -1 : 0;
    if (fclose(fp) != 0)
        ret = -1;

    return ret;
}
