#ifndef GXM_PROGRAM_H
#define GXM_PROGRAM_H
#include <stdint.h>
#include <psp2/gxm.h>
#include "gxm/program_parameter.h"

typedef struct SceGxmProgram {
    uint32_t magic; // should be "GXP\0"

    uint8_t major_version; // min 1
    uint8_t minor_version; // min 4
    uint16_t sdk_version;  // 0x350 - 3.50

    uint32_t
        size; // size of file - ignoring padding bytes at the end after SceGxmProgramParameter table

    uint32_t binary_guid;
    uint32_t source_guid;

    uint32_t program_flags;

    uint32_t buffer_flags; // Buffer flags. 2 bits per buffer. 0x1 - loaded into registers. 0x2 -
                           // read from memory

    uint32_t texunit_flags[2]; // Tex unit flags. 4 bits per tex unit. 0x1 is non dependent read,
                               // 0x2 is dependent.

    uint32_t parameter_count;
    uint32_t
        parameters_offset; // Number of bytes from the start of this field to the first parameter.
    uint32_t varyings_offset; // offset to vertex outputs / fragment inputs, relative to this field

    uint16_t primary_reg_count;   // (PAs)
    uint16_t secondary_reg_count; // (SAs)
    uint32_t temp_reg_count1;
    uint16_t temp_reg_count2; // Temp reg count in selective rate(programmable blending) phase

    uint16_t primary_program_phase_count;
    uint32_t primary_program_instr_count;
    uint32_t primary_program_offset;

    uint32_t secondary_program_instr_count;
    uint32_t secondary_program_offset;     // relative to the beginning of this field
    uint32_t secondary_program_offset_end; // relative to the beginning of this field

    uint32_t scratch_buffer_count;
    uint32_t thread_buffer_count;
    uint32_t literal_buffer_count;

    uint32_t data_buffer_count;
    uint32_t texture_buffer_count;
    uint32_t default_uniform_buffer_count;

    uint32_t literal_buffer_data_offset;

    uint32_t compiler_version; // The version is shifted 4 bits to the left.

    uint32_t literals_count;
    uint32_t literals_offset;
    uint32_t uniform_buffer_count;
    uint32_t uniform_buffer_offset;

    uint32_t dependent_sampler_count;
    uint32_t dependent_sampler_offset;
    uint32_t texture_buffer_dependent_sampler_count;
    uint32_t texture_buffer_dependent_sampler_offset;
    uint32_t container_count;
    uint32_t container_offset;
    uint32_t sampler_query_info_offset; // Offset to array of uint16_t
} SceGxmProgram;
#endif
