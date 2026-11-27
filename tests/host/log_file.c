#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static unsigned append_failures;
static FILE *injected_fopen(const char *path, const char *mode)
{
    if (append_failures && !strcmp(mode, "ab")) {
        --append_failures;
        errno = EBUSY;
        return NULL;
    }
    return fopen(path, mode);
}
#define fopen injected_fopen
#include "../../source/log_file.c"
#undef fopen

static void contents(const char *path, const char *expected)
{
    FILE *file = fopen(path, "rb"); assert(file);
    char data[128] = {0};
    size_t bytes = fread(data, 1, sizeof(data), file);
    assert(!ferror(file) && !fclose(file));
    assert(bytes == strlen(expected) && !memcmp(data, expected, bytes));
}

int main(void)
{
    char directory[] = "/tmp/vita2hos-log-XXXXXX";
    assert(mkdtemp(directory));
    char path[256], previous[256], missing[256], marker[256];
    snprintf(path, sizeof(path), "%s/log.txt", directory);
    snprintf(previous, sizeof(previous), "%s/previous.txt", directory);
    snprintf(missing, sizeof(missing), "%s/missing/log.txt", directory);
    snprintf(marker, sizeof(marker), "%s/previous.txt/marker", directory);
    LogFile log;
    assert(log_file_init(&log, path, previous, 8));
    assert(log_file_write(&log, "abc"));
    append_failures = 1;
    assert(!log_file_write(&log, "x") && log.enabled && log.bytes == 4);
    assert(!append_failures);
    contents(path, "abc\n");
    assert(log_file_write(&log, "def"));
    contents(path, "abc\ndef\n");
    assert(log.bytes == 8);
    /* Rotation already moved the old file, but a failed append open writes
     * nothing. The next message must create the new bounded log. */
    append_failures = 1;
    assert(!log_file_write(&log, "g") && log.enabled && log.bytes == 0);
    assert(!append_failures);
    contents(previous, "abc\ndef\n");
    assert(access(path, F_OK) == -1 && errno == ENOENT);
    assert(log_file_write(&log, "g"));
    contents(previous, "abc\ndef\n"); contents(path, "g\n");
    assert(!log_file_write(&log, "12345678") && log.enabled);
    contents(path, "g\n");
    assert(log_file_init(&log, path, previous, 8));
    contents(previous, "g\n"); contents(path, "");
    assert(log_file_write(&log, "1234567"));
    /* A failed rotation must not truncate the current diagnostic evidence. */
    assert(!unlink(previous) && !mkdir(previous, 0700));
    FILE *file = fopen(marker, "wb"); assert(file && !fclose(file));
    assert(!log_file_write(&log, "next") && !log.enabled);
    contents(path, "1234567\n");
    assert(!log_file_write(&log, "ignored"));
    assert(!unlink(marker) && !rmdir(previous));
    assert(!log_file_init(&log, missing, previous, 8) && !log.enabled);
    assert(!log_file_init(&log, path, path, 8));
    assert(!log_file_init(&log, path, previous, 0));
    assert(log_file_init(&log, path, previous, 1));
    assert(log_file_write(&log, "")); contents(path, "\n");
    assert(!log_file_write(NULL, "test") && !log_file_write(&log, NULL));
    assert(!unlink(path) && !unlink(previous) && !rmdir(directory));
    puts("Bounded diagnostic log rotation, open retry and failure checks passed.");
}
