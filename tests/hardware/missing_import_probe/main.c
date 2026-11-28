#include <psp2/io/fcntl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/error.h>
#include <stdint.h>
#ifdef MEDIA_PROBE
#include <psp2/sysmodule.h>
extern uint32_t owned_mp4_open(uint32_t,uint32_t,uint32_t,uint32_t);
#define PROBE_REPORT "app0:/missing-import-media-probe-v3.txt"
#else
#define PROBE_REPORT "app0:/missing-import-probe.txt"
#endif
extern uint64_t call_missing_arm(void);
extern uint64_t call_missing_thumb(void);
static int record(int fd,const char *text)
{
    unsigned size=0;while(text[size])++size;
    return sceIoWrite(fd,text,size)==(int)size ? 0 : 1;
}
int module_start(unsigned args,void *argp)
{
    (void)args;(void)argp;
    int fd=sceIoOpen(PROBE_REPORT,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0600);
    if(fd<0){sceKernelExitProcess(20);return 20;}
#ifdef MEDIA_PROBE
    int status=record(fd,"missing_import_media_probe v3\n");
    int flag=sceIoOpen("app0:/media-probe.enabled",SCE_O_RDONLY,0);
    int enabled=flag>=0;
    if(enabled && sceIoClose(flag)<0)status=24;
    int result=sceSysmoduleLoadModule(SCE_SYSMODULE_MP4);
    int expected_load=enabled ? 0 : (int)SCE_SYSMODULE_ERROR_FATAL;
    if(result!=expected_load){record(fd,"FAIL module_gate\n");status=25;}
    else status|=record(fd,"PASS module_gate\n");
    int expected_state=enabled ? 0 : (int)SCE_SYSMODULE_ERROR_UNLOADED;
    if(sceSysmoduleIsLoaded(SCE_SYSMODULE_MP4)!=expected_state){record(fd,"FAIL module_state\n");status=26;}
    else status|=record(fd,"PASS module_state\n");
    if(sceSysmoduleUnloadModule(SCE_SYSMODULE_MP4)!=expected_state){record(fd,"FAIL module_unload\n");status=27;}
    else status|=record(fd,"PASS module_unload\n");
    if(sceSysmoduleIsLoaded(SCE_SYSMODULE_MP4)!=(int)SCE_SYSMODULE_ERROR_UNLOADED){record(fd,"FAIL final_unloaded\n");status=28;}
    else status|=record(fd,"PASS final_unloaded\n");
    static uint32_t open_guard[4]={0x11223344,0x55667788,0xaabbccdd,0x55aa55aa};
    if(owned_mp4_open((uint32_t)open_guard,0,0xffffffff,0x8002d00b)!=0 ||
        open_guard[0]!=0x11223344 || open_guard[1]!=0x55667788 ||
        open_guard[2]!=0xaabbccdd || open_guard[3]!=0x55aa55aa) {
        record(fd,"FAIL null_open_handle_and_guards\n");status=29;
    } else status|=record(fd,"PASS null_open_handle_and_guards\n");
#else
    int status=record(fd,"missing_import_probe v1\n");
#endif
    uint64_t expected=((uint64_t)0x12345678<<32)|(uint32_t)SCE_KERNEL_ERROR_MODULEMGR_NO_FUNC_NID;
    for(unsigned i=0;i<64;i++) {
        if(call_missing_arm()!=expected){record(fd,"FAIL arm_error_or_r1\n");status=21;break;}
        if(call_missing_thumb()!=expected){record(fd,"FAIL thumb_error_or_r1\n");status=22;break;}
    }
#ifdef MEDIA_PROBE
    if(!status)status=record(fd,"PASS repeated_arm_thumb_error_and_r1\nPASS missing_import_media_probe\n");
#else
    if(!status)status=record(fd,"PASS repeated_arm_thumb_error_and_r1\nPASS missing_import_probe\n");
#endif
    if(sceIoClose(fd)<0)status=23;
    sceKernelExitProcess(status);return status;
}
