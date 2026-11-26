#include <assert.h>
#include <unistd.h>

/* Include the production module to exercise its static descriptor tables too. */
#include "../../source/modules/SceLibKernel.c"

void log_printf(const char *format, ...) { (void)format; }
SceUID SceSysmem_get_next_uid(void)
{
    static SceUID next_uid = 1;
    return next_uid++;
}
VitaThreadInfo *SceKernelThreadMgr_get_thread_info(void)
{
    static VitaThreadInfo info;
    return &info;
}

static void check_bad_file(SceUID fd)
{
    char byte;
    assert((uint32_t)sceIoClose(fd) == SCE_ERROR_ERRNO_EBADF);
    assert((uint32_t)sceIoRead(fd, &byte, 1) == SCE_ERROR_ERRNO_EBADF);
    assert((uint32_t)sceIoWrite(fd, "x", 1) == SCE_ERROR_ERRNO_EBADF);
    assert((uint32_t)sceIoLseek32(fd, 0, SEEK_SET) == SCE_ERROR_ERRNO_EBADF);
    assert((uint32_t)sceIoSyncByFd(fd, 0) == SCE_ERROR_ERRNO_EBADF);
}

static void check_bad_dir(SceUID fd)
{
    SceIoDirent entry;
    assert((uint32_t)sceIoDread(fd, &entry) == SCE_ERROR_ERRNO_EBADF);
    assert((uint32_t)sceIoDclose(fd) == SCE_ERROR_ERRNO_EBADF);
}

int main(void)
{
    char path[] = "/tmp/vita2hos-io-XXXXXX";
    int native_fd = mkstemp(path);
    char bytes[3] = {0};
    SceUID fd, dfd;

    assert(native_fd >= 0);
    close(native_fd);
    assert(SceLibKernel_init() == 0);

    check_bad_file(-1);
    check_bad_dir(-1);
    fd = sceIoOpen(path, SCE_O_RDWR, 0);
    assert(fd > 0);
    assert(sceIoWrite(fd, "abcdef", 6) == 6);
    assert(sceIoLseek32(fd, 2, SEEK_SET) == 2);
    assert(sceIoRead(fd, bytes, 2) == 2);
    assert(strcmp(bytes, "cd") == 0);
    assert(sceIoLseek32(fd, -1, SEEK_CUR) == 3);
    assert(sceIoLseek32(fd, -1, SEEK_END) == 5);
    assert(sceIoLseek32(fd, 0, SEEK_SET) == 0);
    assert((uint32_t)sceIoLseek32(fd, -1, SEEK_SET) == SCE_ERROR_ERRNO_EINVAL);
    assert((uint32_t)sceIoLseek32(fd, 0, -1) == SCE_ERROR_ERRNO_EINVAL);

    dfd = sceIoDopen("/tmp");
    assert(dfd > 0);
    check_bad_file(dfd);
    check_bad_dir(fd);
    assert(sceIoDclose(dfd) == 0);
    check_bad_dir(dfd);
    assert(sceIoClose(fd) == 0);
    check_bad_file(fd);

    vita_opened_files_dict_clear(g_vita_opened_files);
    vita_opened_dirs_dict_clear(g_vita_opened_dirs);
    unlink(path);
    puts("File I/O regression checks passed.");
    return 0;
}
