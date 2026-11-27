#ifndef VITA2HOS_TROPHY_METADATA_H
#define VITA2HOS_TROPHY_METADATA_H
#include <stddef.h>
#include <stdint.h>
#define TROPHY_METADATA_MAX_TROPHIES 128
#define TROPHY_METADATA_MAX_GROUPS 16
typedef struct {uint8_t grade,group,hidden,present;} TrophyDefinition;
typedef struct {
    char communication_id[13];
    uint32_t groups,trophy_count,group_count,grade_counts[5];
    TrophyDefinition trophies[TROPHY_METADATA_MAX_TROPHIES];
} TrophyMetadata;
enum {TROPHY_METADATA_OK=0,TROPHY_METADATA_INVALID=-1,TROPHY_METADATA_NO_MEMORY=-2};
#ifdef __cplusplus
extern "C" {
#endif
/* Structural trophy configuration, no unlock/progress state. Failure preserves output. */
int trophy_metadata_parse(const void *data,size_t size,TrophyMetadata *output);
#ifdef __cplusplus
}
#endif
#endif
