#include "mp4_box.h"

static uint32_t be32(const uint8_t *p)
{
    return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3];
}
static uint64_t be64(const uint8_t *p)
{
    return (uint64_t)be32(p)<<32 | be32(p+4);
}
Mp4BoxCursor mp4_box_root(const Mp4Reader *reader)
{
    return (Mp4BoxCursor){0, reader ? reader->size : 0, true};
}
Mp4BoxCursor mp4_box_children(const Mp4Box *box)
{
    return (Mp4BoxCursor){box->payload_offset, box->offset+box->size, false};
}
Mp4BoxResult mp4_box_next(const Mp4Reader *reader, Mp4BoxCursor *cursor, Mp4Box *box)
{
    if (!reader || !reader->read_at || !cursor || !box ||
        cursor->position > cursor->end || cursor->end > reader->size ||
        (cursor->top_level && cursor->end != reader->size))
        return MP4_BOX_INVALID;
    uint64_t remaining=cursor->end-cursor->position;
    if (!remaining) return MP4_BOX_END;
    if (remaining < 8) return MP4_BOX_INVALID;
    uint8_t header[16];
    if (!reader->read_at(reader->context,cursor->position,header,8)) return MP4_BOX_IO_ERROR;
    Mp4Box candidate={.offset=cursor->position,.type=be32(header+4)};
    uint64_t header_size=8;
    candidate.size=be32(header);
    if (candidate.size == 1) {
        if (remaining < 16) return MP4_BOX_INVALID;
        if (!reader->read_at(reader->context,cursor->position+8,header,8)) return MP4_BOX_IO_ERROR;
        candidate.size=be64(header);
        header_size=16;
    } else if (!candidate.size) {
        if (!cursor->top_level) return MP4_BOX_INVALID;
        candidate.size=remaining;
    }
    if (candidate.type == MP4_FOURCC('u','u','i','d')) header_size+=16;
    if (candidate.size < header_size || candidate.size > remaining) return MP4_BOX_INVALID;
    if (candidate.type == MP4_FOURCC('u','u','i','d') &&
        !reader->read_at(reader->context,cursor->position+header_size-16,candidate.user_type,16))
        return MP4_BOX_IO_ERROR;
    candidate.payload_offset=cursor->position+header_size;
    candidate.payload_size=candidate.size-header_size;
    *box=candidate;
    cursor->position+=candidate.size;
    return MP4_BOX_OK;
}
