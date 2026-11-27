#define OPENSSL_API_COMPAT 0x10100000L
#define TEST_THREADED_MUTEX
#include <assert.h>
#include <openssl/sha.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static char test_root[512];
#define VITA2HOS_APP0_PATH test_root
static int fail_buffer;
static void *context_malloc(size_t size);
#define malloc context_malloc
#include "../../source/modules/SceNpTrophy.c"
#undef malloc
#include "../../source/module.c"
static void *context_malloc(size_t size){return fail_buffer ? NULL : malloc(size);}
SceUID SceSysmem_get_next_uid(void){static atomic_int next=100;return atomic_fetch_add(&next,1);}
void log_printf(const char *format,...){(void)format;}
static void be32(uint8_t *p,uint32_t value){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(value>>(24-8*i));}
static void be64(uint8_t *p,uint64_t value){be32(p,(uint32_t)(value>>32));be32(p+4,(uint32_t)value);}
static void archive_path(char *path,size_t size,const TrophyCommunicationId *id)
{assert(snprintf(path,size,"%s/sce_sys/trophy/%.9s_%02u/TROPHY.TRP",test_root,id->data,(unsigned)id->num)>0);}
static void write_fixture(const TrophyCommunicationId *id,unsigned faults)
{
    char folder[1024],path[1024],xml[512];
    snprintf(folder,sizeof(folder),"%s/sce_sys/trophy/%.9s_%02u",test_root,id->data,(unsigned)id->num);
    assert(!mkdir(folder,0700) || errno==EEXIST);
    int n=snprintf(xml,sizeof(xml),"<trophyconf><npcommid>%.9s_%02u</npcommid><group id='1'/><trophy id='0' gid='0' ttype='P' hidden='no'/><trophy id='1' gid='1' ttype='B' hidden='yes'/></trophyconf>",id->data,(unsigned)id->num);
    assert(n>0 && (size_t)n<sizeof(xml));
    if(faults&2)xml[1]='x';
    if(faults&4){char *identity=strstr(xml,"NPWR");assert(identity);identity[8]='9';}
    size_t bytes=192+(size_t)n+4;
    uint8_t *data=calloc(1,bytes);assert(data);
    be32(data,0xdca24d00);be32(data+4,2);be64(data+8,bytes);be32(data+16,2);be32(data+20,64);
    memcpy(data+64,"TROPCONF.SFM",13);be64(data+96,192);be64(data+104,(unsigned)n);
    memcpy(data+128,"ICON0.PNG",10);be64(data+160,192+(unsigned)n);be64(data+168,4);
    memcpy(data+192,xml,(size_t)n);memcpy(data+192+n,"OWND",4);
    uint8_t hash[20];assert(SHA1(data,bytes,hash));memcpy(data+28,hash,20);
    if(faults&1)data[bytes-1]^=1;
    archive_path(path,sizeof(path),id);FILE *file=fopen(path,"wb");assert(file);
    assert(fwrite(data,1,bytes,file)==bytes && !fclose(file));free(data);
}
static TrophyCommunicationId communication(unsigned index)
{
    char name[10];assert(snprintf(name,sizeof(name),"NPWR%05u",index)==9);
    TrophyCommunicationId id={0};memcpy(id.data,name,9);return id;
}
int main(int argc,char **argv)
{
    char temporary[]="/tmp/vita2hos-trophy-context-XXXXXX";assert(mkdtemp(temporary));
    strcpy(test_root,temporary);char path[1024];snprintf(path,sizeof(path),"%s/sce_sys",test_root);assert(!mkdir(path,0700));
    snprintf(path,sizeof(path),"%s/sce_sys/trophy",test_root);assert(!mkdir(path,0700));
    TrophyCommunicationId id=communication(1);unsigned char signature=1;int32_t context=777,handle;
    assert(sceNpTrophyCreateContext(&context,&id,&signature,0)==TROPHY_NOT_INITIALIZED && context==777);
    assert(!sceNpTrophyInit(NULL));
    assert(sceNpTrophyCreateContext(NULL,&id,&signature,0)==TROPHY_INVALID_ARGUMENT);
    assert(sceNpTrophyCreateContext(&context,&id,&signature,1)==TROPHY_INVALID_ARGUMENT);
    assert(sceNpTrophyCreateContext(&context,NULL,&signature,0)==TROPHY_INVALID_COMM_ID);
    assert(sceNpTrophyCreateContext(&context,&id,NULL,0)==TROPHY_INVALID_COMM_SIGN);
    assert(sceNpTrophyCreateContext(&context,&id,&signature,0)==TROPHY_TRP_NOT_FOUND && context==777);
    write_fixture(&id,true);
    assert(sceNpTrophyCreateContext(&context,&id,&signature,0)==TROPHY_TRP_VERIFICATION && context==777 && !context_bytes);
    write_fixture(&id,2);
    assert(sceNpTrophyCreateContext(&context,&id,&signature,0)==TROPHY_INVALID_CONF && context==777 && !context_bytes);
    write_fixture(&id,4);
    assert(sceNpTrophyCreateContext(&context,&id,&signature,0)==TROPHY_INVALID_COMM_ID && context==777 && !context_bytes);
    write_fixture(&id,false);fail_buffer=1;
    assert(sceNpTrophyCreateContext(&context,&id,&signature,0)==TROPHY_NO_MEMORY && context==777);
    fail_buffer=0;assert(!sceNpTrophyCreateContext(&context,&id,&signature,0) && context>0);
    TrophyContext *record=find_context(context);assert(record && record->metadata.trophy_count==2 && record->metadata.group_count==1);
    int32_t duplicate=555;assert(sceNpTrophyCreateContext(&duplicate,&id,&signature,0)==TROPHY_CONTEXT_EXISTS && duplicate==555);
    assert(!sceNpTrophyCreateHandle(&handle));uint32_t bytes=0;
    assert(!sceNpTrophyGetGameIcon(context,handle,NULL,&bytes) && bytes==4);
    char icon[8]="zzzzzzz";bytes=2;
    assert(sceNpTrophyGetGameIcon(context,handle,icon,&bytes)==TROPHY_INSUFFICIENT_BUFFER && bytes==4 && icon[0]=='z');
    bytes=sizeof(icon);assert(!sceNpTrophyGetGameIcon(context,handle,icon,&bytes) && bytes==4 && !memcmp(icon,"OWNDzzz",7));
    assert(!sceNpTrophyAbortHandle(handle));assert(sceNpTrophyGetGameIcon(context,handle,NULL,&bytes)==TROPHY_ABORT);
    assert(!sceNpTrophyDestroyHandle(handle));assert(!sceNpTrophyDestroyContext(context) && !context_bytes);
    assert(sceNpTrophyDestroyContext(context)==TROPHY_INVALID_CONTEXT);
    int32_t contexts[TROPHY_CONTEXT_LIMIT];
    for(unsigned i=0;i<TROPHY_CONTEXT_LIMIT;i++) {
        id=communication(i+1);write_fixture(&id,false);assert(!sceNpTrophyCreateContext(&contexts[i],&id,&signature,0));
    }
    id=communication(99);assert(sceNpTrophyCreateContext(&duplicate,&id,&signature,0)==TROPHY_EXCEEDS_MAX);
    assert(!sceNpTrophyTerm() && !context_bytes && !trophy_contexts);
    assert(!sceNpTrophyInit(NULL));assert(sceNpTrophyDestroyContext(contexts[0])==TROPHY_INVALID_CONTEXT);SceNpTrophy_finish();
    for(unsigned i=0;i<TROPHY_CONTEXT_LIMIT;i++) {
        id=communication(i+1);archive_path(path,sizeof(path),&id);assert(!unlink(path));
        snprintf(path,sizeof(path),"%s/sce_sys/trophy/%.9s_%02u",test_root,id.data,(unsigned)id.num);assert(!rmdir(path));
    }
    snprintf(path,sizeof(path),"%s/sce_sys/trophy",test_root);assert(!rmdir(path));
    snprintf(path,sizeof(path),"%s/sce_sys",test_root);assert(!rmdir(path));assert(!rmdir(test_root));
    if(argc==2) {
        assert(strlen(argv[1])<sizeof(test_root));strcpy(test_root,argv[1]);id=communication(2031);
        assert(!sceNpTrophyInit(NULL));assert(!sceNpTrophyCreateContext(&context,&id,&signature,0));
        record=find_context(context);assert(record && record->metadata.trophy_count==84 && record->metadata.group_count==4);
        assert(!sceNpTrophyCreateHandle(&handle));bytes=0;assert(!sceNpTrophyGetGameIcon(context,handle,NULL,&bytes) && bytes>8);
        printf("Private context: verified archive, 84 trophies, 4 groups and %u icon bytes; no writes or guest execution.\n",bytes);
        SceNpTrophy_finish();
    }
    puts("Trophy contexts: real archive/configuration, identity, failures, ownership, icons, bounded capacity and teardown passed.");
}
