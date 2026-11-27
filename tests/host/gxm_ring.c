#include <assert.h>
#include <stdio.h>
#include "gxm/ring.h"
int main(void)
{
    uint32_t offset=123, head=0;
    assert(gxm_ring_span(0x10000,0x4000,0x11000,0x2000,&offset) && offset==0x1000);
    assert(gxm_ring_span(0x10000,0x4000,0x12000,0x2000,&offset) && offset==0x2000);
    assert(!gxm_ring_span(0x10000,0x4000,0xffff,1,&offset) && offset==0x2000);
    assert(!gxm_ring_span(0x10000,0x4000,0x14000,1,&offset) && offset==0x2000);
    assert(!gxm_ring_span(0x10000,0x4000,UINTPTR_MAX,1,&offset));
    assert(!gxm_ring_span(0x10000,0x4000,0x11000,UINT32_MAX,&offset));
    assert(gxm_ring_reserve(64,48,&head,&offset) && offset==0 && head==48);
    assert(gxm_ring_reserve(64,16,&head,&offset) && offset==48 && head==64);
    assert(gxm_ring_reserve(64,32,&head,&offset) && offset==0 && head==32);
    assert(!gxm_ring_reserve(64,65,&head,&offset) && offset==0 && head==32);
    head=UINT32_MAX;
    assert(!gxm_ring_reserve(64,1,&head,&offset) && head==UINT32_MAX);
    head=UINT32_MAX-1;
    assert(gxm_ring_reserve(UINT32_MAX,2,&head,&offset) && head==2 && offset==0);
    puts("GXM ring spans: interior pointers, exact ends, bounds, wrap and overflow passed.");
}
