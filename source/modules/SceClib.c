#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/error.h>
#include "module.h"

/* These pointer/size interfaces share the ARM32 C ABI. The SceLibKernel
 * descriptor is declared in SceLibKernel.c; no guest libc allocator is mixed in. */
EXPORT_FUNC(SceLibKernel,0x9CC2BFDF,memcmp)
EXPORT_FUNC(SceLibKernel,0x660D1F6D,strncmp)
EXPORT_FUNC(SceLibKernel,0xC458D60A,strncpy)
EXPORT_FUNC(SceLibKernel,0x6E728AAE,strrchr)

static unsigned int ascii_lower(unsigned char c)
{ return c>='A' && c<='Z' ? c+('a'-'A') : c; }

EXPORT(SceLibKernel,0xB54C0BE4,int,sceClibStrncasecmp,const char *a,const char *b,SceSize size)
{
    for (SceSize i=0;i<size;++i) {
        unsigned int left=ascii_lower((unsigned char)a[i]),right=ascii_lower((unsigned char)b[i]);
        if (left!=right) return (int)left-(int)right;
        if (!left) break;
    }
    return 0;
}

#if UINTPTR_MAX==UINT32_MAX
_Static_assert(sizeof(va_list)==4 && sizeof(long)==4 && sizeof(double)==8,"ARM32 Clib argument sizes");
#endif

EXPORT(SceLibKernel,0xFA6BE467,int,sceClibVsnprintf,char *dst,SceSize capacity,
       const char *format,va_list args)
{
    if (!format || (capacity && !dst)) return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
    int result=vsnprintf(dst,capacity,format,args);
    /* Match would-be length/empty-output behavior. Do not copy the older
     * pinned emulator's generic success result or empty-string error. */
    return result<0 ? (int)SCE_KERNEL_ERROR_INVALID_ARGUMENT : result;
}

EXPORT(SceLibKernel,0x8CBA03D5,int,sceClibSnprintf,char *dst,SceSize capacity,
       const char *format,...)
{
    va_list args;va_start(args,format);
    int result=sceClibVsnprintf(dst,capacity,format,args);
    va_end(args);return result;
}
