#ifndef VITA2HOS_TROPHY_ARCHIVE_H
#define VITA2HOS_TROPHY_ARCHIVE_H
#include <stddef.h>
#include <stdint.h>
#define TROPHY_ARCHIVE_MAX_BYTES (64u*1024u*1024u)
#define TROPHY_ARCHIVE_MAX_ENTRIES 4096u
typedef struct {char name[32];uint64_t offset,size;} TrophyArchiveEntry;
typedef struct {
    const uint8_t *data; /* Borrowed immutable archive bytes. */
    size_t size,count;
    TrophyArchiveEntry *entries; /* Owned validated index. */
} TrophyArchive;
enum {TROPHY_ARCHIVE_OK=0,TROPHY_ARCHIVE_INVALID=-1,TROPHY_ARCHIVE_UNSUPPORTED=-2,
    TROPHY_ARCHIVE_NO_MEMORY=-3,TROPHY_ARCHIVE_NOT_FOUND=-4,
    TROPHY_ARCHIVE_RANGE=-5,TROPHY_ARCHIVE_CHECKSUM=-6};
/* Output must be zero-initialized/closed. Failure leaves it unchanged.
 * SHA-1 establishes archive integrity, not publisher/signature authenticity. */
int trophy_archive_open(const void *data,size_t size,TrophyArchive *output);
void trophy_archive_close(TrophyArchive *archive);
const TrophyArchiveEntry *trophy_archive_find(const TrophyArchive *archive,const char *name);
int trophy_archive_read(const TrophyArchive *archive,const char *name,uint64_t offset,void *output,size_t size);
#endif
