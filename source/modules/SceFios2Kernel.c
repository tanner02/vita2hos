#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <psp2/fios2kernel.h>
#include <psp2/kernel/error.h>
#include <psp2/kernel/threadmgr/thread.h>
#include "module.h"
#include "log.h"

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

/* The supplied wrapper initializes out/max at +0/+4, range bytes at +8/+9,
 * and independent copy capacity at +12. Reserved trailing words are not read. */
typedef struct {
    uint32_t output, max_path;
    uint8_t min_order, max_order, reserved[2];
    uint32_t output_bytes;
} FiosResolvePacket;
_Static_assert(sizeof(FiosResolvePacket)==16,"FIOS resolve initialized prefix");
#ifdef __arm__
_Static_assert(offsetof(SceFiosResolveSyncSyscallArgs,data_0x0C)==12,"FIOS resolve copy capacity");
_Static_assert(offsetof(SceFiosResolveWithRangeSyncSyscallArgs,data_0x08)==8,"FIOS resolve order offset");
#endif
static int resolve_empty_overlays(SceUID pid,int write,const char *input,const void *args,bool ranged)
{
    if (!args || !input) return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    FiosResolvePacket packet;
    memcpy(&packet,args,sizeof(packet));
    if (write!=0 && write!=1) return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    if (ranged && packet.min_order>packet.max_order) return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
    SceUID current=sceKernelGetProcessId();
    if (current<=0 || (pid && pid!=current)) return SCE_KERNEL_ERROR_UNKNOWN_UID;
    if (!packet.output) return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    /* Bound CPU scanning independently of a corrupted guest capacity. Larger
     * paths remain unsupported; no truncation is reported as successful. */
    if (!packet.max_path || packet.max_path>4096) return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
    size_t length=strnlen(input,packet.max_path);
    if (length==packet.max_path || length+1>packet.output_bytes)
        return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
    if (length>UINT32_MAX-packet.output) return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    /* The runtime has no installed overlays. Preserve Vita device/relative
     * syntax here: app0 host mounting is the I/O resolver's separate job. */
    memmove((void *)(uintptr_t)packet.output,input,length+1);
    return 0;
}
EXPORT(SceFios2Kernel,0xE9AE60FB,int,_sceFiosKernelOverlayResolveSync,
       SceUID pid,int write,const char *input,SceFiosResolveSyncSyscallArgs *args)
{
    return resolve_empty_overlays(pid,write,input,args,false);
}
EXPORT(SceFios2Kernel,0x8CCA471A,int,_sceFiosKernelOverlayResolveWithRangeSync,
       SceUID pid,int write,const char *input,SceFiosResolveWithRangeSyncSyscallArgs *args)
{
    return resolve_empty_overlays(pid,write,input,args,true);
}


/* ZCode batch additions: kernel overlay management. Overlays are accepted
 * (no-op) and data-handler operations report ENOENT since no overlay
 * backends are registered. */

EXPORT(SceFios2Kernel,0x6DBCF0B2,int,_sceFiosKernelOverlayAdd,const SceFiosKernelOverlay *overlay,SceFiosKernelOverlayID *out_id)
{
    LOG("sceFiosKernelOverlayAdd overlay=%p",overlay);
    return 0;
}

EXPORT(SceFios2Kernel,0x2A381357,int,_sceFiosKernelOverlayAddForProcess,SceUID target_process,const SceFiosKernelOverlay *overlay,SceFiosKernelOverlayID *out_id)
{
    LOG("sceFiosKernelOverlayAddForProcess pid=%d",target_process);
    return 0;
}

EXPORT(SceFios2Kernel,0xB4927173,int,_sceFiosKernelOverlayRemove,SceFiosKernelOverlayID id)
{
    LOG("sceFiosKernelOverlayRemove id=%u",id);
    return 0;
}

EXPORT(SceFios2Kernel,0xF8277E07,int,_sceFiosKernelOverlayRemoveForProcess,SceUID target_process,SceFiosKernelOverlayID id)
{
    LOG("sceFiosKernelOverlayRemoveForProcess pid=%d",target_process);
    return 0;
}

EXPORT(SceFios2Kernel,0x6D6CDE05,int,_sceFiosKernelOverlayModify,SceFiosKernelOverlayID id,const SceFiosKernelOverlay *new_value)
{
    LOG("sceFiosKernelOverlayModify id=%u",id);
    return 0;
}

EXPORT(SceFios2Kernel,0x6DF2FC05,int,_sceFiosKernelOverlayModifyForProcess,SceUID target_process,SceFiosKernelOverlayID id,const SceFiosKernelOverlay *new_value)
{
    LOG("sceFiosKernelOverlayModifyForProcess pid=%d",target_process);
    return 0;
}

EXPORT(SceFios2Kernel,0xF44F3505,int,_sceFiosKernelOverlayGetInfo,SceFiosKernelOverlayID id,SceFiosKernelOverlay *out_overlay)
{
    return (int)SCE_KERNEL_ERROR_UNSUP;
}

EXPORT(SceFios2Kernel,0xBC6B3CC5,int,_sceFiosKernelOverlayGetInfoForProcess,SceUID target_process,SceFiosKernelOverlayID id,SceFiosKernelOverlay *out_overlay)
{
    return (int)SCE_KERNEL_ERROR_UNSUP;
}

EXPORT(SceFios2Kernel,0x3E9172EA,int,_sceFiosKernelOverlayThreadSetDisabled,SceInt32 disabled)
{
    LOG("sceFiosKernelOverlayThreadSetDisabled disabled=%d",disabled);
    return 0;
}

EXPORT(SceFios2Kernel,0x629F4FE4,int,_sceFiosKernelOverlayThreadIsDisabled,void)
{
    /* Overlay thread disabled by default: all overlay DH work is refused. */
    return 1;
}

EXPORT(SceFios2Kernel,0x5D6A1CCE,int,_sceFiosKernelOverlayDHOpenSync,SceFiosKernelOverlayDH *out_dh,const char *path,SceUInt8 from_order,SceFiosDHOpenSyncSyscallArgs *args)
{
    return (int)0x80010002;
}

EXPORT(SceFios2Kernel,0x2F06ADC6,int,_sceFiosKernelOverlayDHReadSync,SceFiosKernelOverlayDH dh,SceFiosNativeDirEntry *out_entry)
{
    return (int)0x80010002;
}

EXPORT(SceFios2Kernel,0x2A9724C9,int,_sceFiosKernelOverlayDHSyncSync,SceFiosKernelOverlayDH dh,int flag)
{
    return 0;
}

EXPORT(SceFios2Kernel,0x021B4AF7,int,_sceFiosKernelOverlayDHCloseSync,SceFiosKernelOverlayDH dh)
{
    return 0;
}

EXPORT(SceFios2Kernel,0x759EBEE6,int,_sceFiosKernelOverlayDHStatSync,SceFiosKernelOverlayDH dh,SceFiosNativeStat *out_stat)
{
    return (int)0x80010002;
}

EXPORT(SceFios2Kernel,0xF6A3E335,int,_sceFiosKernelOverlayDHChstatSync,SceFiosKernelOverlayDH dh,const SceFiosNativeStat *new_stat,unsigned int cbit)
{
    return (int)0x80010002;
}

DECLARE_LIBRARY(SceFios2Kernel,0x8757B742);
