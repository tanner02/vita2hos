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
        result = MAKERESULT(Module_Libnx, LibnxError_BadInput);
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
