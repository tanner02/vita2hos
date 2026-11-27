#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
static char test_app0_path[PATH_MAX];
#define VITA2HOS_APP0_PATH test_app0_path
static int fault,rename_calls;
static int fake_mkdir(const char *path,mode_t mode);
static int fake_rmdir(const char *path);
static int fake_unlink(const char *path);
static int fake_rename(const char *from,const char *to);
#define mkdir fake_mkdir
#define rmdir fake_rmdir
#define unlink fake_unlink
#define rename fake_rename
#include "../../source/modules/SceLibKernel.c"
#undef mkdir
#undef rmdir
#undef unlink
#undef rename
#include "../../source/module.c"
void log_printf(const char *format,...) { (void)format; }
void SceKernelThreadMgr_prepare_current_exit(int status) { (void)status; }
VitaThreadInfo *SceKernelThreadMgr_get_thread_info(void) { return NULL; }
SceUID SceSysmem_get_next_uid(void) { static SceUID uid=100;return uid++; }
static int fake_mkdir(const char *path,mode_t mode)
{ if (fault==1) { errno=ENOSYS;return -1; } return mkdir(path,mode); }
static int fake_rmdir(const char *path)
{ if (fault==2) { errno=EBUSY;return -1; } return rmdir(path); }
static int fake_unlink(const char *path)
{ if (fault==3) { errno=EROFS;return -1; } return unlink(path); }
static int fake_rename(const char *from,const char *to)
{ ++rename_calls;if (fault==4) { errno=EXDEV;return -1; } return rename(from,to); }
int main(void)
{
    assert(!SceLibKernel_init());
    int (*make_dir)(const char *,SceMode)=(void *)module_get_func_export(0xcae9ace6,0x9670D39F);
    int (*remove_dir)(const char *)=(void *)module_get_func_export(0xcae9ace6,0xE9F91EC8);
    int (*remove_file)(const char *)=(void *)module_get_func_export(0xcae9ace6,0xE20ED0F3);
    int (*move_file)(const char *,const char *)=(void *)module_get_func_export(0xcae9ace6,0xF737E369);
    assert(make_dir && remove_dir && remove_file && move_file);
    char directory[]="/tmp/vita2hos-paths-XXXXXX";assert(mkdtemp(directory));
    strcpy(test_app0_path,directory);mode_t old_umask=umask(0);
    assert((uint32_t)make_dir(NULL,0700)==0x8001000e);
    assert((uint32_t)remove_file(NULL)==0x8001000e);
    assert((uint32_t)remove_dir("")==SCE_ERROR_ERRNO_ENODEV);
    assert((uint32_t)make_dir("ux0:/dir",0700)==SCE_ERROR_ERRNO_ENODEV);
    assert((uint32_t)make_dir("app0:/../escape",0700)==0x8001000d);
    assert((uint32_t)make_dir("app0:/absent/child",0700)==0x80010002);
    assert(!make_dir("app0:/sub",0777));
    char native[PATH_MAX];snprintf(native,sizeof(native),"%s/sub",directory);
    struct stat status;assert(!stat(native,&status) && (status.st_mode&0777)==0707);
    assert((uint32_t)make_dir("app0:/sub",0700)==0x80010011);
    SceUID source=sceIoOpen("app0:/sub/source",SCE_O_RDWR|SCE_O_CREAT|SCE_O_EXCL,0600);
    SceUID destination=sceIoOpen("app0:/sub/dest",SCE_O_RDWR|SCE_O_CREAT|SCE_O_EXCL,0600);
    assert(source>0 && destination>0);
    assert(sceIoWrite(source,"new",3)==3 && sceIoWrite(destination,"old",3)==3);
    assert((uint32_t)remove_dir("app0:/sub")==0x8001005a); /* Nonrecursive. */
    assert((uint32_t)remove_dir("app0:/sub/source")==0x80010014);
    assert((uint32_t)remove_file("app0:/sub")==0x80010015);
    int before=rename_calls;
    assert((uint32_t)move_file("app0:/sub/source",NULL)==0x8001000e);
    assert((uint32_t)move_file("app0:/sub/source","ux0:/dest")==SCE_ERROR_ERRNO_ENODEV);
    assert((uint32_t)move_file("app0:/sub/source","app0:/../dest")==0x8001000d);
    char long_path[PATH_MAX+8];memset(long_path,'x',sizeof(long_path));long_path[sizeof(long_path)-1]=0;
    assert((uint32_t)move_file("app0:/sub/source",long_path)==0x8001005b);
    assert(rename_calls==before);
    assert(!move_file("app0:/sub/source","app0:/sub/source"));
    assert(!move_file("app0:/sub/source","app0:/sub/dest"));
    SceIoStat info;assert((uint32_t)sceIoGetstat("app0:/sub/source",&info)==0x80010002);
    SceUID renamed=sceIoOpen("app0:/sub/dest",SCE_O_RDONLY,0);assert(renamed>0);
    char data[4]={0};assert(sceIoRead(renamed,data,3)==3 && !memcmp(data,"new",3));
    assert(!sceIoLseek(destination,0,SCE_SEEK_SET));
    assert(sceIoRead(destination,data,3)==3 && !memcmp(data,"old",3));
    assert(!sceIoClose(renamed));
    fault=4;assert((uint32_t)move_file("app0:/sub/dest","app0:/sub/other")==0x80010012);
    fault=3;assert((uint32_t)remove_file("app0:/sub/dest")==0x8001001e);
    fault=2;assert((uint32_t)remove_dir("app0:/sub")==0x80010010);
    fault=1;assert((uint32_t)make_dir("app0:/other",0700)==0x80010058);
    fault=0;
    assert(!remove_file("app0:/sub/dest"));
    assert(!sceIoLseek(source,0,SCE_SEEK_SET));
    assert(sceIoRead(source,data,3)==3 && !memcmp(data,"new",3));
    assert(!sceIoClose(source) && !sceIoClose(destination));
    assert((uint32_t)remove_file("app0:/sub/dest")==0x80010002);
    assert(!remove_dir("app0:/sub"));
    assert(!rmdir(directory));umask(old_umask);
    assert((uint32_t)io_errno(ELOOP)==0x8001005c);
    assert((uint32_t)io_errno(EPERM)==0x80010001);
    assert(!vita_opened_files_dict_size(g_vita_opened_files));
    vita_opened_files_dict_clear(g_vita_opened_files);vita_opened_dirs_dict_clear(g_vita_opened_dirs);
    puts("Filesystem paths: NID dispatch, nonrecursive removal, native rename/open handles, validation and libc errno mapping passed.");
}
