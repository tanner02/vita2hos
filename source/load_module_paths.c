#include "load_module_paths.h"
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

static int append(LoadModulePaths *paths, char *line, const char *root)
{
    while (*line == ' ' || *line == '\t') ++line;
    size_t size = strlen(line);
    while (size && (line[size-1] == ' ' || line[size-1] == '\t' || line[size-1] == '\r'))
        line[--size] = 0;
    if (!size || *line == '#') return 0;
    if (paths->count == LOAD_PROCESS_MAX_MODULES || *line == '/' ||
        size < 7 || strcmp(line + size - 6, ".suprx")) return -1;
    const char *part = line;
    for (const char *p = line;; ++p) {
        if (*p == '\\' || *p == ':' || (*p && (unsigned char)*p < 32)) return -1;
        if (*p == '/' || !*p) {
            size_t length = p - part;
            if (!length || (length == 1 && part[0] == '.') ||
                (length == 2 && part[0] == '.' && part[1] == '.')) return -1;
            if (!*p) break;
            part = p + 1;
        }
    }
    char *target = paths->paths[paths->count];
    int length = snprintf(target, LOAD_MODULE_PATH_SIZE, "%s%s%s", root,
                           root[strlen(root)-1] == '/' ? "" : "/", line);
    if (length < 0 || length >= LOAD_MODULE_PATH_SIZE) return -1;
    for (size_t i=0; i<paths->count; ++i)
        if (!strcmp(target, paths->paths[i])) return -1;
    ++paths->count;
    return 0;
}

int load_module_paths_read(LoadModulePaths *paths, FILE *file, const char *app_root)
{
    if (!paths) return -1;
    paths->count = 0;
    paths->present = false;
    if (!file || !app_root || !*app_root) return -1;
    char line[LOAD_MODULE_PATH_SIZE]; size_t used=0;
    int c;
    while ((c=fgetc(file)) != EOF) {
        if (c == '\n') {
            line[used]=0;
            if (append(paths, line, app_root) < 0) goto fail;
            used=0;
        } else {
            if (!c || used == sizeof(line)-1) goto fail;
            line[used++]=(char)c;
        }
    }
    if (ferror(file)) goto fail;
    if (used) {
        line[used]=0;
        if (append(paths, line, app_root) < 0) goto fail;
    }
    paths->present = true;
    return 0;
fail:
    paths->count=0;
    paths->present=false;
    return -1;
}

int load_module_paths_file(LoadModulePaths *paths, const char *filename, const char *app_root)
{
    if (!paths) return -1;
    paths->count=0;
    paths->present=false;
    if (!filename || !app_root || !*app_root) return -1;
    FILE *file=fopen(filename, "rb");
    if (!file) return errno == ENOENT ? 0 : -1;
    int result=load_module_paths_read(paths, file, app_root);
    if (fclose(file) != 0) { paths->count=0; paths->present=false; return -1; }
    return result;
}

int load_module_paths_preloads(LoadModulePaths *paths, const char *app_root,
                              const LoadProcessParameters *parameters)
{
    if (!paths) return -1;
    paths->count=0;
    paths->present=false;
    if (!app_root || !*app_root || !parameters) return -1;
    if (parameters->version != 5 && parameters->version != 6) return 0;
    static const struct { const char *name; uint32_t disabled; } preloads[] = {
        {"sce_module/libc.suprx", 0x00010000},
        {"sce_module/libfios2.suprx", 0x00200000},
    };
    for (size_t i=0; i<sizeof(preloads)/sizeof(*preloads); ++i) {
        if (parameters->has_preload_disabled &&
            (parameters->preload_disabled & preloads[i].disabled)) continue;
        char path[LOAD_MODULE_PATH_SIZE];
        int length=snprintf(path, sizeof(path), "%s%s%s", app_root,
                            app_root[strlen(app_root)-1]=='/' ? "" : "/", preloads[i].name);
        if (length < 0 || (size_t)length >= sizeof(path)) goto fail;
        struct stat info;
        if (stat(path, &info) < 0) {
            if (errno == ENOENT) continue;
            goto fail;
        }
        if (!S_ISREG(info.st_mode)) goto fail;
        strcpy(paths->paths[paths->count++], path);
    }
    return 0;
fail:
    paths->count=0;
    return -1;
}
