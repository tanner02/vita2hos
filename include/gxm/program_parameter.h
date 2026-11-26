#ifndef GXM_PROGRAM_PARAMETER_H
#define GXM_PROGRAM_PARAMETER_H

#include <psp2/gxm.h>
#include <stdint.h>
#include <assert.h>

/* GXP parameter record; offsets are relative to the start of this record. */
struct SceGxmProgramParameter {
    int32_t name_offset;
    uint16_t category : 4;
    uint16_t type : 4;
    uint16_t component_count : 4;
    uint16_t container_index : 4;
    uint8_t semantic;
    uint8_t semantic_index;
    uint32_t array_size;
    int32_t resource_index;
};

static_assert(sizeof(SceGxmProgramParameter) == 16, "Incorrect GXP parameter size");

#endif
