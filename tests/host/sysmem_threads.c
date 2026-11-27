#define TEST_THREADED_RWLOCK
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>
#include <switch.h>
static void query_unlock(RwLock *lock);
#define rwlockReadUnlock query_unlock
#include "../../source/modules/SceSysmem.c"
#undef rwlockReadUnlock

static atomic_uint live;
void log_printf(const char *format, ...) { (void)format; }
DkMemBlock dk_map_memblock(DkDevice device,void *addr,uint32_t size,uint32_t flags)
{
    (void)device;(void)addr;(void)size;(void)flags;
    void *block=malloc(1);assert(block);atomic_fetch_add(&live,1);return block;
}
void dkMemBlockDestroy(DkMemBlock block)
{ assert(atomic_fetch_sub(&live,1)>0);free(block); }
static pthread_mutex_t gate=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed=PTHREAD_COND_INITIALIZER;
static int unlocked,released;
static _Thread_local bool pause_query;
static void query_unlock(RwLock *lock)
{
    rwlockReadUnlock(lock);
    if (!pause_query) return;
    pause_query=false;
    assert(!pthread_mutex_lock(&gate));unlocked=1;
    assert(!pthread_cond_broadcast(&changed));
    while (!released) assert(!pthread_cond_wait(&changed,&gate));
    assert(!pthread_mutex_unlock(&gate));
}
typedef struct { int kind,result;SceUID uid;void *base;uintptr_t value; } Query;
static void *query(void *argument)
{
    Query *q=argument;pause_query=true;
    if (q->kind==0) {
        void *base=NULL;q->result=sceKernelGetMemBlockBase(q->uid,&base);q->value=(uintptr_t)base;
    } else if (q->kind==1) q->result=sceKernelFindMemBlockByAddr(q->base,4096);
    else q->value=(uintptr_t)SceSysmem_get_dk_memblock_for_addr(q->base);
    return NULL;
}
static void query_free(int kind)
{
    SceUID uid=sceKernelAllocMemBlock("race",SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,4096,NULL);assert(uid>0);
    void *base;assert(!sceKernelGetMemBlockBase(uid,&base));
    uintptr_t mapping=(uintptr_t)SceSysmem_get_dk_memblock_for_addr(base);
    Query q={.kind=kind,.uid=uid,.base=base};
    uintptr_t address=(uintptr_t)base;
    unlocked=released=0;
    pthread_t thread;assert(!pthread_create(&thread,NULL,query,&q));
    assert(!pthread_mutex_lock(&gate));
    while (!unlocked) assert(!pthread_cond_wait(&changed,&gate));
    /* Force reclamation after the query drops its lock but before it returns.
     * The query must already have copied every value it uses from the record. */
    assert(!sceKernelFreeMemBlock(uid) && !atomic_load(&live));
    released=1;assert(!pthread_cond_broadcast(&changed));assert(!pthread_mutex_unlock(&gate));
    assert(!pthread_join(thread,NULL));
    if (kind==0) assert(!q.result && q.value==address);
    else if (kind==1) assert(q.result==uid);
    else assert(q.value==mapping);
}
typedef struct { SceUID uid;int result; } Free;
static pthread_barrier_t start;
static void *free_block(void *argument)
{
    Free *f=argument;pthread_barrier_wait(&start);f->result=sceKernelFreeMemBlock(f->uid);return NULL;
}
int main(void)
{
    alarm(20);assert(!SceSysmem_init(NULL));
    for (int kind=0;kind<3;++kind) query_free(kind);
    assert(!pthread_barrier_init(&start,NULL,3));
    for (unsigned i=0;i<64;++i) {
        SceUID uid=sceKernelAllocMemBlock("free",SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,4096,NULL);assert(uid>0);
        Free a={.uid=uid},b={.uid=uid};pthread_t ta,tb;
        assert(!pthread_create(&ta,NULL,free_block,&a) && !pthread_create(&tb,NULL,free_block,&b));
        pthread_barrier_wait(&start);
        assert(!pthread_join(ta,NULL) && !pthread_join(tb,NULL));
        assert((!a.result && (uint32_t)b.result==SCE_KERNEL_ERROR_INVALID_UID) ||
               (!b.result && (uint32_t)a.result==SCE_KERNEL_ERROR_INVALID_UID));
        assert(!atomic_load(&live));
    }
    assert(!pthread_barrier_destroy(&start));SceSysmem_finish();
    pthread_rwlock_destroy(&g_vita_memblock_infos_lock);
    puts("Sysmem query snapshots and concurrent free ownership checks passed.");
}
