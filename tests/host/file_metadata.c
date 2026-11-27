#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
static char test_app0_path[PATH_MAX];
#define VITA2HOS_APP0_PATH test_app0_path
static int fail_operation;
static int test_chmod(const char *path,mode_t mode);
static int test_truncate(const char *path,off_t size);
static int test_utimes(const char *path,const struct timeval times[2]);
#define chmod test_chmod
#define truncate test_truncate
#define utimes test_utimes
#include "../../source/modules/SceLibKernel.c"
#undef chmod
#undef truncate
#undef utimes
#include "../../source/module.c"
void log_printf(const char *format,...) { (void)format; }
void SceKernelThreadMgr_prepare_current_exit(int status) { (void)status; }
VitaThreadInfo *SceKernelThreadMgr_get_thread_info(void) { return NULL; }
SceUID SceSysmem_get_next_uid(void) { static SceUID uid=1;return uid++; }
static int test_chmod(const char *path,mode_t mode)
{ if (fail_operation==1) { errno=ENOSYS;return -1; } return chmod(path,mode); }
static int test_truncate(const char *path,off_t size)
{ if (fail_operation==2) { errno=EIO;return -1; } return truncate(path,size); }
static int test_utimes(const char *path,const struct timeval times[2])
{ if (fail_operation==3) { errno=EIO;return -1; } return utimes(path,times); }
static void check_calendar(void)
{
    /* Reference conversion is host UTC, independent of the civil-day decoder. */
    const unsigned years[]={1,1600,1900,1969,1970,2000,2024,2038,2100,2400,9999};
    for (unsigned i=0;i<sizeof(years)/sizeof(years[0]);++i) for (unsigned month=1;month<=12;++month) {
        SceDateTime date={.year=years[i],.month=month,.day=1,.hour=23,.minute=59,.second=59,.microsecond=999999};
        struct timeval result;
        assert(!vita_datetime_to_timeval(&date,&result));
        struct tm reference={.tm_year=(int)date.year-1900,.tm_mon=(int)month-1,.tm_mday=1,
                             .tm_hour=23,.tm_min=59,.tm_sec=59};
        assert(result.tv_sec==timegm(&reference) && result.tv_usec==999999);
    }
    SceDateTime leap={.year=2000,.month=2,.day=29};struct timeval value;
    assert(!vita_datetime_to_timeval(&leap,&value));
    leap.year=1900;assert(vita_datetime_to_timeval(&leap,&value)<0);
    leap.year=2100;assert(vita_datetime_to_timeval(&leap,&value)<0);
    leap.year=2024;assert(!vita_datetime_to_timeval(&leap,&value));
}
int main(void)
{
    assert(!setenv("TZ","UTC-9",1));tzset();check_calendar();
    char directory[]="/tmp/vita2hos-metadata-XXXXXX";assert(mkdtemp(directory));
    strcpy(test_app0_path,directory);char path[PATH_MAX];
    assert(snprintf(path,sizeof(path),"%s/file",directory)>0);
    int fd=open(path,O_CREAT|O_RDWR|O_EXCL,0600);assert(fd>=0 && write(fd,"abcdef",6)==6);
    int (*change)(const char *,SceIoStat *,int)=(void *)module_get_func_export(0xcae9ace6,0x29482F7F);
    assert(change);SceIoStat request={0},original;
    assert((uint32_t)change(NULL,&request,0)==0x8001000e);
    assert((uint32_t)change(path,NULL,0)==0x8001000e);
    assert((uint32_t)change("ux0:/file",&request,0)==SCE_ERROR_ERRNO_ENODEV);
    assert((uint32_t)change("app0:/../file",&request,0)==0x8001000d);
    assert((uint32_t)change("app0:/missing",&request,0)==0x80010002);
    assert(!change("app0:/file",&request,0));
    request.st_mode=SCE_S_IFDIR|0640; /* Type stays a regular file; group bits ignored. */
    original=request;assert(!change("app0:/file",&request,SCE_CST_MODE));
    assert(!memcmp(&request,&original,sizeof(request)));
    struct stat native;assert(!fstat(fd,&native) && S_ISREG(native.st_mode) && (native.st_mode&0777)==0600);
    SceIoStat info;assert(!sceIoGetstat(path,&info) && (info.st_mode&0777)==0600 && SCE_S_ISREG(info.st_mode));
    request.st_size=INT64_C(5)*1024*1024*1024+17;
    assert(!change(path,&request,SCE_CST_SIZE));
    assert(!fstat(fd,&native) && native.st_size==request.st_size && lseek(fd,0,SEEK_CUR)==6);
    request.st_size=2;assert(!change(path,&request,SCE_CST_SIZE));
    assert(!fstat(fd,&native) && native.st_size==2);
    request.st_atime=(SceDateTime){.year=2024,.month=1,.day=1,.microsecond=123456};
    request.st_mtime=(SceDateTime){.year=2024,.month=2,.day=29,.hour=12,.minute=34,.second=56,.microsecond=654321};
    assert(!change(path,&request,SCE_CST_AT|SCE_CST_MT));
    assert(!sceIoGetstat(path,&info));
    assert(!memcmp(&info.st_atime,&request.st_atime,sizeof(SceDateTime)));
    assert(!memcmp(&info.st_mtime,&request.st_mtime,sizeof(SceDateTime)));
    SceDateTime old_atime=info.st_atime;request.st_mtime.hour=13;
    assert(!change(path,&request,SCE_CST_MT));assert(!sceIoGetstat(path,&info));
    assert(!memcmp(&info.st_atime,&old_atime,sizeof(old_atime)));
    assert(!memcmp(&info.st_mtime,&request.st_mtime,sizeof(SceDateTime)));
    request.st_size=0;request.st_mode=0777;
    const SceDateTime invalid[]={
        {.year=2023,.month=2,.day=29},{.year=2024,.month=4,.day=31},
        {.year=0,.month=1,.day=1},{.year=10000,.month=1,.day=1},
        {.year=2024,.month=0,.day=1},{.year=2024,.month=13,.day=1},
        {.year=2024,.month=1,.day=0},{.year=2024,.month=1,.day=1,.hour=24},
        {.year=2024,.month=1,.day=1,.minute=60},{.year=2024,.month=1,.day=1,.second=60},
        {.year=2024,.month=1,.day=1,.microsecond=1000000}};
    for (unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
        request.st_atime=invalid[i];
        assert((uint32_t)change(path,&request,SCE_CST_SIZE|SCE_CST_MODE|SCE_CST_AT)==SCE_ERROR_ERRNO_EINVAL);
        assert(!fstat(fd,&native) && native.st_size==2 && (native.st_mode&0777)==0600);
    }
    assert((uint32_t)change(path,&request,SCE_CST_SIZE|SCE_CST_CT)==0x8001004e);
    assert((uint32_t)change(path,&request,SCE_CST_MODE|0x80)==SCE_ERROR_ERRNO_EINVAL);
    request.st_size=-1;
    assert((uint32_t)change(path,&request,SCE_CST_SIZE|SCE_CST_MODE)==SCE_ERROR_ERRNO_EINVAL);
    assert(!fstat(fd,&native) && native.st_size==2 && (native.st_mode&0777)==0600);
    fail_operation=2;request.st_size=0;
    assert((uint32_t)change(path,&request,SCE_CST_SIZE)==0x80010005);
    fail_operation=1;
    assert((uint32_t)change(path,&request,SCE_CST_SIZE|SCE_CST_MODE)==0x8001004e);
    assert(!fstat(fd,&native) && !native.st_size); /* Earlier successful size change remains. */
    fail_operation=3;request.st_mtime=(SceDateTime){.year=2024,.month=1,.day=1};
    assert((uint32_t)change(path,&request,SCE_CST_MT)==0x80010005);
    fail_operation=0;close(fd);assert(!unlink(path) && !rmdir(directory));
    puts("File metadata: NID dispatch, permissions, sparse sizes, UTC calendars, timestamp precision, masks and backend errors passed.");
}
