#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "log_file.h"

static bool rotate(const LogFile *log)
{
    FILE *existing = fopen(log->path, "rb");
    if (!existing) return errno == ENOENT;
    if (fclose(existing)) return false;
    if (remove(log->previous) && errno != ENOENT) return false;
    /* If rename fails, keep the current file intact and stop the file sink. */
    return rename(log->path, log->previous) == 0;
}

bool log_file_init(LogFile *log, const char *path, const char *previous, size_t limit)
{
    if (!log) return false;
    *log = (LogFile){0};
    if (!path || !previous || !limit || !strcmp(path, previous)) return false;
    log->path = path; log->previous = previous; log->limit = limit;
    if (!rotate(log)) return false;
    FILE *file = fopen(path, "ab");
    if (!file) return false;
    if (fclose(file)) return false;
    log->enabled = true;
    return true;
}

bool log_file_write(LogFile *log, const char *message)
{
    if (!log || !log->enabled || !message) return false;
    size_t length = strlen(message);
    if (length >= log->limit) return false;
    if (log->bytes > log->limit - length - 1) {
        if (!rotate(log)) { log->enabled = false; return false; }
        log->bytes = 0;
    }
    FILE *file = fopen(log->path, "ab");
    if (!file) { log->enabled = false; return false; }
    bool ok = fputs(message, file) >= 0 && fputc('\n', file) != EOF;
    if (fclose(file)) ok = false;
    if (!ok) { log->enabled = false; return false; }
    log->bytes += length + 1;
    return true;
}
