/* Compile with -no-pie: the diagnostic's guest slot addresses are uint32_t.
 * Synthetic Thumb pointers are validated but never executed on the host. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define OBSERVER_HOST_TEST
#include "../hardware/startup_observer/observer.c"

enum { PLAN_FD = 10, SOURCE_FD, REPORT_FD };
static unsigned char input[HEADER + LIMIT * ROW + 1];
static const unsigned char source_bytes[] = "hello";
static volatile uint32_t slots[LIMIT];
static unsigned input_size, input_pos, source_pos, read_chunk, write_chunk;
static char report[16384];
static unsigned report_size, opens, closes, writes, callbacks;
static int open_failure, close_failure, read_failure, write_failure;
static int oversized_read, oversized_write, zero_write;

int sceIoOpen(const char *path, int flags, int mode)
{
    int fd;
    ++opens;
    if (!strcmp(path, PLAN)) { fd = PLAN_FD; input_pos = 0; assert(flags == SCE_O_RDONLY); }
    else if (!strcmp(path, SOURCE)) { fd = SOURCE_FD; source_pos = 0; assert(flags == SCE_O_RDONLY); }
    else {
        assert(!strcmp(path, REPORT)); fd = REPORT_FD;
        if (flags & SCE_O_TRUNC) {
            assert(flags == (SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC));
            assert(mode == 0666); report_size = 0; report[0] = 0;
        } else assert(flags == (SCE_O_WRONLY | SCE_O_APPEND));
    }
    return open_failure == fd ? -1 : fd;
}
int sceIoRead(int fd, void *buffer, unsigned size)
{
    assert(fd == PLAN_FD || fd == SOURCE_FD);
    if (read_failure == fd) return -1;
    if (oversized_read == fd) return (int)size + 1;
    unsigned *position = fd == PLAN_FD ? &input_pos : &source_pos;
    unsigned length = fd == PLAN_FD ? input_size : sizeof(source_bytes) - 1;
    const unsigned char *bytes = fd == PLAN_FD ? input : source_bytes;
    unsigned n = length - *position;
    if (n > size) n = size;
    if (read_chunk && n > read_chunk) n = read_chunk;
    memcpy(buffer, bytes + *position, n); *position += n;
    return (int)n;
}
int sceIoWrite(int fd, const void *buffer, unsigned size)
{
    assert(fd == REPORT_FD); ++writes;
    if (write_failure) return -1;
    if (zero_write) return 0;
    if (oversized_write) return (int)size + 1;
    unsigned n = write_chunk && size > write_chunk ? write_chunk : size;
    assert(report_size + n < sizeof(report));
    memcpy(report + report_size, buffer, n); report_size += n;
    report[report_size] = 0; return (int)n;
}
int sceIoClose(int fd)
{
    assert(fd >= PLAN_FD && fd <= REPORT_FD); ++closes;
    return close_failure == fd ? -1 : 0;
}
static void put32(unsigned offset, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) input[offset+i] = (unsigned char)(value >> (8*i));
}
static void reset(unsigned count)
{
    assert((uintptr_t)slots <= UINT32_MAX - sizeof(slots));
    assert((uintptr_t)wrappers[0] <= UINT32_MAX);
    memset(stages, 0, sizeof(stages)); stage_count = 0;
    memset(input, 0, sizeof(input)); memset((void *)slots, 0, sizeof(slots));
    report_size = input_pos = source_pos = opens = closes = writes = callbacks = 0;
    read_chunk = write_chunk = 0; report[0] = 0;
    open_failure = close_failure = read_failure = write_failure = 0;
    oversized_read = oversized_write = zero_write = 0;
    memcpy(input, "STI1", 4);
    /* Published FNV-1a 64-bit vector for the five bytes "hello". */
    put32(4, 0x80aabd0b); put32(8, 0xa430d846);
    put32(12, count); put32(16, 0x1000); put32(20, 0x1000);
    put32(24, (uint32_t)(uintptr_t)slots); put32(28, sizeof(slots));
    input_size = HEADER + count * ROW;
    for (unsigned i = 0; i < count; ++i) {
        slots[i] = 0x1001 + 4*i;
        put32(HEADER+i*ROW, (uint32_t)(uintptr_t)&slots[i]);
        put32(HEADER+i*ROW+4, slots[i]);
        snprintf((char *)input+HEADER+i*ROW+8, 48, "stage_%u", i);
    }
}
static void unchanged(unsigned count)
{
    for (unsigned i = 0; i < count; ++i) assert(slots[i] == 0x1001 + 4*i);
}
static void rejected(int code)
{
    char expected[96];
    assert(module_start(0, NULL) == -3);
    unchanged(2); assert(stage_count == 0);
    snprintf(expected, sizeof(expected), "GUARD startup_observer_v3 0x%08x\n", (unsigned)code);
    assert(!strcmp(report, expected));
}
static void test_plan_guards(void)
{
    reset(2); input[0] = 'X'; rejected(-3);
    reset(2); input_size = HEADER-1; rejected(-2);
    reset(2); --input_size; rejected(-4);
    reset(2); ++input_size; rejected(-4);
    reset(2); put32(12, 0); input_size = HEADER; rejected(-4);
    reset(2); put32(12, LIMIT+1); rejected(-4);
    reset(2); put32(12, UINT32_MAX); rejected(-4);
    /* A bad digest must be rejected before even looking at invalid addresses. */
    reset(2); input[4] ^= 1; put32(HEADER, 0); rejected(-5);
    reset(2); input[8] ^= 1; put32(HEADER, 0); rejected(-5);
    reset(2); put32(16, 0xfffffff0); put32(20, 32); rejected(-6);
    reset(2); put32(24, 0xfffffff0); put32(28, 32); rejected(-6);
    reset(2); put32(20, 0); rejected(-6);
    reset(2); put32(28, 0); rejected(-6);
    reset(2); put32(28, 3); rejected(-6);
    reset(2); put32(HEADER+ROW, (uint32_t)(uintptr_t)&slots[1] + 1); rejected(-6);
    reset(2); put32(HEADER+ROW, (uint32_t)(uintptr_t)slots + sizeof(slots)); rejected(-6);
    reset(2); put32(HEADER+ROW+4, 0x1004); rejected(-6);
    reset(2); put32(HEADER+ROW+4, 0x2001); rejected(-6);
    reset(2); put32(HEADER+ROW+4, 0x0fff); rejected(-6);
    reset(2); input[HEADER+ROW+8] = 0; rejected(-7);
    reset(2); input[HEADER+ROW+8] = 31; rejected(-7);
    reset(2); input[HEADER+ROW+8] = 127; rejected(-7);
    reset(2); input[HEADER+ROW+8] = 255; rejected(-7);
    reset(2); memset(input+HEADER+ROW+8, 'a', 48); rejected(-7);
    reset(2); put32(HEADER+ROW, (uint32_t)(uintptr_t)&slots[0]); rejected(-8);
    reset(2); put32(HEADER+ROW+4, 0x1011); rejected(-9);
    /* Validate all rows before dereferencing any slot: address 4 is unmapped. */
    reset(2); put32(24, 0); put32(28, UINT32_MAX);
    put32(HEADER, 4); input[HEADER+ROW+8] = 0; rejected(-7);
    reset(2); put32(24, 0); put32(28, UINT32_MAX);
    put32(HEADER, 4); put32(HEADER+ROW, 4); rejected(-8);
    reset(LIMIT); ++input_size;
    assert(module_start(0, NULL) == -3); unchanged(LIMIT); assert(stage_count == 0);
    assert(!strcmp(report, "GUARD startup_observer_v3 0xfffffffe\n"));
}
static void test_io_guards(void)
{
    reset(2); open_failure = PLAN_FD; rejected(-1);
    reset(2); read_failure = PLAN_FD; rejected(-2);
    reset(2); oversized_read = PLAN_FD; rejected(-2);
    reset(2); close_failure = PLAN_FD; rejected(-2);
    reset(2); open_failure = SOURCE_FD; rejected(-5);
    reset(2); read_failure = SOURCE_FD; rejected(-5);
    reset(2); oversized_read = SOURCE_FD; rejected(-5);
    reset(2); close_failure = SOURCE_FD; rejected(-5);
    reset(2); open_failure = REPORT_FD;
    assert(module_start(0, NULL) == -2); unchanged(2); assert(!stage_count);
    reset(2); close_failure = REPORT_FD;
    assert(module_start(0, NULL) == -2); unchanged(2); assert(!stage_count);
    reset(2); write_failure = 1;
    assert(module_start(0, NULL) == -3); unchanged(2); assert(report_size == 0);
    reset(2); zero_write = 1;
    assert(module_start(0, NULL) == -3); unchanged(2);
    reset(2); oversized_write = 1;
    assert(module_start(0, NULL) == -3); unchanged(2);
    reset(2); assert(module_start(1, NULL) == -1); assert(opens == 0); unchanged(2);
    assert(module_start(0, (void *)1) == -1); assert(opens == 0); unchanged(2);
}
/* a0/a1 model the raw low/high halves of a 64-bit clock argument. */
#define A0 UINT32_C(0x89abcdef)
#define A1 UINT32_C(0x01234567)
#define A2 UINT32_C(0xdeadbeef)
#define A3 UINT32_C(0x8055aa11)
static void check_args(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    assert(a0 == A0 && a1 == A1 && a2 == A2 && a3 == A3);
    assert(((uint64_t)a1 << 32 | a0) == UINT64_C(0x0123456789abcdef));
}
static uint32_t run(unsigned i) { return wrappers[i](A0, A1, A2, A3); }
static uint32_t inner(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    check_args(a0, a1, a2, a3);
    assert(slots[1]==(uint32_t)(uintptr_t)inner);
    ++callbacks; return UINT32_C(0x80000001);
}
static uint32_t outer(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{
    check_args(a0, a1, a2, a3);
    assert(slots[0]==(uint32_t)(uintptr_t)outer);
    ++callbacks; assert(run(1) == UINT32_C(0x80000001));
    check_args(a0, a1, a2, a3);
    return UINT32_MAX;
}
static uint32_t zero_result(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3)
{ check_args(a0, a1, a2, a3); ++callbacks; return 0; }
static void test_success_and_callbacks(void)
{
    reset(LIMIT); read_chunk = 7; write_chunk = 3;
    assert(module_start(0, NULL) == 0); assert(stage_count == LIMIT);
    assert(!strcmp(report, "GUARD startup_observer_v3 0x00000000\n"));
    for (unsigned i = 0; i < LIMIT; ++i) {
        char name[48]; snprintf(name, sizeof(name), "stage_%u", i);
        assert(slots[i] == (uint32_t)(uintptr_t)wrappers[i]);
        assert(stages[i].slot == &slots[i]);
        assert((uintptr_t)stages[i].original == 0x1001 + 4*i);
        assert(!strcmp(stages[i].name, name));
    }
    unsigned old_opens = opens;
    assert(module_start(0, NULL) == -1); assert(opens == old_opens);
    stages[0].original = outer; stages[1].original = inner;
    assert(run(0) == UINT32_MAX && callbacks == 2);
    assert(!strcmp(report,
        "GUARD startup_observer_v3 0x00000000\n"
        "BEGIN stage_0 0x00000000\nBEGIN stage_1 0x00000001\n"
        "END stage_1 0x80000001\nEND stage_0 0xffffffff\n"));
    stages[63].original = zero_result;
    assert(run(63) == 0 && callbacks == 3);
    assert(strstr(report, "BEGIN stage_63 0x0000003f\nEND stage_63 0x00000000\n"));
    assert(slots[63]==(uint32_t)(uintptr_t)zero_result);
    unsigned saved_report_size=report_size;
    assert(((callback)(uintptr_t)slots[63])(A0,A1,A2,A3)==0 && callbacks==4);
    assert(((callback)(uintptr_t)slots[1])(A0,A1,A2,A3)==UINT32_C(0x80000001) && callbacks==5);
    assert(report_size==saved_report_size); /* Later dispatch bypasses observer. */

    /* Failure of either log event must still forward all four registers once
     * and preserve every return bit, even when invoked through a cached wrapper. */
    stages[0].original = inner;
    open_failure = REPORT_FD; assert(run(0) == 0x80000001 && callbacks == 6);
    open_failure = 0; write_failure = 1;
    assert(run(0) == 0x80000001 && callbacks == 7);
    write_failure = 0; close_failure = REPORT_FD;
    assert(run(0) == 0x80000001 && callbacks == 8);
    assert(strstr(report, "END stage_0 0x80000001\n"));
    slots[0]=0x12345678;
    assert(run(0)==UINT32_C(0x80000001) && callbacks==9);
    assert(slots[0]==0x12345678);
    assert(writes > 50); /* Short writes were completed, rather than dropped. */
    reset(1); memset(input+HEADER+8, '~', 47); input[HEADER+8+47] = 0;
    assert(module_start(0, NULL) == 0 && strlen(stages[0].name) == 47);
    stages[0].original = zero_result; assert(run(0) == 0);
}
int main(void)
{
    test_plan_guards(); test_io_guards(); test_success_and_callbacks();
    puts("Startup observer v3: plan guards, atomic restoration, FNV binding, four raw argument words, raw results and logging passed.");
    return 0;
}
