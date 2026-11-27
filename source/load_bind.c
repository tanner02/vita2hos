#include <stdlib.h>
#include <string.h>
#include <psp2/kernel/error.h>
#include "load.h"
#include "load_layout.h"
#include "arm-encode.h"
#include "log.h"
#include "sce-elf.h"

#define IMP_GET_NEXT(imp) ((sce_module_imports_u_raw *)((char *)imp + imp->size))
#define IMP_GET_FUNC_COUNT(imp)                                                                    \
    (imp->size == sizeof(sce_module_imports_short_raw) ? imp->imports_short.num_syms_funcs         \
                                                       : imp->imports.num_syms_funcs)
#define IMP_GET_VARS_COUNT(imp)                                                                    \
    (imp->size == sizeof(sce_module_imports_short_raw) ? imp->imports_short.num_syms_vars          \
                                                       : imp->imports.num_syms_vars)
#define IMP_GET_TLS_COUNT(imp) \
    (imp->size == sizeof(sce_module_imports_short_raw) ? imp->imports_short.num_syms_tls_vars \
                                                       : imp->imports.num_syms_tls_vars)
#define IMP_GET_NID(imp)                                                                           \
    (imp->size == sizeof(sce_module_imports_short_raw) ? imp->imports_short.library_nid            \
                                                       : imp->imports.library_nid)
#define IMP_GET_NAME(imp)                                                                          \
    (imp->size == sizeof(sce_module_imports_short_raw) ? imp->imports_short.library_name           \
                                                       : imp->imports.library_name)
#define IMP_GET_FUNC_TABLE(imp)                                                                    \
    (imp->size == sizeof(sce_module_imports_short_raw) ? imp->imports_short.func_nid_table         \
                                                       : imp->imports.func_nid_table)
#define IMP_GET_FUNC_ENTRIES(imp)                                                                  \
    (imp->size == sizeof(sce_module_imports_short_raw) ? imp->imports_short.func_entry_table       \
                                                       : imp->imports.func_entry_table)
#define IMP_GET_VARS_TABLE(imp)                                                                    \
    (imp->size == sizeof(sce_module_imports_short_raw) ? imp->imports_short.var_nid_table          \
                                                       : imp->imports.var_nid_table)
#define IMP_GET_VARS_ENTRIES(imp)                                                                  \
    (imp->size == sizeof(sce_module_imports_short_raw) ? imp->imports_short.var_entry_table        \
                                                       : imp->imports.var_entry_table)



static int resolve_imports(const segment_info_t *segments, int count, sce_module_imports_u_raw *import);

static void *map_loaded_address(const segment_info_t *segments, int count, uintptr_t address,
                                 size_t size)
{
    for (int i = 0; i < count; i++) {
        if (segments[i].p_type != PT_LOAD)
            continue;
        LoadAddressRange range = {segments[i].rx_addr, segments[i].rw_addr, segments[i].p_memsz};
        void *mapped = load_address_translate(&range, 1, address, size);
        if (mapped)
            return mapped;
    }
    return NULL;
}


int load_image_bind(LoadImage *image, ModuleGuestExports *exports)
{
    if (!image || !exports || !image->prepared || image->bound || !image->segments ||
        image->segment_count <= 0 ||
        exports != (image->export_registry ? image->export_registry : &image->exports) ||
        (image->imports_size && !image->imports) ||
        image->imports_size > UINTPTR_MAX - (uintptr_t)image->imports) return -1;
    const segment_info_t *segments = image->segments;
    int num_segments = image->segment_count;
    module_set_guest_exports(exports);
    sce_module_imports_u_raw *import =
        (void *)((uintptr_t)image->imports);
    void *end = (void *)((uintptr_t)image->imports + image->imports_size);

    for (; (void *)import < end; import = IMP_GET_NEXT(import)) {
        size_t remaining = (uintptr_t)end - (uintptr_t)import;
        if (remaining < sizeof(import->size) ||
            (import->size != sizeof(sce_module_imports_short_raw) &&
             import->size != sizeof(sce_module_imports_raw)) || import->size > remaining) {
            LOG("Invalid import record size.");
            goto fail;
        }
        LOG("Resolving imports for library NID 0x%08" PRIx32, IMP_GET_NID(import));
        if (resolve_imports(segments, num_segments, import) < 0) {
            LOG("Failed to resolve imports for library NID 0x%08" PRIx32, IMP_GET_NID(import));
            goto fail;
        }
    }


    Result result = 0;
    if (image->fixed) {
        for (int i = 0; i < image->fixed_count; ++i)
            if (image->fixed[i].writable_mapped) load_fixed_flush(&image->fixed[i]);
    } else result = jitTransitionToExecutable(&image->jit);
    if (R_FAILED(result)) {
        LOG("Could not transition JIT to executable: 0x%x", result);
        goto fail;
    }
    image->bound = true;
    return 0;
fail:
    module_set_guest_exports(NULL);
    return -1;
}

static int resolve_imports(const segment_info_t *segments, int count, sce_module_imports_u_raw *import)
{
    const void *addr;
    uint32_t nid;
    uint32_t *stub;
    uint32_t lib_nid = IMP_GET_NID(import);
    if (IMP_GET_TLS_COUNT(import)) {
        LOG("Unsupported TLS imports for library NID 0x%08" PRIx32, lib_nid);
        return -1;
    }

    const uint32_t num_funcs = IMP_GET_FUNC_COUNT(import);
    if (num_funcs && (((uintptr_t)IMP_GET_FUNC_TABLE(import) & 3) ||
                     ((uintptr_t)IMP_GET_FUNC_ENTRIES(import) & 3)))
        return -1;
    const uint32_t *nids = map_loaded_address(segments, count,
        (uintptr_t)IMP_GET_FUNC_TABLE(import), num_funcs * sizeof(uint32_t));
    const uint32_t *entries = map_loaded_address(segments, count,
        (uintptr_t)IMP_GET_FUNC_ENTRIES(import), num_funcs * sizeof(uint32_t));
    if (num_funcs && (!nids || !entries))
        return -1;

    for (uint32_t i = 0; i < IMP_GET_FUNC_COUNT(import); i++) {
        memcpy(&nid, &nids[i], sizeof(nid));
        uint32_t stub_address;
        memcpy(&stub_address, &entries[i], sizeof(stub_address));
        if (stub_address % 4 != 0)
            return -1;
        stub = map_loaded_address(segments, count, stub_address, 3 * sizeof(uint32_t));
        if (!stub)
            return -1;
        addr = module_get_func_export(lib_nid, nid);

        if (addr) {
            stub[0] = arm_encode_movw(12, (uint16_t)(uintptr_t)addr);
            stub[1] = arm_encode_movt(12, (uint16_t)(((uintptr_t)addr) >> 16));
            stub[2] = arm_encode_bx(12);
        } else {
            stub[0] = arm_encode_movw(0, (uint16_t)SCE_KERNEL_ERROR_MODULEMGR_NO_FUNC_NID);
            stub[1] = arm_encode_movt(0, (uint16_t)(SCE_KERNEL_ERROR_MODULEMGR_NO_FUNC_NID >> 16));
            stub[2] = arm_encode_ret();
            LOG("  Could not resolve NID 0x%08" PRIx32 ", export not found!", nid);
            LOG("UNRESOLVED_IMPORT kind=function library=0x%08" PRIx32 " nid=0x%08" PRIx32,
                lib_nid, nid);
        }
    }

    const uint32_t num_vars = IMP_GET_VARS_COUNT(import);
    if (num_vars && (((uintptr_t)IMP_GET_VARS_TABLE(import) |
                     (uintptr_t)IMP_GET_VARS_ENTRIES(import)) & 3))
        return -1;
    const uint32_t *var_nids = map_loaded_address(segments, count,
        (uintptr_t)IMP_GET_VARS_TABLE(import), num_vars * sizeof(uint32_t));
    const uint32_t *var_entries = map_loaded_address(segments, count,
        (uintptr_t)IMP_GET_VARS_ENTRIES(import), num_vars * sizeof(uint32_t));
    if (num_vars && (!var_nids || !var_entries))
        return -1;
    for (uint32_t i = 0; i < num_vars; i++) {
        memcpy(&nid, &var_nids[i], sizeof(nid));
        addr = module_get_var_export(lib_nid, nid);
        if (!addr || (uintptr_t)addr > UINT32_MAX) {
            LOG("UNRESOLVED_IMPORT kind=variable library=0x%08" PRIx32 " nid=0x%08" PRIx32,
                lib_nid, nid);
            return -1;
        }
        uint32_t table_address, header;
        memcpy(&table_address, &var_entries[i], 4);
        if (table_address & 3) return -1;
        const void *table = map_loaded_address(segments, count, table_address, 4);
        if (!table) return -1;
        memcpy(&header, table, 4);
        uint32_t bytes = (header >> 4) & 0xffffff;
        if ((header & 0xf000000f) || bytes < 4) return -1;
        table = map_loaded_address(segments, count, table_address, bytes);
        if (!table || load_variable_relocate((const char *)table + 4, bytes - 4,
                segments, count, (uint32_t)(uintptr_t)addr) < 0) return -1;
    }

    return 0;
}
