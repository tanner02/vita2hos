#include <assert.h>
#include <stdio.h>
#include <stdarg.h>

#include "../../source/load_relocate.c"
#include "../../source/load_var_relocate.c"

void log_printf(const char *format, ...) { (void)format; }

static SCE_Rel record(uint32_t code, uint32_t symbol, uint32_t target,
                       uint32_t offset, uint32_t addend)
{
    SCE_Rel result = {0};
    result.r_long.r_type = (code << 8) | (symbol << 4) | (target << 16);
    result.r_long.r_offset = offset;
    result.r_long.r_addend = addend;
    return result;
}

int main(void)
{
    uint32_t memory[8] = {0};
    segment_info_t segments[2] = {
        {.p_type = PT_LOAD, .rw_addr = (uintptr_t)memory, .rx_addr = 0x1000,
         .p_memsz = sizeof(memory)},
        {.p_type = PT_LOAD, .rx_addr = 0x2000, .p_memsz = 4},
    };
    SCE_Rel r = record(R_ARM_ABS32, 1, 0, 0, 0x123);
    uint32_t variable1[] = {1 | (R_ARM_ABS32 << 8) | (0x12 << 16), 0};
    assert(load_variable_relocate(variable1, sizeof(variable1), segments, 2, 0x3000) == 0);
    assert(memory[0] == 0x3012);
    uint32_t variable2[] = {2 | (R_ARM_REL32 << 8), 4, 0xfffffff0};
    assert(load_variable_relocate(variable2, sizeof(variable2), segments, 2, 0x3000) == 0);
    assert(memory[1] == 0x1fec);
    assert(load_variable_relocate(variable1, 7, segments, 2, 0x3000) < 0);
    assert(load_variable_relocate(variable2, 8, segments, 2, 0x3000) < 0);
    variable2[0] |= 1 << 16;
    assert(load_variable_relocate(variable2, 12, segments, 2, 0x3000) < 0);
    variable1[0] = 3;
    assert(load_variable_relocate(variable1, 8, segments, 2, 0x3000) < 0);
    variable1[0] = 1 | (R_ARM_ABS32 << 8) | (15 << 4);
    assert(load_variable_relocate(variable1, 8, segments, 2, 0x3000) < 0);
    variable1[0] = 1 | (R_ARM_ABS32 << 8); variable1[1] = sizeof(memory);
    assert(load_variable_relocate(variable1, 8, segments, 2, 0x3000) < 0);
    uint32_t overlapping[] = {1 | (R_ARM_ABS32 << 8), 12, 1 | (R_ARM_ABS32 << 8), 16};
    memcpy(memory, overlapping, sizeof(overlapping));
    assert(load_variable_relocate(memory, sizeof(overlapping), segments, 2, 0x3000) == 0);
    assert(memory[3] == 0x3000 && memory[4] == 0x3000);
    assert(load_relocate(&r, 12, segments, 2) == 0 && memory[0] == 0x2123);
    r = record(R_ARM_REL32, 1, 0, 4, 0x20);
    assert(load_relocate(&r, 12, segments, 2) == 0 && memory[1] == 0x101c);
    r = record(R_ARM_ABS32, 15, 0, 8, 0x12345678);
    assert(load_relocate(&r, 12, segments, 2) == 0 && memory[2] == 0x12345678);
    /* Short-form record, including an unaligned input byte stream. */
    SCE_Rel short_r = {0};
    short_r.r_short.r_opt1 = 1 | (1 << 4) | (R_ARM_ABS32 << 8) | (12 << 20);
    short_r.r_short.r_opt2 = 7 << 10;
    unsigned char unaligned[9];
    memcpy(unaligned + 1, &short_r, 8);
    assert(load_relocate(unaligned + 1, 8, segments, 2) == 0 && memory[3] == 0x2007);

    r = record(R_ARM_JUMP24, 1, 0, 16, 0);
    memory[4] = 0xea000000;
    assert(load_relocate(&r, 12, segments, 2) == 0 && memory[4] == 0xea0003fc);

    for (uint32_t length = 1; length < 12; length++) {
        unsigned char *truncated = malloc(length);
        memcpy(truncated, &r, length);
        assert(load_relocate(truncated, length, segments, 2) == -1);
        free(truncated);
    }
    r = record(R_ARM_ABS32, 3, 0, 0, 0);
    assert(load_relocate(&r, 12, segments, 2) == -1);
    r = record(R_ARM_ABS32, 1, 3, 0, 0);
    assert(load_relocate(&r, 12, segments, 2) == -1);
    r = record(R_ARM_ABS32, 1, 0, sizeof(memory) - 2, 0);
    assert(load_relocate(&r, 12, segments, 2) == -1);
    r = record(R_ARM_ABS32, 1, 0, 1, 0);
    assert(load_relocate(&r, 12, segments, 2) == -1);
    r = record(255, 1, 0, 0, 0);
    assert(load_relocate(&r, 12, segments, 2) == -1);
    r = record(R_ARM_ABS32, 1, 0, 0, 0);
    r.r_long.r_type |= R_ARM_ABS32 << 20;
    assert(load_relocate(&r, 12, segments, 2) == -1);

    /* A failed branch must leave its instruction untouched, not copy the value
     * produced by the preceding successful relocation. */
    SCE_Rel pair[] = {record(R_ARM_ABS32, 15, 0, 0, 0xfeed),
                      record(R_ARM_JUMP24, 1, 0, 4, 0)};
    segments[1].rx_addr = 0x10000000;
    memory[1] = 0xea123456;
    assert(load_relocate(pair, sizeof(pair), segments, 2) == -1);
    assert(memory[0] == 0xfeed && memory[1] == 0xea123456);
    assert(load_relocate(NULL, 0, segments, 2) == 0);
    puts("ARM relocation regression checks passed.");
    return 0;
}
