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
    if (!lifecycle || !read || (size && !records)) return -1;
    LoadLifecycle decoded = {0};
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
            }
        }
        p += 32;
        size -= 32;
    }
    *lifecycle = decoded;
    return 0;
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
        if (tls) goto fail;
        if (library) {
            uint64_t total = (uint64_t)functions + variables;
            if (total > SIZE_MAX / sizeof(ModuleGuestSymbol) || total > UINT32_MAX / 4)
                goto fail;
            size_t count = (size_t)total;
            uint32_t nid_address = read32(p + 24), entry_address = read32(p + 28);
            if (count && ((nid_address | entry_address) & 3)) goto fail;
            if ((uint64_t)nid_address + count * 4 > UINT64_C(0x100000000) ||
                (uint64_t)entry_address + count * 4 > UINT64_C(0x100000000)) goto fail;
            const unsigned char *nids = count ? read(context, nid_address, count * 4, false) : NULL;
            const unsigned char *entries = count ? read(context, entry_address, count * 4, false) : NULL;
            if (count && (!nids || !entries)) goto fail;
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
