#ifndef MODULE_H
#define MODULE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum { MODULE_SYMBOL_FUNCTION, MODULE_SYMBOL_VARIABLE } ModuleSymbolKind;
typedef struct {
    uint32_t nid;
    ModuleSymbolKind kind;
    const void *address;
} ModuleGuestSymbol;
typedef struct ModuleGuestLibrary ModuleGuestLibrary;
typedef struct { ModuleGuestLibrary *libraries; } ModuleGuestExports;

/* Startup binding only: populate before guest threads run, clear after they stop.
 * The registry owns copied symbol tables; their addresses remain image-owned. */
int module_guest_exports_register(ModuleGuestExports *exports, const void *owner,
                                  uint32_t library, const ModuleGuestSymbol *symbols, size_t count);
const void *module_guest_exports_find(const ModuleGuestExports *exports, uint32_t library,
                                      ModuleSymbolKind kind, uint32_t nid, bool *library_owned);
void module_guest_exports_remove(ModuleGuestExports *exports, const void *owner);
void module_guest_exports_clear(ModuleGuestExports *exports);
/* Transfer all libraries atomically; duplicate providers leave both unchanged. */
int module_guest_exports_merge(ModuleGuestExports *exports, ModuleGuestExports *pending);
void module_set_guest_exports(ModuleGuestExports *exports);

typedef struct {
    const char *name;
    uint32_t nid;
    const uint32_t *func_nidtable_start;
    const uint32_t *func_nidtable_stop;
    const void **func_table_start;
    const void **func_table_stop;
} library_t;

extern const library_t __start_exported_libraries[];
extern const library_t __stop_exported_libraries[];

typedef struct {
    uint32_t library, nid;
    const void *address;
} ModuleNativeVariable;
extern const ModuleNativeVariable __start_exported_variables[] __attribute__((weak));
extern const ModuleNativeVariable __stop_exported_variables[] __attribute__((weak));
/* The variable's address is exported, not the value stored in it. Keep this
 * pointer-bearing descriptor writable for runtime relocation. Numeric library
 * NIDs avoid requiring a function library for a variable-only provider. */
#define EXPORT_VAR(library_nid, variable_nid, variable) \
    static ModuleNativeVariable variable##_export_##library_nid##_##variable_nid \
        __attribute__((used, section("exported_variables"), aligned(__alignof__(ModuleNativeVariable)))) = \
            {library_nid, variable_nid, &variable};

#define DECLARE_LIBRARY(_name, _nid)                                                               \
    extern const uint32_t __start_##_name##_func_nidtable[];                                       \
    extern const uint32_t __stop_##_name##_func_nidtable[];                                        \
    extern const void *__start_##_name##_func_table[];                                             \
    extern const void *__stop_##_name##_func_table[];                                              \
    /* Pointer-bearing tables must be writable during loader relocation. */                       \
    static library_t _name##_library                                                               \
        __attribute__((used, section("exported_libraries"))) = {                                   \
            .name                = #_name,                                                         \
            .nid                 = _nid,                                                           \
            .func_nidtable_start = __start_##_name##_func_nidtable,                                \
            .func_nidtable_stop  = __stop_##_name##_func_nidtable,                                 \
            .func_table_start    = __start_##_name##_func_table,                                   \
            .func_table_stop     = __stop_##_name##_func_table                                     \
        }

#define EXPORT_FUNC(library, nid, func)                                                            \
    static const uint32_t library##_##name##_##nid##_nid                                           \
        __attribute__((used, section(#library "_func_nidtable"))) = nid;                           \
    static const void *library##_##name##_##nid##_func                                             \
        __attribute__((used, section(#library "_func_table"))) = &func;

#define EXPORT(library, nid, type, name, ...)                                                      \
    type name(__VA_ARGS__);                                                                        \
    EXPORT_FUNC(library, nid, name)                                                                \
    type name(__VA_ARGS__)

#define NUM_EXPORTED_LIBS                                                                          \
    (((uintptr_t)&__stop_exported_libraries - (uintptr_t)&__start_exported_libraries) /            \
     sizeof(library_t))
#define NUM_FUNCS(lib)                                                                             \
    (((uintptr_t)lib->func_table_stop - (uintptr_t)lib->func_table_start) / sizeof(void *))

const void *module_get_func_export(uint32_t lib_nid, uint32_t func_nid);
const void *module_get_var_export(uint32_t lib_nid, uint32_t var_nid);

#endif
