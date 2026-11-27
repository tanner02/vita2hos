#define OPENSSL_API_COMPAT 0x10100000L
#include <assert.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vita_sha1.h"
static void check(const void *data,size_t size,size_t chunk)
{
    unsigned char expected[20],actual[20];assert(SHA1(data,size,expected));
    VitaSha1 context;vita_sha1_init(&context);vita_sha1_update(&context,NULL,0);
    const unsigned char *bytes=data;
    for(size_t i=0;i<size;) {
        size_t count=size-i;if(count>chunk)count=chunk;
        vita_sha1_update(&context,bytes+i,count);i+=count;
    }
    vita_sha1_finish(&context,actual);assert(!memcmp(actual,expected,20));
}
int main(void)
{
    unsigned char data[1024];
    for(unsigned i=0;i<sizeof(data);i++)data[i]=(unsigned char)(i*37+11);
    for(size_t size=0;size<=sizeof(data);size++)for(size_t chunk=1;chunk<=129;chunk=chunk*2+1)check(data,size,chunk);
    check("abc",3,1);check("",0,1);
    unsigned char *million=malloc(1000000);assert(million);memset(million,'a',1000000);check(million,1000000,101);free(million);
    unsigned char digest[20];VitaSha1 context;vita_sha1_init(&context);vita_sha1_update(&context,"abc",3);vita_sha1_finish(&context,digest);
    const unsigned char known[20]={0xa9,0x99,0x3e,0x36,0x47,0x06,0x81,0x6a,0xba,0x3e,0x25,0x71,0x78,0x50,0xc2,0x6c,0x9c,0xd0,0xd8,0x9d};
    assert(!memcmp(digest,known,20));
    puts("Portable SHA-1: known vector, every padding boundary, streamed chunks and million-byte input match OpenSSL.");
}
