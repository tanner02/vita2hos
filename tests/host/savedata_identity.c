#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../source/savedata_identity.c"
static void put32(unsigned char *p,uint32_t value)
{for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(value>>(8*i));}
static void fixture(unsigned char p[160],bool override)
{
    memset(p,0,160);memcpy(p,"\0PSF",4);put32(p+4,0x101);
    put32(p+8,52);put32(p+12,96);put32(p+16,override ? 2 : 1);
    memcpy(p+52,"TITLE_ID",9);memcpy(p+61,"INSTALL_DIR_SAVEDATA",20);
    p[22]=4;p[23]=2;put32(p+24,10);put32(p+28,16);
    memcpy(p+96,"PCSF00516",10);
    p[36]=9;p[38]=4;p[39]=2;put32(p+40,10);put32(p+44,16);put32(p+48,16);
    memcpy(p+112,"PCSA12345",10);
}
int main(int argc,char **argv)
{
    unsigned char p[160];SavedataIdentity id,before;
    fixture(p,false);assert(!savedata_identity_parse(p,sizeof(p),&id));
    assert(!strcmp(id.title,"PCSF00516") && !strcmp(id.directory,id.title));
    fixture(p,true);assert(!savedata_identity_parse(p,sizeof(p),&id));
    assert(!strcmp(id.directory,"PCSA12345"));before=id;
    for(size_t n=0;n<128;n++) {
        assert(savedata_identity_parse(p,n,&id)<0);assert(!memcmp(&id,&before,sizeof(id)));
    }
    memcpy(p+112,"../escape",10);assert(savedata_identity_parse(p,sizeof(p),&id)==-EINVAL);
    fixture(p,true);p[36]=0;assert(savedata_identity_parse(p,sizeof(p),&id)==-EINVAL);
    fixture(p,true);put32(p+48,UINT32_MAX);assert(savedata_identity_parse(p,sizeof(p),&id)==-EINVAL);
    fixture(p,true);p[23]=0;assert(savedata_identity_parse(p,sizeof(p),&id)==-EINVAL);
    assert(!memcmp(&id,&before,sizeof(id)));
    if(argc==2) {
        FILE *f=fopen(argv[1],"rb");assert(f);unsigned char *data=malloc(1024*1024+1);assert(data);
        size_t size=fread(data,1,1024*1024+1,f);assert(!ferror(f) && !fclose(f));
        assert(!savedata_identity_parse(data,size,&id));free(data);
        printf("Private PARAM.SFO identity validated: %s / %s\n",id.title,id.directory);
    }
    puts("Savedata identity: bounded tables, fallback, override, duplicates, traversal and output preservation passed.");
}
