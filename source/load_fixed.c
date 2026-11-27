#include <stdlib.h>
#include <string.h>

#include "load_fixed.h"
#include "log.h"

Result load_fixed_close(LoadFixedMemory *memory)
{
    if (!memory)
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    Result result;
    if (memory->executable_mapped) {
        result = svcControlCodeMemory(memory->handle, CodeMapOperation_UnmapSlave, memory->address,
                                      memory->size, 0);
        if (R_FAILED(result))
            return result;
        memory->executable_mapped = false;
    }
    if (memory->writable_mapped) {
        result = svcControlCodeMemory(memory->handle, CodeMapOperation_UnmapOwner,
                                      (uintptr_t)memory->writable, memory->size, 0);
        if (R_FAILED(result))
            return result;
        memory->writable_mapped = false;
    }
    if (memory->handle != INVALID_HANDLE) {
        result = svcCloseHandle(memory->handle);
        if (R_FAILED(result))
            return result;
        memory->handle = INVALID_HANDLE;
    }
    if (memory->reservation) {
        virtmemLock();
        virtmemRemoveReservation(memory->reservation);
        virtmemUnlock();
    }
    free(memory->backing);
    *memory = (LoadFixedMemory){ 0 };
    return 0;
}

Result load_fixed_reserve(LoadFixedMemory *memory, uintptr_t address, size_t size, bool executable)
{
    if (!memory || memory->backing || memory->handle != INVALID_HANDLE || memory->reservation ||
        !address || address % 4096 || !size || size > UINT32_MAX - 4095 ||
        address > UINT32_MAX - ((size + 4095) & ~(size_t)4095))
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    if (!envIsSyscallHinted(0x4B) || !envIsSyscallHinted(0x4C))
        return MAKERESULT(Module_Libnx, LibnxError_JitUnavailable);
    /* An unmapped page can still lie in the heap or IPC reservation, where
     * CodeMemory mapping is forbidden. Check those complete regions as well. */
    const u32 ids[] = {InfoType_AliasRegionAddress, InfoType_AliasRegionSize,
                       InfoType_HeapRegionAddress, InfoType_HeapRegionSize};
    u64 regions[4];
    for (unsigned i = 0; i < 4; ++i) {
        Result query = svcGetInfo(&regions[i], ids[i], CUR_PROCESS_HANDLE, 0);
        if (R_FAILED(query)) return query;
    }
    uint64_t end = (uint64_t)address + ((size + 4095) & ~(size_t)4095);
    for (unsigned i = 0; i < 4; i += 2) {
        if (regions[i + 1] > UINT64_MAX - regions[i])
            return MAKERESULT(Module_Libnx, LibnxError_BadInput);
        if (regions[i + 1] && address < regions[i] + regions[i + 1] && regions[i] < end) {
            LOG("Fixed range 0x%08" PRIxPTR " intersects %s region 0x%llx size 0x%llx",
                address, i ? "heap" : "alias", (unsigned long long)regions[i],
                (unsigned long long)regions[i + 1]);
            return MAKERESULT(Module_Libnx, LibnxError_AlreadyMapped);
        }
    }
    memory->address = address;
    memory->size = (size + 4095) & ~(size_t)4095;
    memory->executable = executable;
    MemoryInfo info;
    u32 page_info;
    virtmemLock();
    Result result = svcQueryMemory(&info, &page_info, address);
    if (R_SUCCEEDED(result) &&
        ((info.type & 0xff) != MemType_Unmapped || info.addr > address ||
         address - info.addr > info.size || memory->size > info.size - (address - info.addr)))
        result = MAKERESULT(Module_Libnx, LibnxError_AlreadyMapped);
    if (R_SUCCEEDED(result)) {
        memory->reservation = virtmemAddReservation((void *)address, memory->size);
        if (!memory->reservation)
            result = MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }
    virtmemUnlock();
    if (R_FAILED(result))
        goto fail;
    return 0;
fail: {
    Result cleanup = load_fixed_close(memory);
    if (R_FAILED(cleanup))
        LOG("Fixed reservation rollback failed: 0x%x", cleanup);
    return result;
}
}

Result load_fixed_map(LoadFixedMemory *memory)
{
    if (!memory || !memory->reservation || memory->backing || memory->handle != INVALID_HANDLE)
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    memory->backing = aligned_alloc(4096, memory->size);
    if (!memory->backing) {
        load_fixed_close(memory);
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }
    Result result = svcCreateCodeMemory(&memory->handle, memory->backing, memory->size);
    if (R_FAILED(result))
        goto fail;
    virtmemLock();
    memory->writable =
        memory->executable ? virtmemFindCodeMemory(memory->size, 4096) : (void *)memory->address;
    if (!memory->writable)
        result = MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    else
        result = svcControlCodeMemory(memory->handle, CodeMapOperation_MapOwner,
                                      (uintptr_t)memory->writable, memory->size, Perm_Rw);
    if (R_SUCCEEDED(result))
        memory->writable_mapped = true;
    if (R_SUCCEEDED(result) && memory->executable) {
        result = svcControlCodeMemory(memory->handle, CodeMapOperation_MapSlave, memory->address,
                                      memory->size, Perm_Rx);
        if (R_SUCCEEDED(result))
            memory->executable_mapped = true;
    }
    virtmemUnlock();
    if (R_FAILED(result))
        goto fail;
    memset(memory->writable, 0, memory->size);
    return 0;
fail: {
    Result cleanup = load_fixed_close(memory);
    if (R_FAILED(cleanup))
        LOG("Fixed mapping rollback failed: 0x%x", cleanup);
    return result;
}
}

void load_fixed_flush(const LoadFixedMemory *memory)
{
    armDCacheFlush(memory->writable, memory->size);
    if (memory->executable)
        armICacheInvalidate((void *)memory->address, memory->size);
}
