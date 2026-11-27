#ifndef LOAD_MODULE_PATHS_H
#define LOAD_MODULE_PATHS_H
#include "load_process.h"
#include <stdio.h>
#define LOAD_MODULE_PATH_SIZE 1024
typedef struct LoadModulePaths {
    char paths[LOAD_PROCESS_MAX_MODULES][LOAD_MODULE_PATH_SIZE];
    size_t count;
    bool present; /* An explicitly empty list still overrides auto-preloads. */
} LoadModulePaths;
/* Optional file: ENOENT means an empty list. All other errors are failures.
 * Each non-comment line is an app-relative .suprx path, in startup order.
 * On failure, count is zero and no partial list may be used. */
int load_module_paths_read(LoadModulePaths *paths, FILE *file, const char *app_root);
int load_module_paths_file(LoadModulePaths *paths, const char *filename, const char *app_root);
int load_module_paths_preloads(LoadModulePaths *paths, const char *app_root,
                              const LoadProcessParameters *parameters);
#endif
