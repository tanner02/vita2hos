#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "../../source/mp4_box.c"
#include "../../source/mp4_movie.c"

static uint8_t bytes[16384];
static size_t used, mv_version, mv_scale, track_id, handler_type, media_version;
static unsigned reads, fail_read;
static void u32(uint32_t v) { assert(used+4<=sizeof(bytes)); bytes[used++]=v>>24; bytes[used++]=v>>16; bytes[used++]=v>>8; bytes[used++]=v; }
static void u64(uint64_t v) { u32(v>>32); u32(v); }
static void zeros(unsigned n) { assert(used+n<=sizeof(bytes)); memset(bytes+used,0,n); used+=n; }
static size_t begin(uint32_t type) { size_t start=used; u32(0); u32(type); return start; }
static void patch(size_t p,uint32_t v) { bytes[p]=v>>24; bytes[p+1]=v>>16; bytes[p+2]=v>>8; bytes[p+3]=v; }
static void finish(size_t p) { patch(p,used-p); }
static void dates(bool v1) { if(v1) { u64(1); u64(2); } else { u32(1); u32(2); } }
static bool read_at(void *ctx,uint64_t offset,void *data,size_t n)
{
    (void)ctx;
    if (++reads==fail_read) return false;
    assert(offset<=used && n<=used-offset);
    memcpy(data,bytes+(size_t)offset,n); return true;
}
/* Field order assembled independently of the parser, including unparsed fields. */
static void make(bool v1,unsigned count)
{
    used=0; reads=0; fail_read=0;
    size_t free=begin(MP4_FOURCC('f','r','e','e')); finish(free);
    size_t moov=begin(MP4_FOURCC('m','o','o','v'));
    size_t mvhd=begin(MP4_FOURCC('m','v','h','d'));
    mv_version=used; u32(v1?0x01000000:0); dates(v1);
    mv_scale=used; u32(1000);
    if(v1) u64(UINT64_C(0x100000005)); else u32(3000);
    zeros(80); finish(mvhd);
    for(unsigned i=0;i<count;++i) {
        size_t trak=begin(MP4_FOURCC('t','r','a','k'));
        size_t tkhd=begin(MP4_FOURCC('t','k','h','d'));
        u32(v1?0x01000003:3); dates(v1);
        track_id=used; u32(i+1); u32(0);
        if(v1) u64(i?UINT64_MAX:UINT64_C(0x100000004)); else u32(i?UINT32_MAX:2999);
        zeros(52); u32(960u<<16); u32(544u<<16); finish(tkhd);
        size_t mdia=begin(MP4_FOURCC('m','d','i','a'));
        /* Deliberately put hdlr before mdhd: order must not matter. */
        size_t hdlr=begin(MP4_FOURCC('h','d','l','r'));
        u32(0); u32(0); handler_type=used;
        u32(i?MP4_FOURCC('s','o','u','n'):MP4_FOURCC('v','i','d','e'));
        zeros(12); finish(hdlr);
        size_t mdhd=begin(MP4_FOURCC('m','d','h','d'));
        media_version=used; u32(v1?0x01000000:0); dates(v1); u32(i?48000:90000);
        if(v1) u64(i?UINT64_MAX:UINT64_C(0x200000007)); else u32(i?UINT32_MAX:270000);
        u32(0); finish(mdhd);
        finish(mdia); finish(trak);
    }
    finish(moov);
    size_t mdat=begin(MP4_FOURCC('m','d','a','t')); zeros(17); finish(mdat);
}
static Mp4MovieResult parse(Mp4Movie *out)
{
    Mp4Reader reader={read_at,NULL,used}; return mp4_movie_read(&reader,out);
}
static void failure(Mp4MovieResult expected)
{
    Mp4Movie out,before; memset(&out,0xa5,sizeof(out)); memcpy(&before,&out,sizeof(out));
    Mp4MovieResult actual=parse(&out);
    if(actual!=expected) fprintf(stderr,"result=%d expected=%d bytes=%zu fail_read=%u reads=%u\n",actual,expected,used,fail_read,reads);
    assert(actual==expected); assert(!memcmp(&out,&before,sizeof(out)));
}
static bool file_read(void *ctx,uint64_t offset,void *data,size_t n)
{
    return offset<=(uint64_t)INT64_MAX && fseeko(ctx,(off_t)offset,SEEK_SET)==0 && fread(data,1,n,ctx)==n;
}
static bool free_read(void *ctx,uint64_t offset,void *data,size_t n)
{
    unsigned *count=ctx; ++*count;
    assert(n==8 && !(offset%8));
    memcpy(data,"\0\0\0\10free",8); return true;
}
int main(int argc,char **argv)
{
    for(unsigned v1=0;v1<2;++v1) {
        make(v1,2); Mp4Movie out;
        assert(parse(&out)==MP4_MOVIE_OK);
        assert(out.timescale==1000 && out.track_count==2 && !out.fragmented && !out.duration_unknown);
        assert(out.duration==(v1?UINT64_C(0x100000005):3000));
        assert(out.tracks[0].id==1 && out.tracks[0].flags==3);
        assert(out.tracks[0].timescale==90000 && out.tracks[1].timescale==48000);
        assert(out.tracks[0].handler==MP4_FOURCC('v','i','d','e'));
        assert(out.tracks[1].handler==MP4_FOURCC('s','o','u','n'));
        assert(out.tracks[0].width_fixed==960u<<16 && out.tracks[0].height_fixed==544u<<16);
        assert(out.tracks[0].media_duration==(v1?UINT64_C(0x200000007):270000));
        assert(!out.tracks[0].movie_duration_unknown && !out.tracks[0].media_duration_unknown);
        assert(out.tracks[1].movie_duration_unknown && out.tracks[1].media_duration_unknown);
        unsigned total=reads;
        for(unsigned i=1;i<=total;++i) { reads=0; fail_read=i; failure(MP4_MOVIE_IO_ERROR); }
        make(v1,2); patch(track_id,1); failure(MP4_MOVIE_INVALID);
        make(v1,1); patch(track_id,0); failure(MP4_MOVIE_INVALID);
        make(v1,1); patch(mv_scale,0); failure(MP4_MOVIE_INVALID);
        make(v1,1); bytes[mv_version]=2; failure(MP4_MOVIE_UNSUPPORTED);
        make(v1,1); bytes[media_version]=2; failure(MP4_MOVIE_UNSUPPORTED);
        make(v1,1); bytes[handler_type-8]=1; failure(MP4_MOVIE_UNSUPPORTED);
        make(v1,1); patch(handler_type,MP4_FOURCC('t','e','x','t'));
        assert(parse(&out)==MP4_MOVIE_OK && out.tracks[0].handler==MP4_FOURCC('t','e','x','t'));
    }
    make(false,MP4_MAX_TRACKS); Mp4Movie out;
    assert(parse(&out)==MP4_MOVIE_OK && out.track_count==MP4_MAX_TRACKS);
    make(false,MP4_MAX_TRACKS+1); failure(MP4_MOVIE_LIMIT);
    make(false,0); assert(parse(&out)==MP4_MOVIE_OK && !out.track_count);
    make(false,1); patch(12,MP4_FOURCC('f','r','e','e')); failure(MP4_MOVIE_INVALID); /* no moov */
    make(false,1); patch(mv_version-4,MP4_FOURCC('f','r','e','e')); failure(MP4_MOVIE_INVALID); /* no mvhd */
    make(false,1); patch(media_version-4,MP4_FOURCC('f','r','e','e')); failure(MP4_MOVIE_INVALID); /* no mdhd */
    make(false,1); patch(handler_type-12,MP4_FOURCC('f','r','e','e')); failure(MP4_MOVIE_INVALID); /* no hdlr */
    make(false,1); size_t full=used;
    for(size_t n=1;n<full;++n) {
        used=n; reads=0;
        /* Complete metadata remains valid without the optional mdat. */
        if(n==full-25) assert(parse(&out)==MP4_MOVIE_OK);
        else failure(MP4_MOVIE_INVALID);
    }
    used=full;
    make(false,1); size_t moov_size=used-33;
    memmove(bytes+used,bytes+8,moov_size); used+=moov_size;
    failure(MP4_MOVIE_INVALID); /* two moov boxes */
    unsigned count=0;
    Mp4Reader many={free_read,&count,UINT64_C(8)*65537};
    memset(&out,0xa5,sizeof(out)); Mp4Movie before; memcpy(&before,&out,sizeof(out));
    assert(mp4_movie_read(&many,&out)==MP4_MOVIE_LIMIT && count==65536);
    assert(!memcmp(&out,&before,sizeof(out)));
    assert(mp4_movie_read(NULL,&out)==MP4_MOVIE_INVALID);
    assert(mp4_movie_read(&many,NULL)==MP4_MOVIE_INVALID);
    if(argc==2) {
        FILE *f=fopen(argv[1],"rb"); assert(f);
        assert(!fseeko(f,0,SEEK_END)); off_t length=ftello(f); assert(length>=0);
        Mp4Reader reader={file_read,f,(uint64_t)length};
        assert(mp4_movie_read(&reader,&out)==MP4_MOVIE_OK);
        printf("movie scale=%u duration=%llu tracks=%u fragmented=%u\n",out.timescale,(unsigned long long)out.duration,out.track_count,out.fragmented);
        for(unsigned i=0;i<out.track_count;++i)
            printf("track id=%u scale=%u duration=%llu\n",out.tracks[i].id,out.tracks[i].timescale,(unsigned long long)out.tracks[i].media_duration);
        fclose(f);
    }
    puts("MP4 movie metadata: PASS");
    return 0;
}
