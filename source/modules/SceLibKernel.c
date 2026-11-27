#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <m-dict.h>
#include <psp2/kernel/error.h>
#include <psp2/kernel/processmgr.h>
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
    int standard_stream; /* 0: owned file; 1/2/3: borrowed stdin/out/err. */
    char *file;
    Mutex io_lock; /* Serializes cursor operations, including positional emulation. */
} VitaOpenedFile;

typedef struct {
    uint32_t index;
    SceUID uid;
    DIR *dir;
    char *path;
} VitaOpenedDir;

static UEvent g_process_exit_event;
static atomic_int g_process_exit_res;
static atomic_bool g_process_exit_requested;

DICT_DEF2(vita_opened_files_dict, SceUID, M_DEFAULT_OPLIST, VitaOpenedFile *, M_POD_OPLIST)
static vita_opened_files_dict_t g_vita_opened_files;
static RwLock g_vita_opened_files_lock;

DICT_DEF2(vita_opened_dirs_dict, SceUID, M_DEFAULT_OPLIST, VitaOpenedDir *, M_POD_OPLIST)
static vita_opened_dirs_dict_t g_vita_opened_dirs;
static RwLock g_vita_opened_dirs_lock;

/* The caller holds the table lock until it has finished using the entry.
 * Close takes the write lock, excluding every outstanding operation. */
static VitaOpenedFile *get_opened_file_for_fd_locked(SceUID fd)
{
    VitaOpenedFile *vfile;
    VitaOpenedFile **entry;

    entry = vita_opened_files_dict_get(g_vita_opened_files, fd);
    vfile = entry ? *entry : NULL;

    return vfile;
}

static VitaOpenedDir *get_opened_dir_for_fd_locked(SceUID fd)
{
    VitaOpenedDir *vdir;
    VitaOpenedDir **entry;

    entry = vita_opened_dirs_dict_get(g_vita_opened_dirs, fd);
    vdir = entry ? *entry : NULL;

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
    atomic_store(&g_process_exit_requested, true);
    ueventSignal(&g_process_exit_event);

    SceKernelThreadMgr_prepare_current_exit(res);
    threadExit();

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
    mutexInit(&vfile->io_lock);
    vfile->file = strdup(file);
    if (!vfile->file) {
        close(native_fd);
        free(vfile);
        return io_errno(ENOMEM);
    }

    SceUID uid = vfile->uid;
    rwlockWriteLock(&g_vita_opened_files_lock);
    vita_opened_files_dict_set_at(g_vita_opened_files, uid, vfile);
    rwlockWriteUnlock(&g_vita_opened_files_lock);

    return uid;
}

/* Standard descriptors belong to the native runtime. Each getter publishes a
 * distinct guest UID; closing it releases only the guest wrapper. No native
 * FILE object or numeric host descriptor is exposed to the guest. */
static SceUID open_standard_stream(int stream)
{
    VitaOpenedFile *file=calloc(1,sizeof(*file));
    if (!file) return io_errno(ENOMEM);
    file->file=strdup("tty0:");
    if (!file->file) { free(file);return io_errno(ENOMEM); }
    file->uid=SceSysmem_get_next_uid();
    file->native_fd=stream-1;
    file->standard_stream=stream;
    mutexInit(&file->io_lock);
    SceUID uid=file->uid;
    rwlockWriteLock(&g_vita_opened_files_lock);
    vita_opened_files_dict_set_at(g_vita_opened_files,uid,file);
    rwlockWriteUnlock(&g_vita_opened_files_lock);
    return uid;
}

/* SceProcessmgr's library descriptor is declared in SceProcessmgr.c. */
EXPORT(SceProcessmgr,0xC1727F59,SceUID,sceKernelGetStdin,void)
{ return open_standard_stream(1); }
EXPORT(SceProcessmgr,0xE5AA625C,SceUID,sceKernelGetStdout,void)
{ return open_standard_stream(2); }
EXPORT(SceProcessmgr,0xFA5E3ADA,SceUID,sceKernelGetStderr,void)
{ return open_standard_stream(3); }

EXPORT(SceIofilemgr, 0xC70B8886, int, sceIoClose, SceUID fd)
{
    rwlockWriteLock(&g_vita_opened_files_lock);
    VitaOpenedFile *vfile = get_opened_file_for_fd_locked(fd);
    int ret;

    if (!vfile) {
        rwlockWriteUnlock(&g_vita_opened_files_lock);
        return SCE_ERROR_ERRNO_EBADF;
    }

    ret = vfile->standard_stream ? 0 : close(vfile->native_fd);
    int saved_errno = errno;
    vita_opened_files_dict_erase(g_vita_opened_files, fd);
    free(vfile->file);
    free(vfile);
    rwlockWriteUnlock(&g_vita_opened_files_lock);

    if (ret)
        return io_errno(saved_errno);

    return 0;
}

EXPORT(SceIofilemgr, 0xFDB32293, SceSSize, sceIoRead, SceUID fd, void *buf, SceSize nbyte)
{
    rwlockReadLock(&g_vita_opened_files_lock);
    VitaOpenedFile *vfile = get_opened_file_for_fd_locked(fd);
    ssize_t result;
    if (!vfile || vfile->standard_stream>1) result = io_errno(EBADF);
    else if (nbyte && !buf) result = io_errno(EFAULT);
    else {
        mutexLock(&vfile->io_lock);
        result = read(vfile->native_fd, buf, nbyte);
        if (result < 0) result = io_errno(errno);
        mutexUnlock(&vfile->io_lock);
    }
    rwlockReadUnlock(&g_vita_opened_files_lock);
    return result;
}

EXPORT(SceIofilemgr, 0x34EFD876, SceSSize, sceIoWrite, SceUID fd, const void *buf, SceSize nbyte)
{
    rwlockReadLock(&g_vita_opened_files_lock);
    VitaOpenedFile *vfile = get_opened_file_for_fd_locked(fd);
    ssize_t result;
    if (!vfile || vfile->standard_stream==1) result = io_errno(EBADF);
    else if (nbyte && !buf) result = io_errno(EFAULT);
    else {
        mutexLock(&vfile->io_lock);
        result = write(vfile->native_fd, buf, nbyte);
        if (result < 0) result = io_errno(errno);
        mutexUnlock(&vfile->io_lock);
    }
    rwlockReadUnlock(&g_vita_opened_files_lock);
    return result;
}

_Static_assert(sizeof(off_t) >= sizeof(SceOff), "Vita seeks require 64-bit native offsets");
_Static_assert((off_t)-1 < 0, "Vita seeks require signed native offsets");

/* This pinned native C library has no pread/pwrite entry points. Hold the
 * per-handle cursor lock across save/seek/transfer/restore, while the table's
 * read lock prevents close from reclaiming the descriptor. */
static int vita_io_positioned(SceUID fd, void *read_data, const void *write_data,
                              SceSize size, SceOff offset, bool writing)
{
    rwlockReadLock(&g_vita_opened_files_lock);
    VitaOpenedFile *file=get_opened_file_for_fd_locked(fd);
    int result;
    if (!file) result=io_errno(EBADF);
    else if (size && !(writing ? write_data : read_data)) result=io_errno(EFAULT);
    else if (offset < 0 || size > INT32_MAX) result=io_errno(EINVAL);
    else if (file->standard_stream) result=io_errno(ESPIPE);
    else {
        mutexLock(&file->io_lock);
        off_t saved=lseek(file->native_fd,0,SEEK_CUR);
        if (saved < 0) result=io_errno(errno);
        else {
            if (lseek(file->native_fd,offset,SEEK_SET) < 0) result=io_errno(errno);
            else {
                ssize_t transferred=writing ? write(file->native_fd,write_data,size)
                                           : read(file->native_fd,read_data,size);
                result=transferred < 0 ? io_errno(errno) : (int)transferred;
            }
            if (lseek(file->native_fd,saved,SEEK_SET) < 0) result=io_errno(errno);
        }
        mutexUnlock(&file->io_lock);
    }
    rwlockReadUnlock(&g_vita_opened_files_lock);
    return result;
}

EXPORT(SceLibKernel, 0x52315AD7, int, sceIoPread, SceUID fd, void *data,
       SceSize size, SceOff offset)
{
    return vita_io_positioned(fd,data,NULL,size,offset,false);
}

EXPORT(SceLibKernel, 0x8FFFF5A8, int, sceIoPwrite, SceUID fd, const void *data,
       SceSize size, SceOff offset)
{
    return vita_io_positioned(fd,NULL,data,size,offset,true);
}

static SceOff vita_io_seek(SceUID fd, SceOff offset, int whence)
{
    rwlockReadLock(&g_vita_opened_files_lock);
    VitaOpenedFile *vfile = get_opened_file_for_fd_locked(fd);
    SceOff position;

    if (!vfile) {
        position = io_errno(EBADF);
        goto done;
    }
    if (vfile->standard_stream) { position=io_errno(ESPIPE);goto done; }
    switch (whence) {
    case SCE_SEEK_SET: whence = SEEK_SET; break;
    case SCE_SEEK_CUR: whence = SEEK_CUR; break;
    case SCE_SEEK_END: whence = SEEK_END; break;
    default: position = io_errno(EINVAL); goto done;
    }

    mutexLock(&vfile->io_lock);
    position = lseek(vfile->native_fd, offset, whence);
    if (position < 0)
        position = io_errno(errno);
    mutexUnlock(&vfile->io_lock);

done:
    rwlockReadUnlock(&g_vita_opened_files_lock);
    return position;
}

EXPORT(SceLibKernel, 0x99BA173E, SceOff, sceIoLseek, SceUID fd, SceOff offset, int whence)
{
    return vita_io_seek(fd, offset, whence);
}

EXPORT(SceIofilemgr, 0x49252B9B, long, sceIoLseek32, SceUID fd, long offset, int whence)
{
    return (long)vita_io_seek(fd, offset, whence);
}

EXPORT(SceIofilemgr, 0x16512F59, int, sceIoSyncByFd, SceUID fd, int flag)
{
    rwlockReadLock(&g_vita_opened_files_lock);
    VitaOpenedFile *vfile = get_opened_file_for_fd_locked(fd);
    int result = !vfile ? io_errno(EBADF) :
                 fsync(vfile->native_fd) == 0 ? 0 : io_errno(errno);
    rwlockReadUnlock(&g_vita_opened_files_lock);
    return result;
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

    SceUID uid = vdir->uid;
    rwlockWriteLock(&g_vita_opened_dirs_lock);
    vita_opened_dirs_dict_set_at(g_vita_opened_dirs, uid, vdir);
    rwlockWriteUnlock(&g_vita_opened_dirs_lock);

    return uid;
}

EXPORT(SceIofilemgr, 0x422A221A, int, sceIoDclose, SceUID fd)
{
    rwlockWriteLock(&g_vita_opened_dirs_lock);
    VitaOpenedDir *vdir = get_opened_dir_for_fd_locked(fd);
    int ret;

    if (!vdir) {
        rwlockWriteUnlock(&g_vita_opened_dirs_lock);
        return SCE_ERROR_ERRNO_EBADF;
    }

    ret = closedir(vdir->dir);
    free(vdir->path);
    vita_opened_dirs_dict_erase(g_vita_opened_dirs, fd);
    free(vdir);
    rwlockWriteUnlock(&g_vita_opened_dirs_lock);

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
    case ESPIPE: return (int)0x8001001d;
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
    rwlockReadLock(&g_vita_opened_files_lock);
    VitaOpenedFile *file = get_opened_file_for_fd_locked(fd);
    int result;
    struct stat status;
    if (!file) result = io_errno(EBADF);
    else if (!info) result = io_errno(EFAULT);
    else if (fstat(file->native_fd, &status) != 0) result = io_errno(errno);
    else result = stat_to_sce(info, &status);
    rwlockReadUnlock(&g_vita_opened_files_lock);
    return result;
}

static int vita_io_read_dir(VitaOpenedDir *vdir, SceIoDirent *dirent)
{
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

EXPORT(SceLibKernel, 0x9C8B6624, int, sceIoDread, SceUID fd, SceIoDirent *dirent)
{
    /* readdir's returned entry belongs to DIR; serialize through copying it. */
    rwlockWriteLock(&g_vita_opened_dirs_lock);
    int result = vita_io_read_dir(get_opened_dir_for_fd_locked(fd), dirent);
    rwlockWriteUnlock(&g_vita_opened_dirs_lock);
    return result;
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
    atomic_store(&g_process_exit_requested,false);
    atomic_store(&g_process_exit_res,0);
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

bool SceLibKernel_process_exit_requested(void)
{
    return atomic_load(&g_process_exit_requested);
}
