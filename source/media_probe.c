#include <stdarg.h>
#include <stdio.h>
#include <switch.h>
#include "config.h"
#include "media_probe.h"
#ifndef MEDIA_PROBE_FLAG
#define MEDIA_PROBE_FLAG VITA2HOS_ROOT_PATH "/media-probe.enabled"
#endif
#ifndef MEDIA_PROBE_REPORT
#define MEDIA_PROBE_REPORT VITA2HOS_ROOT_PATH "/media-probe.txt"
#endif
#define MEDIA_PROBE_LIMIT 256U
static Mutex media_probe_lock;
static bool checked,enabled;
static unsigned records;
bool media_probe_active(void)
{
    mutexLock(&media_probe_lock);
    if(!checked) {
        FILE *file=fopen(MEDIA_PROBE_FLAG,"rb");
        enabled=file!=NULL;if(file)fclose(file);
        checked=true;
    }
    bool result=enabled;
    mutexUnlock(&media_probe_lock);return result;
}
void media_probe_record(const char *format,...)
{
    if(!media_probe_active())return;
    mutexLock(&media_probe_lock);
    if(records<MEDIA_PROBE_LIMIT) {
        ++records;
        char line[512];va_list args;va_start(args,format);
        int length=vsnprintf(line,sizeof(line),format,args);va_end(args);
        if(length>0 && (unsigned)length<sizeof(line)) {
            FILE *file=fopen(MEDIA_PROBE_REPORT,"ab");
            if(file) {
                fwrite(line,1,(size_t)length,file);fwrite("\n",1,1,file);
                fclose(file);
            }
        }
    }
    mutexUnlock(&media_probe_lock);
}
void media_probe_finish(void)
{
    mutexLock(&media_probe_lock);checked=enabled=false;records=0;mutexUnlock(&media_probe_lock);
}
