#ifndef VITA2HOS_MP4_BOX_H
#define VITA2HOS_MP4_BOX_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MP4_FOURCC(a,b,c,d) (((uint32_t)(a)<<24)|((uint32_t)(b)<<16)|((uint32_t)(c)<<8)|(uint32_t)(d))

/* Exact positional read: return true only after filling all requested bytes.
 * The source must remain stable for the reader's lifetime. No payload buffering,
 * allocation, or dependency on the guest's currently undocumented MP4 ABI. */
typedef bool (*Mp4ReadAt)(void *context, uint64_t offset, void *data, size_t size);
typedef struct { Mp4ReadAt read_at; void *context; uint64_t size; } Mp4Reader;
typedef struct { uint64_t position, end; bool top_level; } Mp4BoxCursor;
typedef struct {
    uint64_t offset, size, payload_offset, payload_size;
    uint32_t type;
    uint8_t user_type[16]; /* UUID only; zero for other box types. */
} Mp4Box;
typedef enum {
    MP4_BOX_OK, MP4_BOX_END, MP4_BOX_INVALID, MP4_BOX_IO_ERROR
} Mp4BoxResult;

Mp4BoxCursor mp4_box_root(const Mp4Reader *reader);
/* Requires an unmodified box returned by mp4_box_next. The caller chooses
 * actual container boxes; opaque media/sample-entry bytes
 * must not be recursively interpreted as child boxes. */
Mp4BoxCursor mp4_box_children(const Mp4Box *box);
/* On END or any error, both cursor and output remain unchanged. Size-zero
 * atoms are supported at file level only (QuickTime atom convention).
 * Unknown types are returned normally and can be skipped without reading data. */
Mp4BoxResult mp4_box_next(const Mp4Reader *reader, Mp4BoxCursor *cursor, Mp4Box *box);
#endif
