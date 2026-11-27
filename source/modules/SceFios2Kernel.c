#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <psp2/fios2kernel.h>
#include <psp2/kernel/error.h>
#include <psp2/kernel/threadmgr/thread.h>
#include "module.h"

/* Explicit guest addresses keep the syscall packet independent of host pointer
 * width. The bundled wrapper initializes the first four words only. */
typedef struct {
    uint32_t ids, max_ids, count, buffer_bytes;
} FiosListPacket;
_Static_assert(sizeof(FiosListPacket)==16,"FIOS GetList initialized packet prefix");
#ifdef __arm__
_Static_assert(sizeof(SceFiosGetListSyscallArgs)==24,"FIOS GetList syscall ABI");
_Static_assert(offsetof(SceFiosGetListSyscallArgs,data_0x04)==4,"FIOS GetList capacity offset");
_Static_assert(offsetof(SceFiosGetListSyscallArgs,data_0x08)==8,"FIOS GetList count pointer offset");
_Static_assert(offsetof(SceFiosGetListSyscallArgs,data_0x0C)==12,"FIOS GetList copy size offset");
#endif

EXPORT(SceFios2Kernel,0x9379E2D5,int,_sceFiosKernelOverlayGetList,
       SceUID pid,SceUInt8 min_order,SceUInt8 max_order,SceFiosGetListSyscallArgs *args)
{
    if (!args) return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    if (min_order>max_order) return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    SceUID current=sceKernelGetProcessId();
    if (current<=0 || (pid!=0 && pid!=current)) return SCE_KERNEL_ERROR_UNKNOWN_UID;
    FiosListPacket packet;
    memcpy(&packet,args,sizeof(packet));
    if (!packet.count || (packet.max_ids && !packet.ids)) return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    if ((packet.count & 3) || (packet.max_ids && (packet.ids & 3)))
        return SCE_KERNEL_ERROR_ILLEGAL_ALIGNMENT;
    uint32_t capacity=packet.max_ids>128 ? 128 : packet.max_ids;
    if (packet.buffer_bytes<capacity*4) return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
    /* No Vita overlays are installed by this runtime yet. Enumeration supports
     * that initial empty state; installation and resolution remain unsupported.
     * Capture the packet before writing, since output may alias input storage. */
    uint32_t count=0;
    memcpy((void *)(uintptr_t)packet.count,&count,sizeof(count));
    return 0;
}

EXPORT(SceFios2Kernel,0xB02E0B26,int,_sceFiosKernelOverlayGetRecommendedScheduler,
       int scheduler_count,const char *path,SceUInt64 *args)
{
    /* The ABI requires an eight-byte input block, but its contents are unused.
     * This API selects an index; it does not initialize or run a scheduler. */
    if (!args) return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    if (scheduler_count>1 && path && !strncmp(path,"host",4) &&
        path[4]>='0' && path[4]<='9' && path[5]==':') return 1;
    return 0;
}

DECLARE_LIBRARY(SceFios2Kernel,0x8757B742);
