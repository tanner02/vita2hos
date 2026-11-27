#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "gxm/program.h"
#include "gxm/texture_usage.h"

static uint16_t read16(const unsigned char *p)
{ return (uint16_t)p[0] | (uint16_t)p[1] << 8; }
static uint32_t read32(const unsigned char *p)
{ return (uint32_t)read16(p) | (uint32_t)read16(p+2) << 16; }

/* GXP table offsets are relative to the offset field itself. */
static const unsigned char *table(const unsigned char *p, size_t size,
                                  size_t field, uint32_t count, size_t stride)
{
    if (!count) return p;
    uint32_t relative = read32(p+field);
    if (!relative || field > size || relative > size-field) return NULL;
    size_t start = field+relative;
    if (count > (size-start)/stride) return NULL;
    return p+start;
}

bool gxm_program_texture_mask(const void *bytes, size_t available, uint16_t *mask)
{
    if (!mask) return false;
    *mask = UINT16_MAX;
    if (!bytes || available < sizeof(SceGxmProgram) ||
        (uintptr_t)bytes > UINTPTR_MAX-available) return false;
    const unsigned char *p = bytes;
    if (memcmp(p,"GXP\0",4) || p[4]!=1 || p[5]!=4) return false;
    uint32_t size = read32(p+offsetof(SceGxmProgram,size));
    if (size < sizeof(SceGxmProgram) || size > available) return false;
#define FIELD(name) offsetof(SceGxmProgram,name)
#define WORD(name) read32(p+FIELD(name))
    /* Texture-buffer accesses have additional sampler conventions. Keep their
     * legacy all-unit uploads until those conventions are supported here. */
    if (WORD(texture_buffer_count) || WORD(texture_buffer_dependent_sampler_count)) return false;
    uint32_t count = WORD(parameter_count);
    const unsigned char *parameters = table(p,size,FIELD(parameters_offset),count,16);
    if (!parameters) return false;
    uint16_t result = 0;
    for (uint32_t i=0; i<count; ++i) {
        const unsigned char *parameter = parameters+16*i;
        if ((parameter[4]&15) != 2) continue; /* SCE_GXM_PARAMETER_CATEGORY_SAMPLER */
        uint32_t index = read32(parameter+12), elements = read32(parameter+8);
        if (index >= 16 || !elements || elements > 16-index) return false;
        for (uint32_t j=0; j<elements; ++j) result |= (uint16_t)(1U << (index+j));
    }
    /* Only fragment shaders interpret the varying union as descriptor metadata.
     * For vertex shaders these bytes are untyped primary-register bit masks. */
    if (WORD(program_flags)&1) {
        const unsigned char *varyings = table(p,size,FIELD(varyings_offset),1,32);
        if (!varyings) return false;
        uint32_t varying_count = read16(varyings+12);
        size_t field = (size_t)(varyings-p)+16;
        const unsigned char *descriptors = table(p,size,field,varying_count,16);
        if (!descriptors) return false;
        for (uint32_t i=0; i<varying_count; ++i) {
            const unsigned char *descriptor = descriptors+16*i;
            if ((read32(descriptor)&0x40f)==0xf) continue;
            uint32_t index = read32(descriptor+4);
            if (index >= 16) return false;
            result |= (uint16_t)(1U << index);
        }
    }
    count = WORD(container_count);
    const unsigned char *containers = table(p,size,FIELD(container_offset),count,8);
    if (!containers) return false;
    bool sampler_container = false;
    for (uint32_t i=0; i<count; ++i)
        if (read16(containers+8*i)==19) sampler_container = true;
    count = WORD(dependent_sampler_count);
    if (count) {
        if (!sampler_container) return false;
        const unsigned char *dependent = table(p,size,FIELD(dependent_sampler_offset),count,4);
        if (!dependent) return false;
        for (uint32_t i=0; i<count; ++i) {
            uint32_t index = read16(dependent+4*i)/4;
            if (index >= 16) return false;
            result |= (uint16_t)(1U << index);
        }
    }
    *mask = result;
    return true;
#undef FIELD
#undef WORD
}
