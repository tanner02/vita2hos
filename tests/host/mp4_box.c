#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../source/mp4_box.c"

typedef struct {
    const uint8_t *data;
    size_t size;
    uint64_t base;
    unsigned calls, fail_call;
} Source;
static bool read_at(void *context,uint64_t offset,void *output,size_t size)
{
    Source *s=context;
    ++s->calls;
    if (s->calls == s->fail_call) return false;
    assert(offset >= s->base && offset-s->base <= s->size);
    assert(size <= s->size-(offset-s->base));
    memcpy(output,s->data+(size_t)(offset-s->base),size);
    return true;
}
static void unchanged(const Mp4Reader *reader,Mp4BoxCursor initial,Mp4BoxResult result)
{
    Mp4BoxCursor cursor=initial;
    Mp4Box box,before;
    memset(&box,0xa5,sizeof(box)); memcpy(&before,&box,sizeof(box));
    assert(mp4_box_next(reader,&cursor,&box) == result);
    assert(cursor.position == initial.position && cursor.end == initial.end && cursor.top_level == initial.top_level);
    assert(!memcmp(&box,&before,sizeof(box)));
}
int main(void)
{
    /* Independently hand-written bytes: free, moov(child trak), EOF mdat. */
    const uint8_t movie[]={0,0,0,8,'f','r','e','e',
        0,0,0,16,'m','o','o','v',0,0,0,8,'t','r','a','k',
        0,0,0,0,'m','d','a','t',0xde,0xad,0xbe,0xef};
    Source s={movie,sizeof(movie),0,0,0};
    Mp4Reader reader={read_at,&s,sizeof(movie)};
    Mp4BoxCursor root=mp4_box_root(&reader); Mp4Box box;
    assert(mp4_box_next(&reader,&root,&box)==MP4_BOX_OK && box.size==8 && !box.payload_size);
    assert(box.type==MP4_FOURCC('f','r','e','e'));
    assert(mp4_box_next(&reader,&root,&box)==MP4_BOX_OK && box.payload_offset==16);
    Mp4BoxCursor child=mp4_box_children(&box);
    assert(mp4_box_next(&reader,&child,&box)==MP4_BOX_OK && box.type==MP4_FOURCC('t','r','a','k'));
    unchanged(&reader,child,MP4_BOX_END);
    assert(mp4_box_next(&reader,&root,&box)==MP4_BOX_OK && box.payload_size==4 && box.offset==24);
    unchanged(&reader,root,MP4_BOX_END);
    assert(s.calls==4); /* no media payload reads */

    /* 64-bit file offsets without allocating/mapping a 4GiB media file. */
    uint8_t large[]={0,0,0,1,'m','d','a','t',0,0,0,1,0,0,0,16};
    s=(Source){large,sizeof(large),UINT64_C(0x100000000),0,0};
    reader.size=s.base+UINT64_C(0x100000010);
    root=(Mp4BoxCursor){s.base,reader.size,true};
    assert(mp4_box_next(&reader,&root,&box)==MP4_BOX_OK);
    assert(box.size==UINT64_C(0x100000010) && box.payload_size==UINT64_C(0x100000000));
    assert(box.payload_offset==UINT64_C(0x100000010) && root.position==reader.size && s.calls==2);

    uint8_t uuid[]={0,0,0,1,'u','u','i','d',0,0,0,0,0,0,0,32,
        0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    s=(Source){uuid,sizeof(uuid),0,0,0}; reader.size=sizeof(uuid);
    root=mp4_box_root(&reader);
    assert(mp4_box_next(&reader,&root,&box)==MP4_BOX_OK && box.payload_offset==32 && !box.payload_size);
    assert(!memcmp(box.user_type,uuid+16,16));
    for (unsigned fail=1;fail<=3;++fail) {
        s.calls=0; s.fail_call=fail;
        unchanged(&reader,mp4_box_root(&reader),MP4_BOX_IO_ERROR);
    }
    s.fail_call=0;
    /* Every truncation, malformed header size, parent overrun, and nested EOF. */
    for (size_t n=1;n<sizeof(uuid);++n) {
        reader.size=n;
        unchanged(&reader,mp4_box_root(&reader),MP4_BOX_INVALID);
    }
    reader.size=sizeof(uuid);
    for (unsigned n=2;n<24;++n) {
        uuid[3]=n;
        unchanged(&reader,mp4_box_root(&reader),MP4_BOX_INVALID);
    }
    uuid[3]=24; /* ordinary UUID header (8+16), not extended */
    root=mp4_box_root(&reader);
    assert(mp4_box_next(&reader,&root,&box)==MP4_BOX_OK && box.payload_offset==24);
    uuid[3]=1;
    uuid[8]=0xff; /* oversized 64-bit length */
    unchanged(&reader,mp4_box_root(&reader),MP4_BOX_INVALID);
    uuid[8]=0;
    uuid[15]=15; /* extended size smaller than extended header */
    unchanged(&reader,mp4_box_root(&reader),MP4_BOX_INVALID);
    s=(Source){movie,sizeof(movie),0,0,0}; reader.size=sizeof(movie);
    unchanged(&reader,(Mp4BoxCursor){24,sizeof(movie),false},MP4_BOX_INVALID);
    unchanged(&reader,(Mp4BoxCursor){8,20,false},MP4_BOX_INVALID);
    unchanged(&reader,(Mp4BoxCursor){9,8,false},MP4_BOX_INVALID);
    unchanged(&reader,(Mp4BoxCursor){0,sizeof(movie)+1,true},MP4_BOX_INVALID);
    unchanged(&reader,(Mp4BoxCursor){0,8,true},MP4_BOX_INVALID);
    /* Addition near UINT64_MAX must never wrap. */
    s.base=UINT64_MAX-15; s.data=movie+8; s.size=8; reader.size=UINT64_MAX;
    unchanged(&reader,(Mp4BoxCursor){s.base,reader.size,true},MP4_BOX_INVALID);
    assert(mp4_box_next(NULL,&root,&box)==MP4_BOX_INVALID);
    puts("MP4 box reader: PASS");
    return 0;
}
