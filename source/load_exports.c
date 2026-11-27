#include "load_exports.h"
#include <stdlib.h>
#include <string.h>
#include "sce-elf.h"

static uint16_t read16(const unsigned char *p)
{
    return (uint16_t)p[0] | (uint16_t)p[1] << 8;
}
static uint32_t read32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

int load_exports_lifecycle(LoadLifecycle *lifecycle, const void *records, size_t size,
                           LoadExportRead read, void *context)
{
    return load_exports_metadata(lifecycle, NULL, records, size, read, context);
}

int load_exports_metadata(LoadLifecycle *lifecycle, uint32_t *process_param,
                          const void *records, size_t size, LoadExportRead read, void *context)
{
    if (!lifecycle || !read || (size && !records)) return -1;
    LoadLifecycle decoded = {0};
    uint32_t parameter = 0;
    const unsigned char *p = records;
    while (size) {
        if (size < 32 || read16(p) != 32) return -1;
        if (!read32(p + 16)) {
            uint32_t functions = read16(p + 6), variables = read32(p + 8);
            uint64_t count = (uint64_t)functions + variables;
            if (read32(p + 12) || count > UINT32_MAX / 4 || count > SIZE_MAX / 4)
                return -1;
            uint32_t nid_address = read32(p + 24), entry_address = read32(p + 28);
            size_t bytes = (size_t)count * 4;
            if (count && ((nid_address | entry_address) & 3)) return -1;
            if ((uint64_t)nid_address + bytes > UINT64_C(0x100000000) ||
                (uint64_t)entry_address + bytes > UINT64_C(0x100000000)) return -1;
            const unsigned char *nids = count ? read(context, nid_address, bytes, false) : NULL;
            const unsigned char *entries = count ? read(context, entry_address, bytes, false) : NULL;
            if (count && (!nids || !entries)) return -1;
            for (size_t i = 0; i < count; ++i) {
                uint32_t nid = read32(nids + i * 4), address = read32(entries + i * 4);
                const void **slot = nid == NID_MODULE_START ? &decoded.start :
                    nid == NID_MODULE_STOP ? &decoded.stop :
                    nid == NID_MODULE_EXIT ? &decoded.exit : NULL;
                bool function = i < functions;
                uint32_t base = function ? address & ~1U : address;
                size_t length = function ? ((address & 1) ? 2 : 4) : 1;
                if (!address || (function && base % length) ||
                    !read(context, base, length, function)) return -1;
                if (slot) {
                    if (!function || *slot) return -1;
                    *slot = (const void *)(uintptr_t)address;
                }
                if (nid == NID_PROCESS_PARAM) {
                    if (function || parameter) return -1;
                    parameter = address;
                }
            }
        }
        p += 32;
        size -= 32;
    }
    *lifecycle = decoded;
    if (process_param) *process_param = parameter;
    return 0;
}

int load_process_parameters_read(LoadProcessParameters *parameters, uint32_t address,
                                  LoadExportRead read, void *context)
{
    if (!parameters || !read) return -1;
    LoadProcessParameters decoded={0};
    if (!address) { *parameters=decoded;return 0; }
    if (address & 3) return -1;
    const unsigned char *header = read(context, address, 12, false);
    if (!header || read32(header + 4) != UINT32_C(0x32505350)) return -1; /* PSP2 */
    uint32_t size = read32(header), version = read32(header + 8);
    if (size < 12) return -1;
    if (!version) { *parameters=decoded;return 0; }
    if ((version != 5 && version != 6) || size < 0x30) return -1;
    const unsigned char *parameter = read(context, address, size, false);
    if (!parameter) return -1;
    const unsigned offsets[]={0x14,0x18,0x24};
    uint32_t values[3]={0};bool present[3]={0};
    for (unsigned i=0;i<3;++i) {
        uint32_t pointer=read32(parameter+offsets[i]);
        if (!pointer) continue;
        if (pointer & 3) return -1;
        const unsigned char *value=read(context,pointer,4,false);
        if (!value) return -1;
        values[i]=read32(value);present[i]=true;
    }
    memcpy(&decoded.main_priority,&values[0],4);
    decoded.main_stack_size=values[1];decoded.preload_disabled=values[2];
    decoded.has_main_priority=present[0];decoded.has_main_stack_size=present[1];
    decoded.has_preload_disabled=present[2];
    uint32_t libc_address = read32(parameter + 0x2c);
    if (libc_address) {
        if (libc_address & 3) return -1;
        const unsigned char *libc = read(context, libc_address, 4, false);
        if (!libc) return -1;
        uint32_t libc_size = read32(libc);
        if (libc_size < 4 || !read(context, libc_address, libc_size, false)) return -1;
    }
    decoded.libc_address=libc_address;
    *parameters=decoded;
    return 0;
}

int load_process_parameters_register(ModuleGuestExports *exports, const void *owner,
                                      uint32_t address, LoadExportRead read, void *context)
{
    if (!exports || !owner) return -1;
    LoadProcessParameters parameters;
    if (load_process_parameters_read(&parameters,address,read,context)<0) return -1;
    if (!parameters.libc_address) return 0;
    /* Kernel-supplied __sce_libcparam points at the executable's actual libc
     * parameters. Preserve the guest VA rather than publishing its RW alias. */
    ModuleGuestSymbol symbol = {UINT32_C(0xdf084dfa), MODULE_SYMBOL_VARIABLE,
                                 (const void *)(uintptr_t)parameters.libc_address};
    return module_guest_exports_register(exports, owner, UINT32_C(0x5ad9c136), &symbol, 1);
}

int load_exports_register(ModuleGuestExports *exports, const void *owner,
                          const void *records, size_t size,
                          LoadExportRead read, void *context)
{
    ModuleGuestExports pending = {0};
    if (!exports || !owner || !read || (size && !records)) return -1;
    const unsigned char *p = records;
    while (size) {
        if (size < 32 || read16(p) != 32) goto fail;
        uint32_t functions = read16(p + 6), variables = read32(p + 8);
        uint32_t tls = read32(p + 12), library = read32(p + 16);
        /* Module lifecycle exports (library zero) are not public libraries.
         * Their addresses require a separate lifecycle decoder. */
        if (library) {
            uint64_t ordinary = (uint64_t)functions + variables;
            uint64_t total = ordinary + tls;
            if (ordinary > SIZE_MAX / sizeof(ModuleGuestSymbol) ||
                total > UINT32_MAX / 4 || total > SIZE_MAX / 4)
                goto fail;
            size_t count = (size_t)ordinary, table_bytes = (size_t)total * 4;
            uint32_t nid_address = read32(p + 24), entry_address = read32(p + 28);
            if (total && ((nid_address | entry_address) & 3)) goto fail;
            if ((uint64_t)nid_address + table_bytes > UINT64_C(0x100000000) ||
                (uint64_t)entry_address + table_bytes > UINT64_C(0x100000000)) goto fail;
            const unsigned char *nids = total ? read(context, nid_address, table_bytes, false) : NULL;
            const unsigned char *entries = total ? read(context, entry_address, table_bytes, false) : NULL;
            if (total && (!nids || !entries)) goto fail;
            /* TLS entries follow ordinary symbols. Their values are not
             * ordinary addresses and must never enter this registry. */
            ModuleGuestSymbol *symbols = count ? malloc(count * sizeof(*symbols)) : NULL;
            if (count && !symbols) goto fail;
            for (size_t i = 0; i < count; ++i) {
                uint32_t address = read32(entries + i * 4);
                bool function = i < functions;
                uint32_t base = function ? address & ~1U : address;
                size_t bytes = function ? ((address & 1) ? 2 : 4) : 1;
                if (!address || (function && base % bytes) ||
                    !read(context, base, bytes, function)) {
                    free(symbols);
                    goto fail;
                }
                symbols[i] = (ModuleGuestSymbol){read32(nids + i * 4),
                    function ? MODULE_SYMBOL_FUNCTION : MODULE_SYMBOL_VARIABLE,
                    (const void *)(uintptr_t)address};
            }
            int result = module_guest_exports_register(&pending, owner, library, symbols, count);
            free(symbols);
            if (result < 0) goto fail;
        }
        p += 32;
        size -= 32;
    }
    if (module_guest_exports_merge(exports, &pending) == 0) return 0;
fail:
    module_guest_exports_clear(&pending);
    return -1;
}
