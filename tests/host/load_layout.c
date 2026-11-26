#include <assert.h>
#include <stdio.h>

#include "../../source/load_layout.c"

int main(void)
{
    unsigned char code[64], data[32];
    LoadAddressRange ranges[] = {
        {0x1000, (uintptr_t)code, sizeof(code)},
        {0x8000, (uintptr_t)data, sizeof(data)},
    };
    assert(load_address_translate(ranges, 2, 0x1004, 12) == code + 4);
    assert(load_address_translate(ranges, 2, 0x8004, 12) == data + 4);
    assert(load_address_translate(ranges, 2, 0x103c, 4) == code + 60);
    assert(load_address_translate(ranges, 2, 0x103c, 5) == NULL);
    assert(load_address_translate(ranges, 2, 0x1040, 1) == NULL);
    assert(load_address_translate(ranges, 2, 0x2000, 4) == NULL);
    assert(load_address_translate(ranges, 2, 0, 0) == NULL);
    assert(load_address_translate(ranges, 2, UINTPTR_MAX, 8) == NULL);
    LoadAddressRange invalid = {0x1000, UINTPTR_MAX - 1, 64};
    assert(load_address_translate(&invalid, 1, 0x1004, 4) == NULL);
    assert(load_address_translate(&invalid, 1, 0x1000, 4) == NULL);
    /* Data has no distinct RX alias and must map to itself. */
    LoadAddressRange direct = {(uintptr_t)data, (uintptr_t)data, sizeof(data)};
    assert(load_address_translate(&direct, 1, (uintptr_t)data + 8, 4) == data + 8);
    uint32_t offset, end;
    assert(load_layout_place(0x1003, 0, 17, 16, 128, &offset, &end) == 0);
    assert(offset == 13 && end == 30);
    assert(load_layout_place(0x1003, end, 9, 32, 128, &offset, &end) == 0);
    assert(offset == 61 && end == 70);
    assert(load_layout_place(0x1003, end, 5, 0, 128, &offset, &end) == 0);
    assert(offset == 70 && end == 75);
    assert(load_layout_place(0x1003, end, 0, 1, 128, &offset, &end) == 0);
    assert(offset == 75 && end == 75);
    const uint32_t saved_offset = offset, saved_end = end;
    assert(load_layout_place(0x1000, 0, 1, 3, 128, &offset, &end) == -1);
    assert(load_layout_place(0x1003, 0, 16, 16, 16, &offset, &end) == -1);
    assert(load_layout_place(UINTPTR_MAX - 4, 8, 1, 1, 128, &offset, &end) == -1);
    assert(load_layout_place(UINTPTR_MAX - 4, 0, 8, 1, 128, &offset, &end) == -1);
    assert(load_layout_place(0, UINT32_MAX - 1, 4, 1, UINT32_MAX, &offset, &end) == -1);
    assert(offset == saved_offset && end == saved_end);

    /* Vary both arena alignment and previous segment sizes. */
    for (uintptr_t base = 0x1000; base < 0x1040; base++) {
        uint32_t used = 0;
        for (uint32_t alignment = 1; alignment <= 4096; alignment *= 2) {
            uint32_t previous = used;
            assert(load_layout_place(base, used, 37, alignment, 65536, &offset, &used) == 0);
            assert(offset >= previous && used == offset + 37);
            assert((base + offset) % alignment == 0);
        }
    }
    puts("Executable segment-layout regression checks passed.");
    return 0;
}
