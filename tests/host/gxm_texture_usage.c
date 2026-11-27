#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../source/gxm_texture_usage.c"

enum { BLOB_SIZE = 512, PARAMETERS = 160, VARYINGS = 256,
       DESCRIPTORS = 288, CONTAINERS = 400, DEPENDENT = 432 };
static unsigned char blob[BLOB_SIZE];
#define FIELD(name) offsetof(SceGxmProgram, name)

static void put16(size_t at, uint16_t value)
{
    assert(at + 2 <= sizeof(blob));
    blob[at] = (unsigned char)value; blob[at+1] = (unsigned char)(value >> 8);
}
static void put32(size_t at, uint32_t value)
{
    assert(at + 4 <= sizeof(blob));
    put16(at, (uint16_t)value); put16(at+2, (uint16_t)(value >> 16));
}
static void relative(size_t field, size_t target)
{
    assert(target > field && target - field <= UINT32_MAX);
    put32(field, (uint32_t)(target-field));
}
static void reset(int fragment)
{
    assert(sizeof(SceGxmProgram) <= PARAMETERS);
    memset(blob, 0, sizeof(blob)); memcpy(blob, "GXP\0", 4);
    blob[4] = 1; blob[5] = 4;
    put32(FIELD(size), sizeof(blob)); put32(FIELD(program_flags), fragment ? 1 : 0);
    relative(FIELD(parameters_offset), PARAMETERS);
    relative(FIELD(varyings_offset), VARYINGS);
    relative(VARYINGS+16, DESCRIPTORS);
    relative(FIELD(container_offset), CONTAINERS);
    relative(FIELD(dependent_sampler_offset), DEPENDENT);
}
static void parameter(unsigned row, uint32_t index, uint32_t elements, unsigned category)
{
    size_t at = PARAMETERS+16*row;
    blob[at+4] = (unsigned char)(0xa0 | category);
    put32(at+8, elements); put32(at+12, index);
}
static void descriptor(unsigned row, uint32_t info, uint32_t index)
{
    size_t at = DESCRIPTORS+16*row;
    put32(at, info); put32(at+4, index);
}
static void dependent(unsigned row, uint16_t index_and_layout)
{ put16(DEPENDENT+4*row, index_and_layout); }
static void sampler_container(void)
{
    put32(FIELD(container_count), 2);
    put16(CONTAINERS, 14); put16(CONTAINERS+8, 19);
}
static void expect(uint16_t expected)
{
    uint16_t mask = 0x5a5a;
    assert(gxm_program_texture_mask(blob, sizeof(blob), &mask));
    assert(mask == expected);
}
static void fallback(void)
{
    uint16_t mask = 0x1234;
    assert(!gxm_program_texture_mask(blob, sizeof(blob), &mask));
    assert(mask == UINT16_MAX);
}
static void mixed(void)
{
    reset(1); put32(FIELD(parameter_count), 2);
    parameter(0, 0, 2, 2); parameter(1, 15, 1, 2);
    put16(VARYINGS+12, 2);
    descriptor(0, 0, 7); descriptor(1, 0x400, 8);
    sampler_container(); put32(FIELD(dependent_sampler_count), 2);
    dependent(0, 4*12); dependent(1, 4*13+3);
}
static void test_valid_metadata(void)
{
    reset(0); expect(0);
    reset(1); expect(0);
    reset(0); put32(FIELD(parameter_count), 3);
    parameter(0, 0, 1, 2); parameter(1, 15, 1, 2); parameter(2, 0, 1, 2);
    expect(0x8001);
    reset(0); put32(FIELD(parameter_count), 2);
    parameter(0, 2, 3, 2); parameter(1, 4, 2, 2); expect(0x003c);
    reset(0); put32(FIELD(parameter_count), 1);
    parameter(0, 0, 16, 2); expect(UINT16_MAX);
    /* Non-sampler parameter indices/array lengths do not describe textures. */
    reset(0); put32(FIELD(parameter_count), 1);
    parameter(0, UINT32_MAX, UINT32_MAX, 0); expect(0);
    /* No parameter symbols are required for nondependent texture queries. */
    reset(1); put16(VARYINGS+12, 3);
    descriptor(0, 0, 0); descriptor(1, 0x400, 15);
    descriptor(2, 0xf, UINT32_MAX); expect(0x8001);
    /* Bit 10 participates in the predicate: 0x40f is not the 0xf sentinel. */
    reset(1); put16(VARYINGS+12, 1); descriptor(0, 0x40f, 6); expect(1U<<6);
    for (unsigned remainder = 0; remainder < 4; ++remainder) {
        reset(0); sampler_container(); put32(FIELD(dependent_sampler_count), 3);
        dependent(0, remainder); dependent(1, 4*15+remainder);
        dependent(2, 4*15+remainder); expect(0x8001);
    }
    /* The fragment count bytes overlap vertex untyped_pa_regs. */
    reset(0); put16(VARYINGS+12, UINT16_MAX); put32(VARYINGS+16, UINT32_MAX);
    expect(0);
    reset(0); put32(FIELD(varyings_offset), UINT32_MAX); expect(0);
    mixed(); expect(0xb183);
    /* Count-zero tables need no offsets or pointed-to data. */
    reset(0); put32(FIELD(parameters_offset), 0); put32(FIELD(container_offset), 0);
    put32(FIELD(dependent_sampler_offset), UINT32_MAX); expect(0);
    /* No name offsets/strings are read for sampler-mask extraction. */
    reset(0); put32(FIELD(parameter_count), 1); parameter(0, 3, 1, 2);
    put32(PARAMETERS, UINT32_MAX); expect(1U<<3);
    /* Header-only vertex input is a valid empty program. */
    reset(0); put32(FIELD(size), sizeof(SceGxmProgram));
    uint16_t mask = UINT16_MAX;
    assert(gxm_program_texture_mask(blob, sizeof(SceGxmProgram), &mask) && mask == 0);
}
static void test_fallback(void)
{
    reset(0); blob[0] = 'B'; fallback();
    reset(0); blob[3] = 'X'; fallback();
    const unsigned versions[][2] = {{0,4},{2,4},{1,0},{1,3},{1,5},{1,255}};
    for (unsigned i = 0; i < sizeof(versions)/sizeof(versions[0]); ++i) {
        reset(0); blob[4] = versions[i][0]; blob[5] = versions[i][1]; fallback();
    }
    reset(0); put32(FIELD(size), 0); fallback();
    reset(0); put32(FIELD(size), sizeof(SceGxmProgram)-1); fallback();
    reset(0); put32(FIELD(size), BLOB_SIZE+1); fallback();
    reset(0); put32(FIELD(size), UINT32_MAX); fallback();
    for (unsigned kind = 0; kind < 2; ++kind) {
        reset(0); put32(kind ? FIELD(texture_buffer_dependent_sampler_count) :
                              FIELD(texture_buffer_count), 1); fallback();
    }
    const uint32_t bad_indices[] = {16, 32, UINT32_MAX};
    for (unsigned i = 0; i < sizeof(bad_indices)/sizeof(bad_indices[0]); ++i) {
        reset(0); put32(FIELD(parameter_count), 1);
        parameter(0, bad_indices[i], 1, 2); fallback();
        reset(1); put16(VARYINGS+12, 1); descriptor(0, 0, bad_indices[i]); fallback();
    }
    const uint32_t bad_arrays[] = {0, 2, UINT32_MAX};
    for (unsigned i = 0; i < sizeof(bad_arrays)/sizeof(bad_arrays[0]); ++i) {
        reset(0); put32(FIELD(parameter_count), 1);
        parameter(0, 15, bad_arrays[i], 2); fallback();
    }
    reset(0); sampler_container(); put32(FIELD(dependent_sampler_count), 1);
    dependent(0, 64); fallback();
    reset(0); sampler_container(); put32(FIELD(dependent_sampler_count), 1);
    dependent(0, UINT16_MAX); fallback();
    reset(0); put32(FIELD(dependent_sampler_count), 1); fallback();
    reset(0); put32(FIELD(dependent_sampler_count), 1);
    put32(FIELD(container_count), 1); put16(CONTAINERS, 18); fallback();
    /* All ordinary table offsets are relative to their own field, not blob base. */
    const size_t fields[] = {FIELD(parameters_offset), FIELD(container_offset),
                             FIELD(dependent_sampler_offset), FIELD(varyings_offset), VARYINGS+16};
    const size_t counts[] = {FIELD(parameter_count), FIELD(container_count),
                             FIELD(dependent_sampler_count), 0, VARYINGS+12};
    for (unsigned i = 0; i < sizeof(fields)/sizeof(fields[0]); ++i) {
        const uint32_t offsets[] = {0, UINT32_MAX, BLOB_SIZE, BLOB_SIZE-(uint32_t)fields[i]};
        for (unsigned j = 0; j < sizeof(offsets)/sizeof(offsets[0]); ++j) {
            reset(1); sampler_container();
            if (i == 4) put16(counts[i], 1);
            else if (i != 3) put32(counts[i], 1);
            put32(fields[i], offsets[j]); fallback();
        }
    }
    const size_t count_fields[] = {FIELD(parameter_count), FIELD(container_count),
                                   FIELD(dependent_sampler_count)};
    for (unsigned i = 0; i < sizeof(count_fields)/sizeof(count_fields[0]); ++i) {
        reset(0); sampler_container(); put32(count_fields[i], UINT32_MAX); fallback();
    }
    reset(1); put16(VARYINGS+12, UINT16_MAX); fallback();
    /* A valid first record never publishes a partial mask on a later failure. */
    reset(0); put32(FIELD(parameter_count), 2);
    parameter(0, 0, 1, 2); parameter(1, 16, 1, 2); fallback();
    mixed(); put32(DESCRIPTORS+16+4, 16); fallback();
    mixed(); dependent(1, 64); fallback();
}
static void test_spans(void)
{
    uint16_t mask = 0;
    assert(!gxm_program_texture_mask(NULL, BLOB_SIZE, &mask) && mask == UINT16_MAX);
    assert(!gxm_program_texture_mask(blob, sizeof(blob), NULL));
    mask = 0;
    assert(!gxm_program_texture_mask((void *)(UINTPTR_MAX-7), sizeof(SceGxmProgram), &mask));
    assert(mask == UINT16_MAX);
    mixed();
    _Alignas(4) unsigned char unaligned[BLOB_SIZE+1];
    memcpy(unaligned+1, blob, sizeof(blob));
    assert(gxm_program_texture_mask(unaligned+1, BLOB_SIZE, &mask) && mask == 0xb183);
    /* Exact allocations make an overread beyond the independently supplied span
     * visible to ASan, including the header and every table boundary. */
    for (size_t n = 0; n <= BLOB_SIZE; ++n) {
        unsigned char *short_blob = malloc(n ? n : 1); assert(short_blob);
        memcpy(short_blob, blob, n); mask = 0;
        int ok = gxm_program_texture_mask(short_blob, n, &mask);
        assert(ok == (n == BLOB_SIZE));
        assert(mask == (ok ? 0xb183 : UINT16_MAX));
        free(short_blob);
    }
    for (size_t n = sizeof(SceGxmProgram); n <= BLOB_SIZE; ++n) {
        put32(FIELD(size), (uint32_t)n);
        unsigned char *short_blob = malloc(n); assert(short_blob);
        memcpy(short_blob, blob, n); mask = 0;
        int ok = gxm_program_texture_mask(short_blob, n, &mask);
        assert(ok == (n >= DEPENDENT+8));
        assert(mask == (ok ? 0xb183 : UINT16_MAX));
        free(short_blob);
    }
}
static void test_stage_selection(void)
{
    uint32_t used=0;
    assert(gxm_texture_usage_select(&used,false,1) && used==1);
    assert(!gxm_texture_usage_select(&used,false,1));
    assert(gxm_texture_usage_select(&used,true,0x8000) && used==UINT32_C(0x80000001));
    assert(gxm_texture_usage_active(used,0) && gxm_texture_usage_active(used,31));
    assert(!gxm_texture_usage_active(used,15) && !gxm_texture_usage_active(used,16));
    assert(!gxm_texture_usage_active(used,32) && !gxm_texture_usage_active(used,UINT32_MAX));
    assert(gxm_texture_usage_select(&used,false,0) && used==UINT32_C(0x80000000));
    assert(!gxm_texture_usage_active(used,0));
    assert(gxm_texture_usage_select(&used,false,0x8000) && gxm_texture_usage_active(used,15));
    assert(gxm_texture_usage_select(&used,true,1) && used==UINT32_C(0x00018000));
    assert(!gxm_texture_usage_active(used,31) && gxm_texture_usage_active(used,16));
    assert(gxm_texture_usage_select(&used,true,UINT16_MAX));
    assert(gxm_texture_usage_select(&used,false,UINT16_MAX) && used==UINT32_MAX);
    for (unsigned i=0;i<32;++i) assert(gxm_texture_usage_active(used,i));
    assert(!gxm_texture_usage_select(&used,true,UINT16_MAX));
}
int main(void)
{
    test_valid_metadata(); test_fallback(); test_spans(); test_stage_selection();
    puts("Texture usage: bounded named/anonymous masks, conservative fallback, relative tables, unaligned input and truncated spans passed.");
    return 0;
}
