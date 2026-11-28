#ifndef VITA2HOS_UAM_DKSH_H
#define VITA2HOS_UAM_DKSH_H
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static inline uint32_t uam_read_u32(const unsigned char *p)
{ return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
static inline bool uam_dksh_valid(const void *data,uint32_t size)
{
    if(!data||size<24||memcmp(data,"DKSH",4))return false;
    const unsigned char *p=(const unsigned char *)data;
    uint32_t header=uam_read_u32(p+4),code=uam_read_u32(p+8),bytes=uam_read_u32(p+12);
    return header>=24&&header<=code&&code<=size&&bytes==size-code;
}
#endif
