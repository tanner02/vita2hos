#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <m-dict.h>
#include <psp2/kernel/error.h>
#include <psp2common/types.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <switch.h>
#include <unistd.h>
#undef st_ctime
#undef st_atime
#undef st_mtime
#include <psp2/io/dirent.h>

#include "log.h"
#include "config.h"
#include "module.h"
#include "util.h"

#include "modules/SceKernelThreadMgr.h"
#include "modules/SceLibKernel.h"
#include "modules/SceSysmem.h"

typedef struct {
    uint32_t index;
    SceUID uid;
    int native_fd;
    char *file;
} VitaOpenedFile;

typedef struct {
    uint32_t index;
    SceUID uid;
    DIR *dir;
    char *path;
} VitaOpenedDir;

static UEvent g_process_exit_event;
static atomic_int g_process_exit_res;

DICT_DEF2(vita_opened_files_dict, SceUID, M_DEFAULT_OPLIST, VitaOpenedFile *, M_POD_OPLIST)
static vita_opened_files_dict_t g_vita_opened_files;
static RwLock g_vita_opened_files_lock;

DICT_DEF2(vita_opened_dirs_dict, SceUID, M_DEFAULT_OPLIST, VitaOpenedDir *, M_POD_OPLIST)
static vita_opened_dirs_dict_t g_vita_opened_dirs;
static RwLock g_vita_opened_dirs_lock;

static VitaOpenedFile *get_opened_file_for_fd(SceUID fd)
{
    VitaOpenedFile *vfile;
    VitaOpenedFile **entry;

    rwlockReadLock(&g_vita_opened_files_lock);
    entry = vita_opened_files_dict_get(g_vita_opened_files, fd);
    vfile = entry ? *entry : NULL;
    rwlockReadUnlock(&g_vita_opened_files_lock);

    return vfile;
}

static VitaOpenedDir *get_opened_dir_for_fd(SceUID fd)
{
    VitaOpenedDir *vdir;
    VitaOpenedDir **entry;

    rwlockReadLock(&g_vita_opened_dirs_lock);
    entry = vita_opened_dirs_dict_get(g_vita_opened_dirs, fd);
    vdir = entry ? *entry : NULL;
    rwlockReadUnlock(&g_vita_opened_dirs_lock);

    return vdir;
}

EXPORT(SceLibKernel, 0xB295EB61, void *, sceKernelGetTLSAddr, int key)
{
    VitaThreadInfo *ti;

    if (key >= 0 && key <= 0x100) {
        ti = SceKernelThreadMgr_get_thread_info();
        return &ti->vita_tls[key];
    }

    return NULL;
}

EXPORT(SceLibKernel, 0x7595D9AA, int, sceKernelExitProcess, int res)
{
    LOG("sceKernelExitProcess called! Return value %d", res);

    atomic_store(&g_process_exit_res, res);
    ueventSignal(&g_process_exit_event);

    threadExit();

    return 0;
}

EXPORT(SceLibKernel, 0xDA6EC8EF, int, sceKernelCreateLwMutex, SceKernelLwMutexWork *pWork,
       const char *pName, unsigned int attr, int initCount,
       const SceKernelLwMutexOptParam *pOptParam)
{
    Mutex *mutex = (void *)pWork;
    mutexInit(mutex);
    if (initCount > 0)
        mutexLock(mutex);
    return 0;
}

EXPORT(SceLibKernel, 0x244E76D2, int, sceKernelDeleteLwMutex, SceKernelLwMutexWork *pWork)
{
    mutexUnlock((void *)pWork);
    return 0;
}

EXPORT(SceLibKernel, 0x46E7BE7B, int, sceKernelLockLwMutex, SceKernelLwMutexWork *pWork,
       int lockCount, unsigned int *pTimeout)
{
    mutexLock((void *)pWork);
    return 0;
}

EXPORT(SceLibKernel, 0xA6A2C915, int, sceKernelTryLockLwMutex, SceKernelLwMutexWork *pWork,
       int lockCount)
{
    if (mutexTryLock((void *)pWork))
        return 0;
    else
        return SCE_KERNEL_ERROR_LW_MUTEX_FAILED_TO_OWN;
}

EXPORT(SceLibKernel, 0x91FA6614, int, sceKernelUnlockLwMutex, SceKernelLwMutexWork *pWork,
       int unlockCount)
{
    mutexUnlock((void *)pWork);
    return 0;
}

static int io_errno(int error);

/* app0 is the directory containing the loaded executable. Do not alias other
 * Vita devices to the SD root: they need their own mount implementations. */
static int vita_io_path(const char *path, char *resolved, size_t capacity)
{
    if (!path)
        return io_errno(EFAULT);
    const char *device = strchr(path, ':');
    if (!device) {
        size_t length = strlen(path);
        if (length >= capacity)
            return io_errno(ENAMETOOLONG);
        memcpy(resolved, path, length + 1);
        return 0;
    }
    if (device - path != 4 || memcmp(path, "app0", 4))
        return SCE_ERROR_ERRNO_ENODEV;
    path = device + 1;
    size_t length = strlen(VITA2HOS_APP0_PATH);
    if (!length || length >= capacity)
        return io_errno(ENAMETOOLONG);
    memcpy(resolved, VITA2HOS_APP0_PATH, length + 1);
    while (length > 1 && resolved[length - 1] == '/')
        resolved[--length] = '\0';
    const size_t root_length = length;
    while (*path) {
        while (*path == '/') ++path;
        const char *component = path;
        while (*path && *path != '/') {
            if (*path == ':')
                return io_errno(EINVAL);
            ++path;
        }
        size_t count = path - component;
        if (!count || (count == 1 && component[0] == '.'))
            continue;
        if (count == 2 && component[0] == '.' && component[1] == '.') {
            if (length == root_length)
                return io_errno(EACCES);
            while (length > root_length && resolved[length - 1] != '/')
                --length;
            if (length > root_length) --length;
            resolved[length] = '\0';
            continue;
        }
        size_t separator = resolved[length - 1] != '/';
        if (count >= capacity - length - separator)
            return io_errno(ENAMETOOLONG);
        if (separator) resolved[length++] = '/';
        memcpy(resolved + length, component, count);
        length += count;
        resolved[length] = '\0';
    }
    return 0;
}

static int vita_io_open_flags(int flags, int *native_flags)
{
    const int supported = SCE_O_RDWR | SCE_O_APPEND | SCE_O_CREAT |
                          SCE_O_TRUNC | SCE_O_EXCL;
    if (flags & ~supported)
        return SCE_ERROR_ERRNO_EINVAL;
    switch (flags & SCE_O_RDWR) {
    case SCE_O_RDONLY: *native_flags = O_RDONLY; break;
    case SCE_O_WRONLY: *native_flags = O_WRONLY; break;
    case SCE_O_RDWR: *native_flags = O_RDWR; break;
    default: return SCE_ERROR_ERRNO_EINVAL;
    }
    if (flags & SCE_O_APPEND) *native_flags |= O_APPEND;
    if (flags & SCE_O_CREAT) *native_flags |= O_CREAT;
    if (flags & SCE_O_TRUNC) *native_flags |= O_TRUNC;
    if (flags & SCE_O_EXCL) *native_flags |= O_EXCL;
    return 0;
}

EXPORT(SceLibKernel, 0x6C60AC61, SceUID, sceIoOpen, const char *file, int flags, SceMode mode)
{
    VitaOpenedFile *vfile;
    char resolved[PATH_MAX];
    int native_flags, native_fd;

    if (!file)
        return io_errno(EFAULT);
    int result = vita_io_open_flags(flags, &native_flags);
    if (result)
        return result;

    result = vita_io_path(file, resolved, sizeof(resolved));
    if (result)
        return result;
    file = resolved;

    if (*file == '\0')
        return SCE_ERROR_ERRNO_ENODEV;

    native_fd = open(file, native_flags, mode & 0777);
    if (native_fd < 0)
        return io_errno(errno);

    vfile = malloc(sizeof(*vfile));
    if (!vfile) {
        close(native_fd);
        return io_errno(ENOMEM);
    }

    memset(vfile, 0, sizeof(*vfile));
    vfile->uid = SceSysmem_get_next_uid();
    vfile->native_fd = native_fd;
    vfile->file = strdup(file);
    if (!vfile->file) {
        close(native_fd);
        free(vfile);
        return io_errno(ENOMEM);
    }

    rwlockWriteLock(&g_vita_opened_files_lock);
    vita_opened_files_dict_set_at(g_vita_opened_files, vfile->uid, vfile);
    rwlockWriteUnlock(&g_vita_opened_files_lock);

    return vfile->uid;
}

EXPORT(SceIofilemgr, 0xC70B8886, int, sceIoClose, SceUID fd)
{
    VitaOpenedFile *vfile = get_opened_file_for_fd(fd);
    int ret;

    if (!vfile)
        return SCE_ERROR_ERRNO_EBADF;

    ret = close(vfile->native_fd);
    int saved_errno = errno;
    rwlockWriteLock(&g_vita_opened_files_lock);
    vita_opened_files_dict_erase(g_vita_opened_files, fd);
    rwlockWriteUnlock(&g_vita_opened_files_lock);
    free(vfile->file);
    free(vfile);

    if (ret)
        return io_errno(saved_errno);

    return 0;
}

EXPORT(SceIofilemgr, 0xFDB32293, SceSSize, sceIoRead, SceUID fd, void *buf, SceSize nbyte)
{
    VitaOpenedFile *vfile = get_opened_file_for_fd(fd);
    if (!vfile)
        return SCE_ERROR_ERRNO_EBADF;

    if (nbyte && !buf)
        return io_errno(EFAULT);
    ssize_t result = read(vfile->native_fd, buf, nbyte);
    return result < 0 ? io_errno(errno) : result;
}

EXPORT(SceIofilemgr, 0x34EFD876, SceSSize, sceIoWrite, SceUID fd, const void *buf, SceSize nbyte)
{
    VitaOpenedFile *vfile = get_opened_file_for_fd(fd);
    if (!vfile)
        return SCE_ERROR_ERRNO_EBADF;

    if (nbyte && !buf)
        return io_errno(EFAULT);
    ssize_t result = write(vfile->native_fd, buf, nbyte);
    return result < 0 ? io_errno(errno) : result;
}

EXPORT(SceIofilemgr, 0x49252B9B, long, sceIoLseek32, SceUID fd, long offset, int whence)
{
    VitaOpenedFile *vfile = get_opened_file_for_fd(fd);
    off_t position;

    if (!vfile)
        return SCE_ERROR_ERRNO_EBADF;

    position = lseek(vfile->native_fd, offset, whence);
    if (position < 0)
        return io_errno(errno);

    return position;
}

EXPORT(SceIofilemgr, 0x16512F59, int, sceIoSyncByFd, SceUID fd, int flag)
{
    VitaOpenedFile *vfile = get_opened_file_for_fd(fd);
    if (!vfile)
        return SCE_ERROR_ERRNO_EBADF;

    return fsync(vfile->native_fd) == 0 ? 0 : io_errno(errno);
}

EXPORT(SceLibKernel, 0xA9283DD0, SceUID, sceIoDopen, const char *dirname)
{
    VitaOpenedDir *vdir;
    char resolved[PATH_MAX];
    DIR *dir;

    int result = vita_io_path(dirname, resolved, sizeof(resolved));
    if (result)
        return result;
    dirname = resolved;

    dir = opendir(dirname);
    if (!dir)
        return io_errno(errno);

    vdir = malloc(sizeof(*vdir));
    if (!vdir) {
        closedir(dir);
        return SCE_ERROR_ERRNO_EMFILE;
    }

    memset(vdir, 0, sizeof(*vdir));
    vdir->uid = SceSysmem_get_next_uid();
    vdir->dir = dir;
    vdir->path = strdup(dirname);
    if (!vdir->path) {
        closedir(dir);
        free(vdir);
        return io_errno(ENOMEM);
    }

    rwlockWriteLock(&g_vita_opened_dirs_lock);
    vita_opened_dirs_dict_set_at(g_vita_opened_dirs, vdir->uid, vdir);
    rwlockWriteUnlock(&g_vita_opened_dirs_lock);

    return vdir->uid;
}

EXPORT(SceIofilemgr, 0x422A221A, int, sceIoDclose, SceUID fd)
{
    VitaOpenedDir *vdir = get_opened_dir_for_fd(fd);
    int ret;

    if (!vdir)
        return SCE_ERROR_ERRNO_EBADF;

    ret = closedir(vdir->dir);
    free(vdir->path);
    rwlockWriteLock(&g_vita_opened_dirs_lock);
    vita_opened_dirs_dict_erase(g_vita_opened_dirs, fd);
    rwlockWriteUnlock(&g_vita_opened_dirs_lock);
    free(vdir);

    if (ret)
        return SCE_ERROR_ERRNO_EBADF;

    return 0;
}

static inline void tm_to_sce_datetime(SceDateTime *dt, const struct tm *tm)
{
    dt->year = tm->tm_year + 1900;
    dt->month = tm->tm_mon + 1;
    dt->day = tm->tm_mday;
    dt->hour = tm->tm_hour;
    dt->minute = tm->tm_min;
    dt->second = tm->tm_sec;
    dt->microsecond = 0;
}

static int io_errno(int error)
{
    /* Translate explicitly: host errno numbering is not identical for all
     * POSIX errors. The Vita low word uses its own errno namespace. */
    switch (error) {
    case ENOENT: return (int)0x80010002;
    case EACCES: return (int)0x8001000d;
    case EFAULT: return (int)0x8001000e;
    case ENOMEM: return (int)0x8001000c;
    case EEXIST: return (int)0x80010011;
    case EMFILE: return SCE_ERROR_ERRNO_EMFILE;
    case ENOSPC: return (int)0x8001001c;
    case ENOTDIR: return (int)0x80010014;
    case EISDIR: return (int)0x80010015;
    case ENAMETOOLONG: return (int)0x8001005b;
    case EBADF: return SCE_ERROR_ERRNO_EBADF;
    case EINVAL: return SCE_ERROR_ERRNO_EINVAL;
    default: return (int)0x80010005; /* EIO */
    }
}

static int stat_to_sce(SceIoStat *destination, const struct stat *source)
{
    SceIoStat result = {0};
    if (S_ISDIR(source->st_mode)) {
        result.st_mode = SCE_S_IFDIR | SCE_S_IRWXU | SCE_S_IRWXG | SCE_S_IRWXS;
        result.st_attr = SCE_SO_IFDIR;
    } else {
        result.st_mode = SCE_S_IFREG | SCE_S_IRUSR | SCE_S_IWUSR | SCE_S_IRGRP |
                         SCE_S_IWGRP | SCE_S_IRSYS | SCE_S_IWSYS;
        result.st_attr = SCE_SO_IFREG;
    }
    result.st_size = source->st_size;
    struct tm time;
    if (!gmtime_r(&source->st_ctim.tv_sec, &time))
        return io_errno(EINVAL);
    tm_to_sce_datetime(&result.st_ctime, &time);
    if (!gmtime_r(&source->st_atim.tv_sec, &time))
        return io_errno(EINVAL);
    tm_to_sce_datetime(&result.st_atime, &time);
    if (!gmtime_r(&source->st_mtim.tv_sec, &time))
        return io_errno(EINVAL);
    tm_to_sce_datetime(&result.st_mtime, &time);
    *destination = result;
    return 0;
}

EXPORT(SceLibKernel, 0xBCA5B623, int, sceIoGetstat, const char *file, SceIoStat *info)
{
    if (!file || !info)
        return io_errno(EFAULT);
    char resolved[PATH_MAX];
    int result = vita_io_path(file, resolved, sizeof(resolved));
    if (result)
        return result;
    file = resolved;
    if (!*file)
        return SCE_ERROR_ERRNO_ENODEV;
    struct stat status;
    if (stat(file, &status) != 0)
        return io_errno(errno);
    return stat_to_sce(info, &status);
}

EXPORT(SceLibKernel, 0x57F8CD25, int, sceIoGetstatByFd, SceUID fd, SceIoStat *info)
{
    VitaOpenedFile *file = get_opened_file_for_fd(fd);
    if (!file)
        return SCE_ERROR_ERRNO_EBADF;
    if (!info)
        return io_errno(EFAULT);
    struct stat status;
    if (fstat(file->native_fd, &status) != 0)
        return io_errno(errno);
    return stat_to_sce(info, &status);
}

EXPORT(SceLibKernel, 0x9C8B6624, int, sceIoDread, SceUID fd, SceIoDirent *dirent)
{
    VitaOpenedDir *vdir = get_opened_dir_for_fd(fd);
    struct dirent *de;
    struct stat statbuf;
    const char *path;
    char full_path[PATH_MAX];

    if (!vdir)
        return SCE_ERROR_ERRNO_EBADF;
    if (!dirent)
        return io_errno(EFAULT);

    errno = 0;
    de = readdir(vdir->dir);
    if (!de)
        return errno ? io_errno(errno) : 0;

    int length = snprintf(full_path, sizeof(full_path), "%s/%s", vdir->path, de->d_name);
    if (length < 0 || (size_t)length >= sizeof(full_path))
        return io_errno(ENAMETOOLONG);
    path = full_path;

    if (stat(path, &statbuf) != 0)
        return io_errno(errno);

    int result = stat_to_sce(&dirent->d_stat, &statbuf);
    if (result)
        return result;
    snprintf(dirent->d_name, sizeof(dirent->d_name), "%s", de->d_name);
    dirent->d_private = NULL;

    return 1;
}

EXPORT_FUNC(SceLibKernel, 0x632980D7, memset)
EXPORT_FUNC(SceLibKernel, 0x14E9DBD7, memcpy)
EXPORT_FUNC(SceLibKernel, 0x736753C8, memmove)
EXPORT_FUNC(SceLibKernel, 0xFA26BC62, printf)

EXPORT(SceLibKernel, 0x0FB972F9, int, sceKernelGetThreadId)
{
    return SceKernelThreadMgr_get_thread_info()->uid;
}

DECLARE_LIBRARY(SceLibKernel, 0xcae9ace6);
DECLARE_LIBRARY(SceIofilemgr, 0xf2ff276e);

int SceLibKernel_init(void)
{
    vita_opened_files_dict_init(g_vita_opened_files);
    vita_opened_dirs_dict_init(g_vita_opened_dirs);
    rwlockInit(&g_vita_opened_files_lock);
    rwlockInit(&g_vita_opened_dirs_lock);
    ueventCreate(&g_process_exit_event, false);

    return 0;
}

UEvent *SceLibKernel_get_process_exit_uevent(void)
{
    return &g_process_exit_event;
}

int SceLibKernel_get_process_exit_res(void)
{
    return atomic_load(&g_process_exit_res);
}
