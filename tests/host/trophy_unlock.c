#define OPENSSL_API_COMPAT 0x10100000L
#define TEST_THREADED_MUTEX
#include <assert.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
static char test_root[512];
#define VITA2HOS_APP0_PATH test_root
#include "../../source/modules/SceNpTrophy.c"
#include "../../source/modules/SceSysmodule.c"
#include "../../source/module.c"
SceUID SceSysmem_get_next_uid(void){static int next=500;return next++;}
bool SceNet_host_ready(void){return false;}
bool SceNet_is_initialized(void){return false;}
bool SceAppUtil_is_initialized(void){return false;}
void log_printf(const char *format,...){(void)format;}
static void be32(uint8_t *p,uint32_t value){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(value>>(24-8*i));}
static void be64(uint8_t *p,uint64_t value){be32(p,(uint32_t)(value>>32));be32(p+4,(uint32_t)value);}
static void archive_path(char *path,size_t size,const TrophyCommunicationId *id)
{assert(snprintf(path,size,"%s/sce_sys/trophy/%.9s_%02u/TROPHY.TRP",test_root,id->data,(unsigned)id->num)>0);}
/* One platinum (id 0) and two bronze (ids 1,2) trophies; the platinum
 * auto-unlocks once both bronze trophies are held. */
static void write_fixture(const TrophyCommunicationId *id)
{
    char folder[1024],path[1024],xml[640];
    snprintf(folder,sizeof(folder),"%s/sce_sys/trophy/%.9s_%02u",test_root,id->data,(unsigned)id->num);
    assert(!mkdir(folder,0700) || errno==EEXIST);
    int n=snprintf(xml,sizeof(xml),"<trophyconf><npcommid>%.9s_%02u</npcommid><group id='1'/><trophy id='0' gid='0' ttype='P' hidden='no'/><trophy id='1' gid='1' ttype='B' hidden='no'/><trophy id='2' gid='1' ttype='B' hidden='no'/></trophyconf>",id->data,(unsigned)id->num);
    assert(n>0 && (size_t)n<sizeof(xml));
    size_t bytes=192+(size_t)n+4;
    uint8_t *data=calloc(1,bytes);assert(data);
    be32(data,0xdca24d00);be32(data+4,2);be64(data+8,bytes);be32(data+16,2);be32(data+20,64);
    memcpy(data+64,"TROPCONF.SFM",13);be64(data+96,192);be64(data+104,(unsigned)n);
    memcpy(data+128,"ICON0.PNG",10);be64(data+160,192+(unsigned)n);be64(data+168,4);
    memcpy(data+192,xml,(size_t)n);memcpy(data+192+n,"OWND",4);
    uint8_t hash[20];assert(SHA1(data,bytes,hash));memcpy(data+28,hash,20);
    archive_path(path,sizeof(path),id);FILE *file=fopen(path,"wb");assert(file);
    assert(fwrite(data,1,bytes,file)==bytes && !fclose(file));free(data);
}
static TrophyCommunicationId communication(void)
{
    TrophyCommunicationId id={0};memcpy(id.data,"NPWR00042",9);id.num=0;return id;
}
int main(void)
{
    char temporary[]="/tmp/vita2hos-trophy-unlock-XXXXXX";assert(mkdtemp(temporary));
    strcpy(test_root,temporary);char path[1024];snprintf(path,sizeof(path),"%s/sce_sys",test_root);assert(!mkdir(path,0700));
    snprintf(path,sizeof(path),"%s/sce_sys/trophy",test_root);assert(!mkdir(path,0700));
    TrophyCommunicationId id=communication();unsigned char signature=1;int32_t context=777,handle;
    assert(!sceSysmoduleLoadModule(SCE_SYSMODULE_NP_TROPHY));
    assert(!sceNpTrophyInit(NULL));
    write_fixture(&id);
    assert(!sceNpTrophyCreateContext(&context,&id,&signature,0) && context>0);
    TrophyContext *record=find_context(context);assert(record && record->metadata.trophy_count==3);
    for(unsigned i=0;i<3;i++)assert(record->metadata.trophies[i].present);
    assert(record->metadata.trophies[0].grade==1);
    assert(!sceNpTrophyCreateHandle(&handle));
    uint32_t flags[4]={0,0,0,0};uint32_t count=0;
    /* Initial state: zero unlocks, trophy count from the parsed config. */
    assert(sceNpTrophyGetTrophyUnlockState(context,handle,flags,&count)==0 && count==3);
    assert(!flags[0] && !flags[1] && !flags[2] && !flags[3]);
    /* Guards. */
    assert(sceNpTrophyGetTrophyUnlockState(9999,handle,flags,&count)==TROPHY_INVALID_CONTEXT);
    assert(sceNpTrophyUnlockTrophy(9999,handle,1,NULL)==TROPHY_INVALID_CONTEXT);
    assert(sceNpTrophyUnlockTrophy(context,7777,1,NULL)==TROPHY_INVALID_HANDLE);
    assert(sceNpTrophyUnlockTrophy(context,handle,3,NULL)==TROPHY_INVALID_TROPHY_ID);
    assert(sceNpTrophyUnlockTrophy(context,handle,-1,NULL)==TROPHY_INVALID_TROPHY_ID);
    assert(sceNpTrophyGetTrophyUnlockState(context,handle,NULL,&count)==TROPHY_INVALID_ARGUMENT);
    assert(sceNpTrophyGetTrophyUnlockState(context,handle,flags,NULL)==TROPHY_INVALID_ARGUMENT);
    /* Platinum cannot be unlocked directly (rejected before any state or
     * platinum_id write), per the pinned reference. */
    int32_t platinum=123;
    assert(sceNpTrophyUnlockTrophy(context,handle,0,&platinum)==TROPHY_PLATINUM_CANNOT_UNLOCK && platinum==123);
    assert(sceNpTrophyGetTrophyUnlockState(context,handle,flags,&count)==0 && !flags[0] && count==3);
    /* Bronze 1: unlock, observe the bit, reject a duplicate. */
    assert(!sceNpTrophyUnlockTrophy(context,handle,1,&platinum) && platinum==-1);
    assert(!sceNpTrophyGetTrophyUnlockState(context,handle,flags,&count) && count==3);
    assert(flags[0]==0x2u && !flags[1] && !flags[2] && !flags[3]);
    assert(sceNpTrophyUnlockTrophy(context,handle,1,NULL)==TROPHY_ALREADY_UNLOCKED);
    /* Bronze 2 completes the set: the platinum auto-unlocks and reports its id. */
    assert(!sceNpTrophyUnlockTrophy(context,handle,2,&platinum) && platinum==0);
    assert(!sceNpTrophyGetTrophyUnlockState(context,handle,flags,&count) && flags[0]==0x7u);
    /* Re-requesting the platinum stays rejected (guard precedes state). */
    assert(sceNpTrophyUnlockTrophy(context,handle,0,NULL)==TROPHY_PLATINUM_CANNOT_UNLOCK);
    /* Aborted handles fail without touching state. */
    assert(!sceNpTrophyAbortHandle(handle));
    assert(sceNpTrophyUnlockTrophy(context,handle,1,NULL)==TROPHY_ABORT);
    assert(sceNpTrophyGetTrophyUnlockState(context,handle,flags,&count)==TROPHY_ABORT);
    assert(!sceNpTrophyGetTrophyUnlockState(context,handle,flags,&count) || flags[0]==0x7u);
    /* Term clears the session state. */
    assert(!sceNpTrophyTerm());
    assert(sceNpTrophyGetTrophyUnlockState(context,handle,flags,&count)==TROPHY_NOT_INITIALIZED);
    assert(!sceSysmoduleUnloadModule(SCE_SYSMODULE_NP_TROPHY));
    SceNpTrophy_finish();SceSysmodule_finish();
    assert(module_get_func_export(0x4332c10d,0xb397aa24));
    assert(module_get_func_export(0x4332c10d,0xc8d2a4de));
    puts("Trophy unlock: bit-array state, platinum auto-unlock, duplicate/id/abort guards and term clearing passed.");
}
