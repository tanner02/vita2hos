#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

#include "../../source/util.c"

void log_printf(const char *format, ...) { (void)format; }

static void check_load_failure(const char *path)
{
    void *data = (void *)1;
    uint32_t size = 123;
    assert(util_load_file(path, &data, &size) == -1);
    assert(data == NULL);
    assert(size == 0);
}

int main(void)
{
    char path[] = "/tmp/vita2hos-util-XXXXXX";
    const char binary[] = {'a', '\0', 'b', '\xff'};
    int fd = mkstemp(path);
    int pipes[2];
    char pipe_path[64];
    void *data;
    uint32_t size;

    assert(fd >= 0);
    close(fd);
    assert(util_load_file(path, &data, &size) == 0);
    assert(data != NULL && size == 0);
    free(data);

    assert(util_write_binary_file(path, binary, sizeof(binary)) == 0);
    assert(util_load_file(path, &data, &size) == 0);
    assert(size == sizeof(binary));
    assert(memcmp(data, binary, size) == 0);
    free(data);
    assert(util_write_text_file(path, "hello\n") == 0);
    assert(util_load_file(path, &data, &size) == 0);
    assert(size == 6 && memcmp(data, "hello\n", size) == 0);
    free(data);

    /* A sparse file exercises the size limit without allocating 4 GiB. */
    fd = open(path, O_WRONLY);
    assert(fd >= 0);
    assert(ftruncate(fd, (off_t)UINT32_MAX + 1) == 0);
    close(fd);
    check_load_failure(path);
    unlink(path);
    check_load_failure(path);

    /* Pipes cannot seek; this must fail before allocation or reading. */
    assert(pipe(pipes) == 0);
    snprintf(pipe_path, sizeof(pipe_path), "/proc/self/fd/%d", pipes[0]);
    check_load_failure(pipe_path);
    close(pipes[0]);
    close(pipes[1]);

    assert(util_write_binary_file("/dev/full", binary, sizeof(binary)) == -1);
    assert(util_write_text_file("/dev/full", "hello") == -1);
    assert(util_write_binary_file("/dev/null/missing", binary, sizeof(binary)) == -1);
    assert(util_write_text_file("/dev/null/missing", "hello") == -1);
    puts("Utility I/O regression checks passed.");
    return 0;
}
