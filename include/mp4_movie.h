#ifndef VITA2HOS_MP4_MOVIE_H
#define VITA2HOS_MP4_MOVIE_H
#include "mp4_box.h"

#define MP4_MAX_TRACKS 32
typedef struct {
    uint32_t id, flags, handler, timescale;
    uint32_t width_fixed, height_fixed; /* 16.16, not codec dimensions */
    uint64_t movie_duration, media_duration;
    bool movie_duration_unknown, media_duration_unknown;
    Mp4Box container; /* validated trak scope, for sample/edit parsing */
} Mp4Track;
typedef struct {
    uint32_t timescale, track_count;
    uint64_t duration;
    bool duration_unknown, fragmented;
    Mp4Track tracks[MP4_MAX_TRACKS];
} Mp4Movie;
typedef enum {
    MP4_MOVIE_OK, MP4_MOVIE_INVALID, MP4_MOVIE_IO_ERROR,
    MP4_MOVIE_UNSUPPORTED, MP4_MOVIE_LIMIT
} Mp4MovieResult;

/* Extract metadata, not validation of sample tables or playable codecs.
 * Supports v0/v1 mvhd/tkhd/mdhd and v0 hdlr. Unknown boxes are skipped.
 * One moov, unique nonzero track IDs, nonzero time scales, at most 32 tracks
 * and 65536 visited boxes. Output is unchanged on every failure. */
Mp4MovieResult mp4_movie_read(const Mp4Reader *reader, Mp4Movie *movie);
#endif
