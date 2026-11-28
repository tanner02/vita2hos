#include "mp4_movie.h"

static uint32_t word(const uint8_t *p)
{
    return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3];
}
static uint64_t longword(const uint8_t *p)
{
    return (uint64_t)word(p)<<32 | word(p+4);
}
static Mp4MovieResult next(const Mp4Reader *r,Mp4BoxCursor *c,Mp4Box *b,unsigned *budget,bool *end)
{
    *end=false;
    if (!*budget) return MP4_MOVIE_LIMIT;
    --*budget;
    Mp4BoxResult result=mp4_box_next(r,c,b);
    if (result==MP4_BOX_END) { *end=true; return MP4_MOVIE_OK; }
    if (result==MP4_BOX_IO_ERROR) return MP4_MOVIE_IO_ERROR;
    return result==MP4_BOX_OK ? MP4_MOVIE_OK : MP4_MOVIE_INVALID;
}
static Mp4MovieResult header(const Mp4Reader *r,const Mp4Box *b,uint8_t *data,
                             size_t size0,size_t size1)
{
    if (b->payload_size<4) return MP4_MOVIE_INVALID;
    if (!r->read_at(r->context,b->payload_offset,data,4)) return MP4_MOVIE_IO_ERROR;
    if (data[0]>1 || (data[0]==1 && !size1)) return MP4_MOVIE_UNSUPPORTED;
    size_t size=data[0] ? size1 : size0;
    if (b->payload_size<size) return MP4_MOVIE_INVALID;
    if (!r->read_at(r->context,b->payload_offset+4,data+4,size-4)) return MP4_MOVIE_IO_ERROR;
    return MP4_MOVIE_OK;
}
static Mp4MovieResult timing(const Mp4Reader *r,const Mp4Box *b,bool movie,
                             uint32_t *scale,uint64_t *duration,bool *unknown)
{
    uint8_t data[112];
    Mp4MovieResult result=header(r,b,data,movie?100:24,movie?112:36);
    if (result!=MP4_MOVIE_OK) return result;
    unsigned offset=data[0]?20:12;
    *scale=word(data+offset);
    *duration=data[0]?longword(data+offset+4):word(data+offset+4);
    *unknown=*duration==(data[0]?UINT64_MAX:UINT32_MAX);
    return *scale ? MP4_MOVIE_OK : MP4_MOVIE_INVALID;
}
static Mp4MovieResult media(const Mp4Reader *r,const Mp4Box *parent,Mp4Track *t,unsigned *budget)
{
    Mp4BoxCursor c=mp4_box_children(parent); unsigned seen=0;
    for (;;) {
        Mp4Box b; bool end;
        Mp4MovieResult result=next(r,&c,&b,budget,&end);
        if (result!=MP4_MOVIE_OK) return result;
        if (end) return seen==3 ? MP4_MOVIE_OK : MP4_MOVIE_INVALID;
        if (b.type==MP4_FOURCC('m','d','h','d')) {
            if (seen&1) return MP4_MOVIE_INVALID;
            seen|=1;
            result=timing(r,&b,false,&t->timescale,&t->media_duration,&t->media_duration_unknown);
        } else if (b.type==MP4_FOURCC('h','d','l','r')) {
            if (seen&2) return MP4_MOVIE_INVALID;
            seen|=2;
            uint8_t data[24]; result=header(r,&b,data,24,0);
            if (result==MP4_MOVIE_OK) t->handler=word(data+8);
        }
        if (result!=MP4_MOVIE_OK) return result;
    }
}
static Mp4MovieResult track(const Mp4Reader *r,const Mp4Box *parent,Mp4Track *t,unsigned *budget)
{
    Mp4BoxCursor c=mp4_box_children(parent); unsigned seen=0;
    t->container=*parent;
    for (;;) {
        Mp4Box b; bool end;
        Mp4MovieResult result=next(r,&c,&b,budget,&end);
        if (result!=MP4_MOVIE_OK) return result;
        if (end) return seen==3 ? MP4_MOVIE_OK : MP4_MOVIE_INVALID;
        if (b.type==MP4_FOURCC('t','k','h','d')) {
            if (seen&1) return MP4_MOVIE_INVALID;
            seen|=1;
            uint8_t data[96]; result=header(r,&b,data,84,96);
            if (result!=MP4_MOVIE_OK) return result;
            unsigned offset=data[0]?20:12;
            t->flags=word(data)&0xffffff;
            t->id=word(data+offset);
            if (!t->id) return MP4_MOVIE_INVALID;
            t->movie_duration=data[0]?longword(data+offset+8):word(data+offset+8);
            t->movie_duration_unknown=t->movie_duration==(data[0]?UINT64_MAX:UINT32_MAX);
            t->width_fixed=word(data+(data[0]?88:76));
            t->height_fixed=word(data+(data[0]?92:80));
        } else if (b.type==MP4_FOURCC('m','d','i','a')) {
            if (seen&2) return MP4_MOVIE_INVALID;
            seen|=2;
            result=media(r,&b,t,budget);
        }
        if (result!=MP4_MOVIE_OK) return result;
    }
}
static Mp4MovieResult movie(const Mp4Reader *r,const Mp4Box *parent,Mp4Movie *m,unsigned *budget)
{
    Mp4BoxCursor c=mp4_box_children(parent); bool seen=false;
    for (;;) {
        Mp4Box b; bool end;
        Mp4MovieResult result=next(r,&c,&b,budget,&end);
        if (result!=MP4_MOVIE_OK) return result;
        if (end) return seen ? MP4_MOVIE_OK : MP4_MOVIE_INVALID;
        if (b.type==MP4_FOURCC('m','v','h','d')) {
            if (seen) return MP4_MOVIE_INVALID;
            seen=true;
            result=timing(r,&b,true,&m->timescale,&m->duration,&m->duration_unknown);
        } else if (b.type==MP4_FOURCC('t','r','a','k')) {
            if (m->track_count==MP4_MAX_TRACKS) return MP4_MOVIE_LIMIT;
            Mp4Track *t=&m->tracks[m->track_count];
            result=track(r,&b,t,budget);
            if (result!=MP4_MOVIE_OK) return result;
            for (unsigned i=0;i<m->track_count;++i)
                if (m->tracks[i].id==t->id) return MP4_MOVIE_INVALID;
            ++m->track_count;
        } else if (b.type==MP4_FOURCC('m','v','e','x')) m->fragmented=true;
        if (result!=MP4_MOVIE_OK) return result;
    }
}
Mp4MovieResult mp4_movie_read(const Mp4Reader *r,Mp4Movie *output)
{
    if (!r || !r->read_at || !output) return MP4_MOVIE_INVALID;
    Mp4Movie candidate={0}; Mp4BoxCursor c=mp4_box_root(r);
    unsigned budget=65536; bool seen=false;
    for (;;) {
        Mp4Box b; bool end;
        Mp4MovieResult result=next(r,&c,&b,&budget,&end);
        if (result!=MP4_MOVIE_OK) return result;
        if (end) {
            if (!seen) return MP4_MOVIE_INVALID;
            *output=candidate; return MP4_MOVIE_OK;
        }
        if (b.type==MP4_FOURCC('m','o','o','v')) {
            if (seen) return MP4_MOVIE_INVALID;
            seen=true; result=movie(r,&b,&candidate,&budget);
        } else if (b.type==MP4_FOURCC('m','o','o','f')) candidate.fragmented=true;
        if (result!=MP4_MOVIE_OK) return result;
    }
}
