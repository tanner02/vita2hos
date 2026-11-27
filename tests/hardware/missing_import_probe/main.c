#include <psp2/io/fcntl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/error.h>
#include <stdint.h>
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
    int fd=sceIoOpen("app0:/missing-import-probe.txt",SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0600);
    if(fd<0){sceKernelExitProcess(20);return 20;}
    int status=record(fd,"missing_import_probe v1\n");
    uint64_t expected=((uint64_t)0x12345678<<32)|(uint32_t)SCE_KERNEL_ERROR_MODULEMGR_NO_FUNC_NID;
    for(unsigned i=0;i<64;i++) {
        if(call_missing_arm()!=expected){record(fd,"FAIL arm_error_or_r1\n");status=21;break;}
        if(call_missing_thumb()!=expected){record(fd,"FAIL thumb_error_or_r1\n");status=22;break;}
    }
    if(!status)status=record(fd,"PASS repeated_arm_thumb_error_and_r1\nPASS missing_import_probe\n");
    if(sceIoClose(fd)<0)status=23;
    sceKernelExitProcess(status);return status;
}
