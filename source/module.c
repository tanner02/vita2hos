#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "module.h"

struct ModuleGuestLibrary {
    ModuleGuestLibrary *next;
    const void *owner;
    uint32_t nid;
    size_t count;
    ModuleGuestSymbol symbols[];
};
static ModuleGuestExports *active_guest_exports;

int module_guest_exports_merge(ModuleGuestExports *exports, ModuleGuestExports *pending)
{
    if (!exports || !pending || exports == pending) return -1;
    for (ModuleGuestLibrary *p = pending->libraries; p; p = p->next)
        for (ModuleGuestLibrary *q = exports->libraries; q; q = q->next)
            if (p->nid == q->nid) return -1;
    if (pending->libraries) {
        ModuleGuestLibrary *tail = pending->libraries;
        while (tail->next) tail = tail->next;
        tail->next = exports->libraries;
        exports->libraries = pending->libraries;
        pending->libraries = NULL;
    }
    return 0;
}

int module_guest_exports_register(ModuleGuestExports *exports, const void *owner,
                                  uint32_t library, const ModuleGuestSymbol *symbols, size_t count)
{
    if (!exports || !owner || !library || (count && !symbols) ||
        count > (SIZE_MAX - sizeof(ModuleGuestLibrary)) / sizeof(ModuleGuestSymbol)) return -1;
    for (ModuleGuestLibrary *p = exports->libraries; p; p = p->next)
        if (p->nid == library) return -1;
    for (size_t i = 0; i < count; ++i) {
        if (!symbols[i].address ||
            (symbols[i].kind != MODULE_SYMBOL_FUNCTION && symbols[i].kind != MODULE_SYMBOL_VARIABLE)) return -1;
        for (size_t j = 0; j < i; ++j)
            if (symbols[i].nid == symbols[j].nid && symbols[i].kind == symbols[j].kind) return -1;
    }
    ModuleGuestLibrary *p = malloc(sizeof(*p) + count * sizeof(*symbols));
    if (!p) return -1;
    p->owner = owner; p->nid = library; p->count = count;
    if (count) memcpy(p->symbols, symbols, count * sizeof(*symbols));
    p->next = exports->libraries;
    exports->libraries = p;
    return 0;
}

const void *module_guest_exports_find(const ModuleGuestExports *exports, uint32_t library,
                                      ModuleSymbolKind kind, uint32_t nid, bool *library_owned)
{
    if (library_owned) *library_owned = false;
    if (!exports) return NULL;
    for (const ModuleGuestLibrary *p = exports->libraries; p; p = p->next) {
        if (p->nid != library) continue;
        if (library_owned) *library_owned = true;
        for (size_t i = 0; i < p->count; ++i)
            if (p->symbols[i].nid == nid && p->symbols[i].kind == kind) return p->symbols[i].address;
        return NULL;
    }
    return NULL;
}

void module_guest_exports_remove(ModuleGuestExports *exports, const void *owner)
{
    if (!exports || !owner) return;
    ModuleGuestLibrary **p = &exports->libraries;
    while (*p) {
        if ((*p)->owner == owner) {
            ModuleGuestLibrary *old = *p;
            *p = old->next;
            free(old);
        } else p = &(*p)->next;
    }
}

void module_guest_exports_clear(ModuleGuestExports *exports)
{
    if (!exports) return;
    if (active_guest_exports == exports) active_guest_exports = NULL;
    while (exports->libraries) {
        ModuleGuestLibrary *p = exports->libraries;
        exports->libraries = p->next;
        free(p);
    }
}

void module_set_guest_exports(ModuleGuestExports *exports)
{
    active_guest_exports = exports;
}

const void *module_get_func_export(uint32_t lib_nid, uint32_t func_nid)
{
    bool owned;
    const void *guest = module_guest_exports_find(active_guest_exports, lib_nid,
                                                 MODULE_SYMBOL_FUNCTION, func_nid, &owned);
    /* A guest library owns its whole namespace. Falling back per function can
     * mix guest and host allocators, leaving pointers with the wrong owner. */
    if (owned) return guest;
    size_t num_libs = NUM_EXPORTED_LIBS;

    for (size_t i = 0; i < num_libs; i++) {
        if (__start_exported_libraries[i].nid == lib_nid) {
            const library_t *const lib = &__start_exported_libraries[i];
            size_t num_funcs           = NUM_FUNCS(lib);

            for (size_t j = 0; j < num_funcs; j++) {
                if (lib->func_nidtable_start[j] == func_nid)
                    return lib->func_table_start[j];
            }
        }
    }

    return NULL;
}

const void *module_get_var_export(uint32_t lib_nid, uint32_t var_nid)
{
    bool owned;
    const void *guest = module_guest_exports_find(active_guest_exports, lib_nid,
                                                 MODULE_SYMBOL_VARIABLE, var_nid, &owned);
    if (owned) return guest;
    size_t count = ((uintptr_t)__stop_exported_variables -
                    (uintptr_t)__start_exported_variables) / sizeof(ModuleNativeVariable);
    for (size_t i = 0; i < count; ++i) {
        const ModuleNativeVariable *variable = &__start_exported_variables[i];
        if (variable->library == lib_nid && variable->nid == var_nid) return variable->address;
    }
    return NULL;
}
