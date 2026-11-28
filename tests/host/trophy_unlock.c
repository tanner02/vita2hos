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
#include "savedata_mount.h"
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
static char mount_root[512];
static int test_resolver(const char *path,char *output,size_t capacity)
{
    (void)path;
    if(!mount_root[0])return -34; /* -ENODEV until the test installs the mount */
    size_t length=strlen(mount_root);
    if(length+1>=capacity)return -36;
    memcpy(output,mount_root,length+1);
    return 0;
}
int main(void)
{
    char temporary[]="/tmp/vita2hos-trophy-unlock-XXXXXX";assert(mkdtemp(temporary));
    strcpy(test_root,temporary);char path[1024];snprintf(path,sizeof(path),"%s/sce_sys",test_root);assert(!mkdir(path,0700));
    snprintf(path,sizeof(path),"%s/sce_sys/trophy",test_root);assert(!mkdir(path,0700));
    snprintf(mount_root,sizeof(mount_root),"%s/sce_persist",test_root);assert(!mkdir(mount_root,0700));
    TrophyCommunicationId id=communication();unsigned char signature=1;int32_t context=777,handle;
    uint32_t flags[4]={0,0,0,0};uint32_t count=0;

    /* Without the mount installed the state is session-only: unlocks succeed
     * but nothing is written or restored. */
    assert(!sceSysmoduleLoadModule(SCE_SYSMODULE_NP_TROPHY));
    assert(!sceNpTrophyInit(NULL));
    write_fixture(&id);
    assert(!sceNpTrophyCreateContext(&context,&id,&signature,0) && context>0);
    assert(!sceNpTrophyCreateHandle(&handle));
    int32_t platinum=123;
    assert(sceNpTrophyUnlockTrophy(context,handle,0,&platinum)==TROPHY_PLATINUM_CANNOT_UNLOCK && platinum==123);
    assert(!sceNpTrophyUnlockTrophy(context,handle,1,&platinum) && platinum==-1);
    assert(!sceNpTrophyGetTrophyUnlockState(context,handle,flags,&count) && count==3 && flags[0]==0x2u);
    assert(sceNpTrophyUnlockTrophy(context,handle,1,NULL)==TROPHY_ALREADY_UNLOCKED);
    assert(!sceNpTrophyTerm() && !SceNpTrophy_is_initialized());
    SceNpTrophy_finish();SceSysmodule_finish();

    /* With the mount installed the state persists across Term/re-init and
     * re-created contexts. */
    savedata_mount_set(test_resolver);
    assert(!sceSysmoduleLoadModule(SCE_SYSMODULE_NP_TROPHY));
    assert(!sceNpTrophyInit(NULL));
    assert(!sceNpTrophyCreateContext(&context,&id,&signature,0) && context>0);
    TrophyContext *record=find_context(context);assert(record);
    /* The session-only phase wrote nothing: the restored context starts empty. */
    assert(record->unlocked_count==0);
    assert(!sceNpTrophyCreateHandle(&handle));
    assert(!sceNpTrophyUnlockTrophy(context,handle,1,&platinum) && platinum==-1);
    assert(!sceNpTrophyUnlockTrophy(context,handle,2,&platinum) && platinum==0);
    /* Bronze 2 completed the set: platinum auto-unlocked and reported. */
    assert(!sceNpTrophyGetTrophyUnlockState(context,handle,flags,&count) && count==3 && flags[0]==0x7u);
    /* Re-requesting the platinum stays rejected (guard precedes state). */
    assert(sceNpTrophyUnlockTrophy(context,handle,0,NULL)==TROPHY_PLATINUM_CANNOT_UNLOCK);
    /* The state file exists on the mounted root with the unlock bits. */
    char state_path[1120];
    assert(snprintf(state_path,sizeof(state_path),"%s/trophy_state.bin",mount_root)>0);
    FILE *check=fopen(state_path,"rb");assert(check);
    TrophyStateFile stored;
    size_t got=fread(&stored,1,sizeof(stored),check);
    assert(!fclose(check) && got==sizeof(stored));
    assert(stored.magic==TROPHY_STATE_MAGIC && stored.version==TROPHY_STATE_VERSION);
    assert(!memcmp(stored.communication,"NPWR00042",9) && stored.trophy_count==3);
    assert(stored.unlocked_count==3 && stored.flags[0]==0x7u);
    /* Term + full re-create restores the bits from the file. */
    assert(!sceNpTrophyDestroyHandle(handle));
    assert(!sceNpTrophyTerm() && !SceNpTrophy_is_initialized());
    assert(!sceNpTrophyInit(NULL));
    assert(!sceNpTrophyCreateContext(&context,&id,&signature,0) && context>0);
    record=find_context(context);assert(record && record->unlocked_count==3);
    assert((record->unlocked[0]&0x7u)==0x7u);
    assert(!sceNpTrophyCreateHandle(&handle));
    assert(!sceNpTrophyGetTrophyUnlockState(context,handle,flags,&count) && count==3 && flags[0]==0x7u);
    /* Duplicate/invalid/abort guards on the restored state. */
    assert(sceNpTrophyUnlockTrophy(context,handle,1,NULL)==TROPHY_ALREADY_UNLOCKED);
    assert(sceNpTrophyUnlockTrophy(context,handle,3,NULL)==TROPHY_INVALID_TROPHY_ID);
    assert(sceNpTrophyUnlockTrophy(context,handle,0,NULL)==TROPHY_PLATINUM_CANNOT_UNLOCK);
    assert(!sceNpTrophyAbortHandle(handle));
    assert(sceNpTrophyUnlockTrophy(context,handle,1,NULL)==TROPHY_ABORT);
    assert(sceNpTrophyGetTrophyUnlockState(context,handle,flags,&count)==TROPHY_ABORT);
    assert(!sceNpTrophyDestroyHandle(handle));
    assert(!sceNpTrophyTerm() && !SceNpTrophy_is_initialized());
    /* Uninstalling the mount leaves stale files unread: fresh state again. */
    savedata_mount_set(NULL);
    assert(!sceSysmoduleUnloadModule(SCE_SYSMODULE_NP_TROPHY));
    SceNpTrophy_finish();SceSysmodule_finish();
    assert(module_get_func_export(0x4332c10d,0xb397aa24));
    assert(module_get_func_export(0x4332c10d,0xc8d2a4de));
    puts("Trophy unlock: bit-array state, platinum rejection/auto-unlock, persistence round-trip and guards passed.");
}
