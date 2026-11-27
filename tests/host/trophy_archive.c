#define OPENSSL_API_COMPAT 0x10100000L
#include <assert.h>
#include <openssl/sha.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef SHA_CTX Sha1Context;
static void sha1ContextCreate(Sha1Context *ctx){assert(SHA1_Init(ctx));}
static void sha1ContextUpdate(Sha1Context *ctx,const void *data,size_t size){assert(SHA1_Update(ctx,data,size));}
static void sha1ContextGetHash(Sha1Context *ctx,void *output){assert(SHA1_Final(output,ctx));}
static int fail_allocation;
static void *archive_calloc(size_t count,size_t size);
#define calloc archive_calloc
#include "../../source/trophy_archive.c"
#undef calloc
static void *archive_calloc(size_t count,size_t size){return fail_allocation ? NULL : calloc(count,size);}
static void be32(uint8_t *p,uint32_t value)
{for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(value>>(24-8*i));}
static void be64(uint8_t *p,uint64_t value)
{be32(p,(uint32_t)(value>>32));be32(p+4,(uint32_t)value);}
static void sign_fixture(uint8_t *data,size_t size)
{
    uint8_t hash[20];memset(data+28,0,20);
    assert(SHA1(data,size,hash));memcpy(data+28,hash,20);
}
static void fixture(uint8_t *data)
{
    memset(data,0,200);be32(data,0xdca24d00);be32(data+4,2);be64(data+8,200);
    be32(data+16,2);be32(data+20,64);
    memcpy(data+64,"TROP.CFG",9);be64(data+96,192);be64(data+104,4);
    memcpy(data+128,"TROP.CFG.EXTRA",15);be64(data+160,196);be64(data+168,4);
    memcpy(data+192,"ABCDEFGH",8);sign_fixture(data,200);
}
static void rejected(uint8_t *data,int error)
{
    TrophyArchive output={0},saved=output;
    assert(trophy_archive_open(data,200,&output)==error);
    assert(!memcmp(&output,&saved,sizeof(output)));
}
int main(int argc,char **argv)
{
    uint8_t data[200];fixture(data);TrophyArchive archive={0};
    assert(!trophy_archive_open(data,sizeof(data),&archive) && archive.count==2);
    TrophyArchive saved=archive;
    assert(trophy_archive_open(data,sizeof(data),&archive)==TROPHY_ARCHIVE_INVALID);
    assert(!memcmp(&archive,&saved,sizeof(archive)));
    assert(trophy_archive_find(&archive,"TROP.CFG") && !trophy_archive_find(&archive,"TROP"));
    assert(!trophy_archive_find(&archive,"TROP.CFG.EXTR"));
    char output[8]="zzzzzzz";
    assert(!trophy_archive_read(&archive,"TROP.CFG",1,output,3) && !memcmp(output,"BCDzzzz",7));
    assert(!trophy_archive_read(&archive,"TROP.CFG",4,NULL,0));
    assert(trophy_archive_read(&archive,"TROP.CFG",4,output,1)==TROPHY_ARCHIVE_RANGE);
    assert(trophy_archive_read(&archive,"TROP.CFG",UINT64_MAX,output,1)==TROPHY_ARCHIVE_RANGE);
    assert(trophy_archive_read(&archive,"missing",0,output,1)==TROPHY_ARCHIVE_NOT_FOUND);
    trophy_archive_close(&archive);trophy_archive_close(&archive);
    assert(!archive.entries && !archive.data && !archive.count);
    data[199]^=1;rejected(data,TROPHY_ARCHIVE_CHECKSUM);
    fixture(data);be64(data+8,UINT64_MAX);rejected(data,TROPHY_ARCHIVE_INVALID);
    fixture(data);be32(data+4,4);rejected(data,TROPHY_ARCHIVE_UNSUPPORTED);
    fixture(data);be32(data+20,128);rejected(data,TROPHY_ARCHIVE_UNSUPPORTED);
    fixture(data);be32(data+16,4097);rejected(data,TROPHY_ARCHIVE_INVALID);
    fixture(data);be32(data+16,3);rejected(data,TROPHY_ARCHIVE_INVALID);
    fixture(data);memset(data+64,'A',32);sign_fixture(data,200);rejected(data,TROPHY_ARCHIVE_INVALID);
    fixture(data);memcpy(data+64,"../bad",7);sign_fixture(data,200);rejected(data,TROPHY_ARCHIVE_INVALID);
    fixture(data);memcpy(data+128,data+64,32);sign_fixture(data,200);rejected(data,TROPHY_ARCHIVE_INVALID);
    fixture(data);be64(data+96,0);sign_fixture(data,200);rejected(data,TROPHY_ARCHIVE_INVALID);
    fixture(data);be64(data+96,UINT64_MAX);sign_fixture(data,200);rejected(data,TROPHY_ARCHIVE_INVALID);
    fixture(data);be64(data+104,UINT64_MAX);sign_fixture(data,200);rejected(data,TROPHY_ARCHIVE_INVALID);
    fixture(data);fail_allocation=1;rejected(data,TROPHY_ARCHIVE_NO_MEMORY);fail_allocation=0;
    fixture(data);be32(data+4,3);sign_fixture(data,200);assert(!trophy_archive_open(data,200,&archive));trophy_archive_close(&archive);
    fixture(data);be64(data+160,192);sign_fixture(data,200);assert(!trophy_archive_open(data,200,&archive));trophy_archive_close(&archive);
    assert(trophy_archive_open(data,63,&archive)==TROPHY_ARCHIVE_INVALID);
    assert(trophy_archive_open(NULL,200,&archive)==TROPHY_ARCHIVE_INVALID);
    assert(trophy_archive_open(data,200,NULL)==TROPHY_ARCHIVE_INVALID);
    if(argc==2) {
        FILE *file=fopen(argv[1],"rb");assert(file && !fseek(file,0,SEEK_END));long bytes=ftell(file);
        assert(bytes>=64 && bytes<=TROPHY_ARCHIVE_MAX_BYTES && !fseek(file,0,SEEK_SET));
        uint8_t *input=malloc((size_t)bytes);assert(input && fread(input,1,(size_t)bytes,file)==(size_t)bytes);
        assert(!fclose(file));assert(!trophy_archive_open(input,(size_t)bytes,&archive));
        printf("Private archive: SHA-1 and %zu entry ranges validated; no extraction or guest execution.\n",archive.count);
        trophy_archive_close(&archive);free(input);
    }
    puts("Trophy archives: integrity, exact lookup, bounded reads, malformed tables and allocation failure passed.");
}
