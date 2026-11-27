#define TEST_THREADED_MUTEX
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

static int fail_allocation,fail_at,allocation_calls;
static void *trophy_calloc(size_t count,size_t size);
#define calloc trophy_calloc
#include "../../source/modules/SceNpTrophy.c"
#undef calloc
#include "../../source/modules/SceSysmodule.c"
#include "../../source/module.c"
static void *trophy_calloc(size_t count,size_t size){allocation_calls++;if(fail_allocation || (fail_at && fail_at==allocation_calls))return NULL;return calloc(count,size);}
SceUID SceSysmem_get_next_uid(void){static atomic_int next=100;return atomic_fetch_add(&next,1);}
bool SceNet_host_ready(void){return false;}
bool SceNet_is_initialized(void){return false;}
void log_printf(const char *format,...){(void)format;}
static void *worker(void *arg)
{
    (void)arg;
    for(unsigned i=0;i<200;i++) {
        int32_t handle=-1;
        assert(!sceNpTrophyCreateHandle(&handle) && handle>0);
        assert(!sceNpTrophyAbortHandle(handle));assert(!sceNpTrophyDestroyHandle(handle));
    }
    return NULL;
}
int main(void)
{
    int32_t handle=123;
    assert(sceNpTrophyTerm()==TROPHY_NOT_INITIALIZED);
    assert(sceNpTrophyCreateHandle(&handle)==TROPHY_NOT_INITIALIZED && handle==123);
    assert(sceNpTrophyInit(&handle)==TROPHY_INVALID_ARGUMENT && !SceNpTrophy_is_initialized());
    fail_allocation=1;assert(sceNpTrophyInit(NULL)==TROPHY_NO_MEMORY && !SceNpTrophy_is_initialized());
    fail_allocation=0;fail_at=2;allocation_calls=0;
    assert(sceNpTrophyInit(NULL)==TROPHY_NO_MEMORY && !trophy_handles && !trophy_contexts);
    fail_at=0;assert(!sceSysmoduleLoadModule(SCE_SYSMODULE_NP_TROPHY));
    assert(!sceSysmoduleIsLoaded(SCE_SYSMODULE_NP_TROPHY));
    assert(!sceNpTrophyInit(NULL) && SceNpTrophy_is_initialized());
    assert(sceNpTrophyInit(NULL)==TROPHY_ALREADY_INITIALIZED);
    assert(sceSysmoduleUnloadModule(SCE_SYSMODULE_NP_TROPHY)==(int)SCE_SYSMODULE_ERROR_FATAL);
    assert(sceNpTrophyCreateHandle(NULL)==TROPHY_INVALID_ARGUMENT);
    int32_t handles[TROPHY_HANDLE_LIMIT];
    for(unsigned i=0;i<TROPHY_HANDLE_LIMIT;i++) {
        assert(!sceNpTrophyCreateHandle(&handles[i]));
        for(unsigned j=0;j<i;j++)assert(handles[i]!=handles[j]);
    }
    assert(sceNpTrophyCreateHandle(&handle)==TROPHY_EXCEEDS_MAX && handle==123);
    assert(!sceNpTrophyAbortHandle(handles[0]) && find_handle(handles[0])->aborted);
    assert(!sceNpTrophyAbortHandle(handles[0]));
    assert(!sceNpTrophyDestroyHandle(handles[0]));
    assert(sceNpTrophyDestroyHandle(handles[0])==TROPHY_INVALID_HANDLE);
    assert(!sceNpTrophyCreateHandle(&handle) && handle!=handles[0] && !find_handle(handle)->aborted);
    assert(sceNpTrophyAbortHandle(handles[0])==TROPHY_INVALID_HANDLE);
    assert(!sceNpTrophyTerm() && !SceNpTrophy_is_initialized());
    assert(!sceSysmoduleUnloadModule(SCE_SYSMODULE_NP_TROPHY));
    assert(!sceNpTrophyInit(NULL));
    assert(sceNpTrophyDestroyHandle(handle)==TROPHY_INVALID_HANDLE);
    pthread_t threads[8];
    for(unsigned i=0;i<8;i++)assert(!pthread_create(&threads[i],NULL,worker,NULL));
    for(unsigned i=0;i<8;i++)assert(!pthread_join(threads[i],NULL));
    SceNpTrophy_finish();SceNpTrophy_finish();SceSysmodule_finish();
    assert(!SceNpTrophy_is_initialized());
    assert(module_get_func_export(0x4332c10d,0x34516838));
    assert(module_get_func_export(0x4332c10d,0x4ebc6977));
    puts("Trophy lifecycle: failure preservation, bounded owned handles, abort/reuse, concurrent ownership and provider state passed.");
}
