#define TEST_THREADED_MUTEX
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../source/modules/SceMspace.c"
#include "../../source/module.c"
DECLARE_LIBRARY(SceLibKernel,0xcae9ace6);
static void *worker(void *arg)
{
    SceClibMspace heap=arg;
    for(unsigned i=0;i<1000;i++) {
        size_t size=16+(i%127);unsigned char *p=sceClibMspaceMalloc(heap,size);assert(p);
        memset(p,i,size);for(size_t j=0;j<size;j++)assert(p[j]==(unsigned char)i);
        sceClibMspaceFree(heap,p);
    }
    return NULL;
}
int main(void)
{
    mutexInit(&space_lock);
    _Alignas(64) unsigned char storage[65536+128],second[16384];
    memset(storage,0x5a,sizeof(storage));void *base=storage+64;SceSize size=65536;
    assert(!sceClibMspaceCreate(NULL,size));assert(!sceClibMspaceCreate(base,0));
    assert(!sceClibMspaceCreate((char *)base+1,size));
    assert(!sceClibMspaceCreate(base,32));
    SceClibMspace heap=sceClibMspaceCreate(base,size);assert(heap);
    assert((uintptr_t)heap>=(uintptr_t)base && (uintptr_t)heap<(uintptr_t)base+size);
    assert(!sceClibMspaceCreate(base,size));assert(!sceClibMspaceCreate(storage+128,4096));
    assert(!sceClibMspaceMalloc(heap,1024*1024));assert(!sceClibMspaceMalloc((void *)1,64));
    void *p=sceClibMspaceMalloc(heap,128);assert(p && !((uintptr_t)p&7));
    assert((uintptr_t)p>=(uintptr_t)base && (uintptr_t)p+128<=(uintptr_t)base+size);
    memset(p,0xab,128);SceClibMspace other=sceClibMspaceCreate(second,sizeof(second));assert(other);
    sceClibMspaceFree(other,p);sceClibMspaceFree(heap,(char *)p+8);
    for(int i=0;i<128;i++)assert(((unsigned char *)p)[i]==0xab);
    sceClibMspaceFree(heap,p);sceClibMspaceFree(heap,p);sceClibMspaceFree(heap,NULL);
    pthread_t threads[4];for(int i=0;i<4;i++)assert(!pthread_create(&threads[i],NULL,worker,heap));
    for(int i=0;i<4;i++)assert(!pthread_join(threads[i],NULL));
    void *large=sceClibMspaceMalloc(heap,48000);assert(large);
    sceClibMspaceDestroy(heap);assert(!sceClibMspaceMalloc(heap,16));sceClibMspaceDestroy(heap);
    for(unsigned i=0;i<64;i++)assert(storage[i]==0x5a && storage[65536+64+i]==0x5a);
    heap=sceClibMspaceCreate(base,size);assert(heap);sceClibMspaceDestroy(heap);sceClibMspaceDestroy(other);
    const uint32_t nids[]={0x3B9E301A,0x86EF7680,0x9C56B4D1,0xAE1A21EC};
    for(unsigned i=0;i<4;i++)assert(module_get_func_export(0xcae9ace6,nids[i]));
    assert(!spaces);pthread_mutex_destroy(&space_lock);
    puts("Mspaces: caller pool bounds, exhaustion, coalescing, wrong-heap frees, concurrent allocation and destruction passed.");
}
