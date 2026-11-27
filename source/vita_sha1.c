/* Portable SHA-1 for TRP integrity. libnx32 exposes the crypto declarations
 * but does not provide their implementation. No authentication is implied. */
#include <string.h>
#include "vita_sha1.h"
static uint32_t rotate(uint32_t value,unsigned bits)
{return (value<<bits)|(value>>(32-bits));}
static void transform(VitaSha1 *context)
{
    uint32_t words[80];
    for(unsigned i=0;i<16;i++) {
        const uint8_t *p=context->block+i*4;
        words[i]=(uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];
    }
    for(unsigned i=16;i<80;i++)words[i]=rotate(words[i-3]^words[i-8]^words[i-14]^words[i-16],1);
    uint32_t a=context->state[0],b=context->state[1],c=context->state[2],d=context->state[3],e=context->state[4];
    for(unsigned i=0;i<80;i++) {
        uint32_t function,constant;
        if(i<20){function=(b&c)|(~b&d);constant=0x5a827999;}
        else if(i<40){function=b^c^d;constant=0x6ed9eba1;}
        else if(i<60){function=(b&c)|(b&d)|(c&d);constant=0x8f1bbcdc;}
        else {function=b^c^d;constant=0xca62c1d6;}
        uint32_t next=rotate(a,5)+function+e+constant+words[i];
        e=d;d=c;c=rotate(b,30);b=a;a=next;
    }
    context->state[0]+=a;context->state[1]+=b;context->state[2]+=c;
    context->state[3]+=d;context->state[4]+=e;
}
void vita_sha1_init(VitaSha1 *context)
{
    *context=(VitaSha1){.state={0x67452301,0xefcdab89,0x98badcfe,0x10325476,0xc3d2e1f0}};
}
void vita_sha1_update(VitaSha1 *context,const void *data,size_t size)
{
    const uint8_t *source=data;context->bytes+=size;
    while(size) {
        size_t count=64-context->used;if(count>size)count=size;
        memcpy(context->block+context->used,source,count);
        source+=count;size-=count;context->used+=count;
        if(context->used==64){transform(context);context->used=0;}
    }
}
void vita_sha1_finish(VitaSha1 *context,uint8_t digest[20])
{
    uint64_t bits=context->bytes*8;
    uint8_t padding[128]={0x80},length[8];
    for(unsigned i=0;i<8;i++)length[i]=(uint8_t)(bits>>(56-i*8));
    size_t count=context->used<56 ? 56-context->used : 120-context->used;
    vita_sha1_update(context,padding,count);vita_sha1_update(context,length,sizeof(length));
    for(unsigned i=0;i<5;i++)for(unsigned j=0;j<4;j++)digest[i*4+j]=(uint8_t)(context->state[i]>>(24-j*8));
}
