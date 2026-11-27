#ifndef LOG_FILE_H
#define LOG_FILE_H
#include <stdbool.h>
#include <stddef.h>
typedef struct {
    const char *path, *previous;
    size_t bytes, limit;
    bool enabled;
} LogFile;
bool log_file_init(LogFile *log, const char *path, const char *previous, size_t limit);
bool log_file_write(LogFile *log, const char *message);
#endif
