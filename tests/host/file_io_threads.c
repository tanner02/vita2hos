#include <assert.h>
#include <dirent.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_THREADED_RWLOCK
#define TEST_THREADED_MUTEX
static ssize_t gated_read(int, void *, size_t);
static ssize_t gated_write(int, const void *, size_t);
static off_t gated_lseek(int, off_t, int);
static int gated_fsync(int);
static int gated_fstat(int, struct stat *);
static struct dirent *gated_readdir(DIR *);
static int gated_close(int);
static int gated_closedir(DIR *);
#define read gated_read
#define write gated_write
#define lseek gated_lseek
#define fsync gated_fsync
#define fstat gated_fstat
#define readdir gated_readdir
#define close gated_close
#define closedir gated_closedir
#include "../../source/modules/SceLibKernel.c"
#undef read
#undef write
#undef lseek
#undef fsync
#undef fstat
#undef readdir
#undef close
#undef closedir

void log_printf(const char *format, ...) { (void)format; }
void SceKernelThreadMgr_prepare_current_exit(int status) { (void)status; }
SceUID SceSysmem_get_next_uid(void)
{
    static atomic_int next_uid = 1;
    return atomic_fetch_add(&next_uid, 1);
}
VitaThreadInfo *SceKernelThreadMgr_get_thread_info(void)
{
    static _Thread_local VitaThreadInfo info;
    return &info;
}

enum Operation { READ, WRITE, PREAD, PWRITE, SEEK, SYNC, STAT, DREAD, CLOSE, DCLOSE };
static enum Operation operation;
static _Thread_local int intercept;
static pthread_mutex_t gate_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate_changed = PTHREAD_COND_INITIALIZER;
static int entered, released;
static atomic_bool closer_started;
static SceUID handle;

static void gate(enum Operation native_operation)
{
    if (!intercept || (operation != native_operation &&
        !(operation==PREAD && native_operation==READ) &&
        !(operation==PWRITE && native_operation==WRITE))) return;
    assert(pthread_mutex_lock(&gate_lock) == 0);
    entered = 1;
    assert(pthread_cond_broadcast(&gate_changed) == 0);
    while (!released)
        assert(pthread_cond_wait(&gate_changed, &gate_lock) == 0);
    assert(pthread_mutex_unlock(&gate_lock) == 0);
}
static ssize_t gated_read(int fd, void *data, size_t size)
{ gate(READ); return read(fd, data, size); }
static ssize_t gated_write(int fd, const void *data, size_t size)
{ gate(WRITE); return write(fd, data, size); }
static off_t gated_lseek(int fd, off_t offset, int whence)
{ gate(SEEK); return lseek(fd, offset, whence); }
static int gated_fsync(int fd)
{ gate(SYNC); return fsync(fd); }
static int gated_fstat(int fd, struct stat *status)
{ gate(STAT); return fstat(fd, status); }
static struct dirent *gated_readdir(DIR *dir)
{ gate(DREAD); return readdir(dir); }
static int gated_close(int fd)
{ gate(CLOSE); return close(fd); }
static int gated_closedir(DIR *dir)
{ gate(DCLOSE); return closedir(dir); }

static void *perform_operation(void *argument)
{
    (void)argument;
    intercept = 1;
    char byte;
    SceIoStat info;
    SceIoDirent entry;
    switch (operation) {
    case READ: assert(sceIoRead(handle, &byte, 1) == 1); break;
    case WRITE: assert(sceIoWrite(handle, "x", 1) == 1); break;
    case PREAD: assert(sceIoPread(handle, &byte, 1, 0) == 1); break;
    case PWRITE: assert(sceIoPwrite(handle, "x", 1, 0) == 1); break;
    case SEEK: assert(sceIoLseek(handle, 0, SCE_SEEK_SET) == 0); break;
    case SYNC: assert(sceIoSyncByFd(handle, 0) == 0); break;
    case STAT: assert(sceIoGetstatByFd(handle, &info) == 0); break;
    case DREAD: assert(sceIoDread(handle, &entry) == 1); break;
    case CLOSE: assert(sceIoClose(handle) == 0); break;
    case DCLOSE: assert(sceIoDclose(handle) == 0); break;
    }
    return NULL;
}

static void *perform_close(void *argument)
{
    (void)argument;
    atomic_store(&closer_started, true);
    int result = operation == DREAD || operation == DCLOSE ?
                 sceIoDclose(handle) : sceIoClose(handle);
    assert(result == (operation == CLOSE || operation == DCLOSE ?
                      (int32_t)SCE_ERROR_ERRNO_EBADF : 0));
    return NULL;
}

int main(void)
{
    alarm(15); /* A broken lock must fail the check rather than hang CI. */
    char path[] = "/tmp/vita2hos-io-threads-XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0 && write(fd, "x", 1) == 1);
    close(fd);
    assert(SceLibKernel_init() == 0);
    SceUID argument_fd = sceIoOpen(path, SCE_O_RDWR, 0);
    assert(argument_fd > 0);
    assert((uint32_t)sceIoRead(argument_fd, NULL, 1) == 0x8001000e);
    assert((uint32_t)sceIoWrite(argument_fd, NULL, 1) == 0x8001000e);
    assert((uint32_t)sceIoGetstatByFd(argument_fd, NULL) == 0x8001000e);
    assert(sceIoLseek(argument_fd, 0, 3) == (int32_t)SCE_ERROR_ERRNO_EINVAL);
    assert(pthread_rwlock_trywrlock(&g_vita_opened_files_lock) == 0);
    assert(pthread_rwlock_unlock(&g_vita_opened_files_lock) == 0);
    assert(sceIoClose(argument_fd) == 0);
    for (operation = READ; operation <= DCLOSE; ++operation) {
        int directory = operation == DREAD || operation == DCLOSE;
        handle = directory ? sceIoDopen("/tmp") : sceIoOpen(path, SCE_O_RDWR, 0);
        assert(handle > 0);
        entered = released = 0;
        atomic_store(&closer_started, false);
        pthread_t worker, closer;
        assert(pthread_create(&worker, NULL, perform_operation, NULL) == 0);
        assert(pthread_mutex_lock(&gate_lock) == 0);
        while (!entered)
            assert(pthread_cond_wait(&gate_changed, &gate_lock) == 0);
        /* Deterministically verify lifetime protection at the native call,
         * rather than depending on a scheduler delay to expose the race. */
        RwLock *table_lock = directory ? &g_vita_opened_dirs_lock : &g_vita_opened_files_lock;
        assert(pthread_rwlock_trywrlock(table_lock) == EBUSY);
        if (operation<=SEEK) {
            rwlockReadLock(table_lock);
            VitaOpenedFile *file=get_opened_file_for_fd_locked(handle);assert(file);
            assert(pthread_mutex_trylock(&file->io_lock)==EBUSY);
            rwlockReadUnlock(table_lock);
        }
        assert(pthread_create(&closer, NULL, perform_close, NULL) == 0);
        while (!atomic_load(&closer_started)) sched_yield();
        released = 1;
        assert(pthread_cond_broadcast(&gate_changed) == 0);
        assert(pthread_mutex_unlock(&gate_lock) == 0);
        assert(pthread_join(worker, NULL) == 0);
        assert(pthread_join(closer, NULL) == 0);
        assert((uint32_t)(directory ? sceIoDclose(handle) : sceIoClose(handle)) ==
               SCE_ERROR_ERRNO_EBADF);
        assert(pthread_rwlock_trywrlock(table_lock) == 0);
        assert(pthread_rwlock_unlock(table_lock) == 0);
    }
    vita_opened_files_dict_clear(g_vita_opened_files);
    vita_opened_dirs_dict_clear(g_vita_opened_dirs);
    pthread_rwlock_destroy(&g_vita_opened_files_lock);
    pthread_rwlock_destroy(&g_vita_opened_dirs_lock);
    unlink(path);
    puts("File I/O threaded lifetime checks passed.");
    return 0;
}
