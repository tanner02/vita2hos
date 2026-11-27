#include "guest_tp_plan.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t plan_word(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static uint64_t plan_long(const unsigned char *p)
{
    return plan_word(p) | (uint64_t)plan_word(p+4)<<32;
}
int guest_tp_plan_apply(const segment_info_t *segments, int count, uint64_t source_tag,
                         uint64_t source_size, const void *plan, size_t size)
{
    if (!plan || size < 32) return -1;
    const unsigned char *bytes=plan;
    uint32_t sites=plan_word(bytes+12);
    if (memcmp(bytes,"V2HOSTP1",8) || plan_word(bytes+8)!=1 || !sites ||
        sites>GUEST_TP_MAX_SITES || size!=32+(size_t)sites*16 ||
        plan_long(bytes+16)!=source_tag || plan_long(bytes+24)!=source_size) return -1;
    GuestTpSite *decoded=malloc(sites*sizeof(*decoded));
    if (!decoded) return -1;
    for (uint32_t i=0;i<sites;++i) {
        const unsigned char *record=bytes+32+i*16;
        decoded[i]=(GuestTpSite){plan_word(record),plan_word(record+4),
                                 plan_word(record+8),plan_word(record+12)};
    }
    int result=guest_tp_patch(segments,count,decoded,sites);
    free(decoded);
    return result<0 ? -1 : (int)sites;
}
int guest_tp_plan_file(const segment_info_t *segments, int count, uint64_t source_tag,
                        uint64_t source_size, const char *filename)
{
    if (!filename || !*filename) return -1;
    size_t length=strlen(filename);
    if (length>SIZE_MAX-9) return -1;
    char *path=malloc(length+9);
    if (!path) return -1;
    memcpy(path,filename,length);memcpy(path+length,".tlsplan",9);
    FILE *file=fopen(path,"rb");int open_error=errno;free(path);
    if (!file) return open_error==ENOENT ? 0 : -1;
    size_t capacity=32+GUEST_TP_MAX_SITES*16;
    unsigned char *bytes=malloc(capacity+1);
    if (!bytes) { fclose(file);return -1; }
    size_t size=fread(bytes,1,capacity+1,file);
    bool error=ferror(file)!=0;
    if (fclose(file)!=0) error=true;
    int result=error || size>capacity ? -1 :
        guest_tp_plan_apply(segments,count,source_tag,source_size,bytes,size);
    free(bytes);
    return result;
}
