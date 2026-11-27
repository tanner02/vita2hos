#define TEST_FIXED_MEMORY
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <switch.h>

static struct {
    void *backing;
    size_t size;
    uintptr_t owner, slave;
    bool live;
} objects[32];
static Handle latest;
static unsigned reservations, locks, dc_flushes, ic_flushes, require_reservations;
static int fail_operation = -1;
static unsigned failures;
static bool hints = true, occupied, fail_query, fail_reservation, fail_create, fail_alias;
static bool fail_allocation, bad_span;
static bool fail_info;
static uint64_t host_regions[4] = {0xc0000000, 0x10000000, 0x40000000, 0x20000000};
static Result jit_close_result;
static void *test_allocate(size_t alignment, size_t size)
{ return fail_allocation ? NULL : aligned_alloc(alignment, size); }

/* Emulate a writable alias without asking the host to map Vita addresses into
 * ASan's shadow-memory region. The backing bytes remain real ASan allocations. */
static void *clear_alias(void *address, int value, size_t size)
{
    for (unsigned i = 1; i < 32; ++i) {
        if (objects[i].live && objects[i].owner == (uintptr_t)address) {
            assert(objects[i].size == size);
            memset(objects[i].backing, value, size);
            return address;
        }
    }
    assert(!"Unmapped writable alias"); return NULL;
}
#define memset clear_alias
#define aligned_alloc test_allocate
#include "../../source/load_fixed.c"
#undef memset
#undef aligned_alloc
#include "../../source/load_image.c"
#include "../../source/load_fixed_layout.c"

void log_printf(const char *format, ...) { (void)format; }
bool envIsSyscallHinted(unsigned svc) { assert(svc == 0x4b || svc == 0x4c); return hints; }
void virtmemLock(void) { assert(locks++ == 0); }
void virtmemUnlock(void) { assert(locks-- == 1); }
Result svcQueryMemory(MemoryInfo *info, u32 *page, uint64_t address)
{
    assert(locks == 1); (void)address; *page = 0;
    *info = (MemoryInfo){0x80000000, 0x10000000, occupied ? 1 : MemType_Unmapped};
    if (bad_span) info->size = 4096;
    return fail_query ? 0x901 : 0;
}
Result svcGetInfo(uint64_t *out, u32 id, Handle handle, uint64_t sub_id)
{
    assert(handle == CUR_PROCESS_HANDLE && sub_id == 0 && id >= 2 && id <= 5);
    *out = host_regions[id - 2];
    return fail_info ? 0x905 : 0;
}
VirtmemReservation *virtmemAddReservation(void *address, size_t size)
{
    assert(locks == 1);
    if (fail_reservation) return NULL;
    VirtmemReservation *r = malloc(sizeof(*r)); assert(r);
    *r = (VirtmemReservation){(uintptr_t)address, size}; ++reservations; return r;
}
void virtmemRemoveReservation(VirtmemReservation *r)
{ assert(locks == 1 && reservations > 0); --reservations; free(r); }
void *virtmemFindCodeMemory(size_t size, size_t guard)
{
    assert(locks == 1 && guard == 4096 && objects[latest].size == size);
    assert(reservations >= require_reservations);
    return fail_alias ? NULL : objects[latest].backing;
}
Result svcCreateCodeMemory(Handle *handle, void *backing, size_t size)
{
    if (fail_create) return 0x902;
    assert(++latest < 32);
    objects[latest].live = true; objects[latest].backing = backing; objects[latest].size = size;
    memset(backing, 0xff, size); *handle = latest; return 0;
}
Result svcControlCodeMemory(Handle handle, u32 operation, uint64_t address, uint64_t size, u32 perm)
{
    assert(handle && objects[handle].live && objects[handle].size == size);
    if (operation <= 1) assert(locks == 1);
    if ((int)operation == fail_operation && failures) { --failures; return 0x903; }
    switch (operation) {
    case CodeMapOperation_MapOwner:
        assert(!objects[handle].owner && perm == Perm_Rw); objects[handle].owner = address; break;
    case CodeMapOperation_MapSlave:
        assert(!objects[handle].slave && perm == Perm_Rx); objects[handle].slave = address; break;
    case CodeMapOperation_UnmapOwner:
        assert(objects[handle].owner == address && perm == 0); objects[handle].owner = 0; break;
    case CodeMapOperation_UnmapSlave:
        assert(objects[handle].slave == address && perm == 0); objects[handle].slave = 0; break;
    default: assert(0);
    }
    return 0;
}
Result svcCloseHandle(Handle handle)
{
    assert(objects[handle].live && !objects[handle].owner && !objects[handle].slave);
    objects[handle].live = false; return 0;
}
void armDCacheFlush(void *address, size_t size) { assert(address && size); ++dc_flushes; }
void armICacheInvalidate(void *address, size_t size) { assert(address && size); ++ic_flushes; }
Result jitClose(Jit *jit) { (void)jit; return jit_close_result; }

static void clean(void)
{
    assert(!reservations && !locks);
    for (unsigned i = 1; i <= latest; ++i) assert(!objects[i].live);
}

int main(void)
{
    segment_info_t segments[2] = {
        {.p_type=PT_LOAD,.p_vaddr=0x81000000,.p_filesz=8880984,.p_memsz=8881056,.p_align=16,.p_flags=PF_R|PF_X},
        {.p_type=PT_LOAD,.p_vaddr=0x81879000,.p_filesz=56980,.p_memsz=3559623,.p_align=16,.p_flags=PF_R|PF_W}
    };
    assert(load_fixed_layout(segments, 2) == 0);
    segment_info_t saved = segments[1];
    segments[1].p_vaddr = 0x81878000; assert(load_fixed_layout(segments, 2) != 0);
    segments[1] = saved; segments[1].p_vaddr++; assert(load_fixed_layout(segments, 2) != 0);
    segments[1] = saved; segments[1].p_filesz = UINT32_MAX; assert(load_fixed_layout(segments, 2) != 0);
    segments[1] = saved; segments[1].p_vaddr = 0xfffff000; assert(load_fixed_layout(segments, 2) != 0);
    segments[1] = saved; segments[0].p_flags |= PF_W; assert(load_fixed_layout(segments, 2) != 0);
    segments[0].p_flags = PF_R; assert(load_fixed_layout(segments, 2) != 0);
    assert(load_fixed_layout(NULL, 1) != 0 && load_fixed_layout(segments, 65) != 0);

    LoadFixedMemory code = {0}, data = {0};
    assert(load_fixed_reserve(&code, 0x81000001, 100, true) != 0); clean();
    hints = false; assert(load_fixed_reserve(&code, 0x81000000, 100, true) != 0); hints = true; clean();
    occupied = true; assert(load_fixed_reserve(&code, 0x81000000, 100, true) != 0); occupied = false; clean();
    fail_query = true; assert(load_fixed_reserve(&code, 0x81000000, 100, true) != 0); fail_query = false; clean();
    fail_info = true; assert(load_fixed_reserve(&code, 0x81000000, 100, true) != 0); fail_info = false; clean();
    host_regions[2] = 0x81000000;
    assert(load_fixed_reserve(&code, 0x81000000, 100, true) == MAKERESULT(Module_Libnx, LibnxError_AlreadyMapped));
    host_regions[2] = 0x40000000; clean();
    host_regions[0] = 0x81000010; host_regions[1] = 0;
    assert(load_fixed_reserve(&code, 0x81000000, 100, true) == 0 && load_fixed_close(&code) == 0);
    host_regions[0] = 0xc0000000; host_regions[1] = 0x10000000; clean();
    bad_span = true; assert(load_fixed_reserve(&code, 0x81000000, 100, true) != 0); bad_span = false; clean();
    fail_reservation = true; assert(load_fixed_reserve(&code, 0x81000000, 100, true) != 0); fail_reservation = false; clean();
    assert(load_fixed_reserve(&code, 0x81000000, 4097, true) == 0);
    assert(load_fixed_reserve(&data, 0x81879000, 100, false) == 0);
    require_reservations = 2;
    assert(load_fixed_map(&code) == 0 && code.size == 8192);
    assert(objects[code.handle].slave == 0x81000000 && code.writable != (void *)code.address);
    assert(load_fixed_map(&data) == 0 && data.size == 4096);
    assert(objects[data.handle].owner == 0x81879000 && !objects[data.handle].slave);
    for (unsigned i = 0; i < code.size; ++i) assert(((char *)code.backing)[i] == 0);
    for (unsigned i = 0; i < data.size; ++i) assert(((char *)data.backing)[i] == 0);
    load_fixed_flush(&code); load_fixed_flush(&data);
    assert(dc_flushes == 2 && ic_flushes == 1);
    assert(load_fixed_map(&code) != 0); /* Do not overwrite live ownership. */
    fail_operation = CodeMapOperation_UnmapSlave; failures = 1;
    assert(load_fixed_close(&code) != 0 && code.backing && code.executable_mapped);
    fail_operation = -1;
    assert(load_fixed_close(&code) == 0 && load_fixed_close(&code) == 0);
    assert(load_fixed_close(&data) == 0); clean(); require_reservations = 0;
    for (int stage = 0; stage < 5; ++stage) {
        assert(load_fixed_reserve(&code, 0x81000000, 100, true) == 0);
        fail_create = stage == 0; fail_alias = stage == 1;
        fail_operation = stage == 2 ? CodeMapOperation_MapOwner : stage == 3 ? CodeMapOperation_MapSlave : -1;
        failures = 1;
        fail_allocation = stage == 4;
        assert(load_fixed_map(&code) != 0);
        fail_create = fail_alias = fail_allocation = false; fail_operation = -1; clean();
    }
    LoadImage fixed_image = {.fixed=calloc(2, sizeof(LoadFixedMemory)),.fixed_count=2};
    assert(load_fixed_reserve(&fixed_image.fixed[0], 0x81000000, 4096, true) == 0);
    assert(load_fixed_reserve(&fixed_image.fixed[1], 0x81879000, 4096, false) == 0);
    require_reservations = 2;
    assert(load_fixed_map(&fixed_image.fixed[0]) == 0 && load_fixed_map(&fixed_image.fixed[1]) == 0);
    fail_operation = CodeMapOperation_UnmapSlave; failures = 1;
    assert(load_image_close(&fixed_image) != 0 && fixed_image.fixed_count == 2);
    assert(fixed_image.fixed[0].backing && !fixed_image.fixed[1].backing);
    fail_operation = -1;
    assert(load_image_close(&fixed_image) == 0 && !fixed_image.fixed && !fixed_image.fixed_count);
    clean(); require_reservations = 0;
    LoadImage image = {.data=malloc(4),.jit_active=true};
    jit_close_result = 0x904;
    assert(load_image_close(&image) != 0 && image.jit_active && image.data);
    jit_close_result = 0;
    assert(load_image_close(&image) == 0 && !image.jit_active && !image.data);
    assert(load_image_close(&image) == 0); clean();
    puts("Fixed-address layout, mapping ownership, rollback and cache checks passed.");
}
