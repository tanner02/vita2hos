#include <psp2/gxm.h>
#include "module.h"
#include "log.h"

/* User markers are optional profiling annotations. Preserve their ordering in
 * debug logs; no Vita Razor capture or native GPU timestamp is produced.
 * Contexts/tags are borrowed for the call and never retained. */
static int marker(SceGxmContext *context,const char *operation,const char *tag)
{
    if (!context || !tag) return SCE_GXM_ERROR_INVALID_POINTER;
    LOG_DEBUG("GXM_MARKER context=%p operation=%s tag=%.63s",(void *)context,operation,tag);
    return 0;
}
EXPORT(SceGxm,0x3276C475,int,sceGxmPushUserMarker,SceGxmContext *context,const char *tag)
{ return marker(context,"push",tag); }
EXPORT(SceGxm,0xC7A8CB77,int,sceGxmSetUserMarker,SceGxmContext *context,const char *tag)
{ return marker(context,"set",tag); }
EXPORT(SceGxm,0x4FA073A6,int,sceGxmPopUserMarker,SceGxmContext *context)
{
    if (!context) return SCE_GXM_ERROR_INVALID_POINTER;
    LOG_DEBUG("GXM_MARKER context=%p operation=pop",(void *)context);
    return 0;
}
EXPORT(SceGxm,0x3D25FCE9,int,sceGxmPadHeartbeat,const SceGxmColorSurface *surface,
       SceGxmSyncObject *sync)
{
    /* As in the pinned reference, this profiler heartbeat has no display or
     * synchronization side effects. Rendering and presentation own the fences. */
    if (!surface || !sync) return SCE_GXM_ERROR_INVALID_POINTER;
    return 0;
}
