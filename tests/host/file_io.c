#include <assert.h>
#include <unistd.h>
#include <limits.h>

static char test_app0_path[PATH_MAX];
#define VITA2HOS_APP0_PATH test_app0_path

/* Include the production module to exercise its static descriptor tables too. */
#include "../../source/modules/SceLibKernel.c"

void log_printf(const char *format, ...) { (void)format; }
void SceKernelThreadMgr_prepare_current_exit(int status) { (void)status; }
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
    SceIoStat info;
    assert((uint32_t)sceIoGetstatByFd(fd, &info) == SCE_ERROR_ERRNO_EBADF);
    assert((uint32_t)sceIoClose(fd) == SCE_ERROR_ERRNO_EBADF);
    assert((uint32_t)sceIoRead(fd, &byte, 1) == SCE_ERROR_ERRNO_EBADF);
    assert((uint32_t)sceIoWrite(fd, "x", 1) == SCE_ERROR_ERRNO_EBADF);
    assert((uint32_t)sceIoLseek32(fd, 0, SEEK_SET) == SCE_ERROR_ERRNO_EBADF);
    assert(sceIoLseek(fd, 0, SCE_SEEK_SET) == (int32_t)SCE_ERROR_ERRNO_EBADF);
    assert((uint32_t)sceIoSyncByFd(fd, 0) == SCE_ERROR_ERRNO_EBADF);
}

static void check_bad_dir(SceUID fd)
{
    SceIoDirent entry;
    assert((uint32_t)sceIoDread(fd, &entry) == SCE_ERROR_ERRNO_EBADF);
    assert((uint32_t)sceIoDclose(fd) == SCE_ERROR_ERRNO_EBADF);
}

static void check_open_modes(void)
{
    char path[] = "/tmp/vita2hos-modes-XXXXXX";
    int native_fd = mkstemp(path);
    assert(native_fd >= 0);
    close(native_fd);
    unlink(path);
    assert((uint32_t)sceIoOpen(NULL, SCE_O_RDONLY, 0) == 0x8001000e);
    assert((uint32_t)sceIoOpen(path, 0, 0600) == SCE_ERROR_ERRNO_EINVAL);
    assert((uint32_t)sceIoOpen(path, SCE_O_RDWR | SCE_O_NOWAIT, 0600) ==
           SCE_ERROR_ERRNO_EINVAL);
    assert((uint32_t)sceIoOpen(path, SCE_O_RDONLY, 0) == 0x80010002);
    SceUID fd = sceIoOpen(path, SCE_O_RDWR | SCE_O_CREAT | SCE_O_EXCL, 0600);
    assert(fd > 0);
    assert(sceIoWrite(fd, "abc", 3) == 3);
    SceIoStat info;
    /* Written bytes must be visible without a separate sync/close. */
    assert(sceIoGetstat(path, &info) == 0 && info.st_size == 3);
    assert((uint32_t)sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_EXCL,
                              0600) == 0x80010011);
    assert(sceIoClose(fd) == 0);
    fd = sceIoOpen(path, SCE_O_WRONLY, 0);
    assert(fd > 0);
    char bytes[8] = {0};
    assert((uint32_t)sceIoRead(fd, bytes, 1) == SCE_ERROR_ERRNO_EBADF);
    assert(sceIoWrite(fd, "X", 1) == 1);
    assert(sceIoClose(fd) == 0);
    fd = sceIoOpen(path, SCE_O_RDWR | SCE_O_CREAT, 0600);
    assert(fd > 0);
    assert(sceIoRead(fd, bytes, 3) == 3 && !memcmp(bytes, "Xbc", 3));
    assert(sceIoWrite(fd, "d", 1) == 1);
    assert(sceIoRead(fd, bytes, sizeof(bytes)) == 0);
    assert((uint32_t)sceIoWrite(fd, NULL, 1) == 0x8001000e);
    assert((uint32_t)sceIoRead(fd, NULL, 1) == 0x8001000e);
    assert(sceIoClose(fd) == 0);
    fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_APPEND, 0);
    assert(fd > 0 && sceIoLseek32(fd, 0, SEEK_SET) == 0);
    assert(sceIoWrite(fd, "e", 1) == 1);
    assert(sceIoClose(fd) == 0);
    fd = sceIoOpen(path, SCE_O_RDONLY, 0);
    assert(fd > 0);
    assert(sceIoRead(fd, bytes, 8) == 5 && !memcmp(bytes, "Xbcde", 5));
    assert((uint32_t)sceIoWrite(fd, "x", 1) == SCE_ERROR_ERRNO_EBADF);
    assert(sceIoClose(fd) == 0);
    fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_TRUNC, 0);
    assert(fd > 0);
    assert(sceIoGetstat(path, &info) == 0 && info.st_size == 0);
    assert(sceIoClose(fd) == 0);
    unlink(path);
}

static void check_app0_paths(void)
{
    char directory[] = "/tmp/vita2hos-app0-XXXXXX";
    assert(mkdtemp(directory));
    strcpy(test_app0_path, directory);
    char native_path[PATH_MAX];
    snprintf(native_path, sizeof(native_path), "%s/asset.bin", directory);
    int native_fd = open(native_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    assert(native_fd >= 0 && write(native_fd, "texture", 7) == 7);
    close(native_fd);
    const char *aliases[] = {"app0:asset.bin", "app0:/asset.bin",
                            "app0://./sub/../asset.bin"};
    for (size_t i = 0; i < ARRAY_SIZE(aliases); ++i) {
        SceUID fd = sceIoOpen(aliases[i], SCE_O_RDONLY, 0);
        assert(fd > 0);
        char contents[8] = {0};
        assert(sceIoRead(fd, contents, 8) == 7 && !strcmp(contents, "texture"));
        SceIoStat info;
        assert(sceIoGetstat(aliases[i], &info) == 0 && info.st_size == 7);
        assert(sceIoClose(fd) == 0);
    }
    SceIoStat info;
    assert(sceIoGetstat("app0:", &info) == 0 && SCE_S_ISDIR(info.st_mode));
    assert(sceIoGetstat("app0:/sub/..", &info) == 0 && SCE_S_ISDIR(info.st_mode));
    assert((uint32_t)sceIoOpen("app0:/../outside", SCE_O_RDONLY, 0) == 0x8001000d);
    assert((uint32_t)sceIoGetstat("app0:/../outside", &info) == 0x8001000d);
    assert((uint32_t)sceIoDopen("app0:/../") == 0x8001000d);
    assert((uint32_t)sceIoGetstat("ux0:/asset.bin", &info) == SCE_ERROR_ERRNO_ENODEV);
    assert((uint32_t)sceIoOpen("other:/asset.bin", SCE_O_RDONLY, 0) == SCE_ERROR_ERRNO_ENODEV);
    assert((uint32_t)sceIoDopen(NULL) == 0x8001000e);
    assert((uint32_t)sceIoGetstat("app0:/missing", &info) == 0x80010002);
    char oversized[PATH_MAX * 2];
    memset(oversized, 'x', sizeof(oversized));
    memcpy(oversized, "app0:/", 6);
    oversized[sizeof(oversized) - 1] = '\0';
    assert((uint32_t)sceIoOpen(oversized, SCE_O_RDONLY, 0) == 0x8001005b);
    assert((uint32_t)sceIoDopen(oversized) == 0x8001005b);
    assert((uint32_t)sceIoGetstat(oversized, &info) == 0x8001005b);
    SceUID dfd = sceIoDopen("app0:/");
    assert(dfd > 0);
    SceIoDirent entry;
    int found = 0, result;
    while ((result = sceIoDread(dfd, &entry)) == 1) {
        if (!strcmp(entry.d_name, "asset.bin")) {
            assert(entry.d_stat.st_size == 7);
            found = 1;
        }
    }
    assert(result == 0 && found);
    assert(sceIoDclose(dfd) == 0);
    unlink(native_path);
    rmdir(directory);
}

static void check_large_seeks(void)
{
    char path[] = "/tmp/vita2hos-seek-XXXXXX";
    int native_fd = mkstemp(path);
    assert(native_fd >= 0);
    close(native_fd);
    SceUID fd = sceIoOpen(path, SCE_O_RDWR, 0);
    assert(fd > 0);
    const SceOff offset = (SceOff)5 * 1024 * 1024 * 1024 + 17;
    assert(sceIoLseek(fd, offset, SCE_SEEK_SET) == offset);
    assert(sceIoWrite(fd, "wide", 4) == 4);
    assert(sceIoLseek(fd, 0, SCE_SEEK_CUR) == offset + 4);
    assert(sceIoLseek(fd, -4, SCE_SEEK_END) == offset);
    char bytes[4];
    assert(sceIoRead(fd, bytes, sizeof(bytes)) == 4 && !memcmp(bytes, "wide", 4));
    assert(sceIoLseek(fd, -5, SCE_SEEK_CUR) == offset - 1);
    assert(sceIoRead(fd, bytes, 1) == 1 && bytes[0] == 0);
    assert(sceIoLseek(fd, 0, SCE_SEEK_END) == offset + 4);
    SceIoStat info;
    assert(sceIoGetstatByFd(fd, &info) == 0 && info.st_size == offset + 4);
    assert(sceIoLseek(fd, -1, SCE_SEEK_SET) == (int32_t)SCE_ERROR_ERRNO_EINVAL);
    assert(sceIoLseek(fd, 0, 3) == (int32_t)SCE_ERROR_ERRNO_EINVAL);
    assert(sceIoLseek(fd, 0, SCE_SEEK_CUR) == offset + 4);
    assert(sceIoClose(fd) == 0);
    unlink(path);
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
    check_open_modes();
    check_app0_paths();
    check_large_seeks();

    check_bad_file(-1);
    check_bad_dir(-1);
    fd = sceIoOpen(path, SCE_O_RDWR, 0);
    assert(fd > 0);
    assert(sceIoWrite(fd, "abcdef", 6) == 6);
    assert(sceIoSyncByFd(fd, 0) == 0);
    SceIoStat info, by_fd;
    assert(sceIoGetstat(path, &info) == 0);
    assert(sceIoGetstatByFd(fd, &by_fd) == 0);
    assert(info.st_size == 6 && by_fd.st_size == 6);
    assert(info.st_mode & SCE_S_IFREG);
    assert(info.st_attr == SCE_SO_IFREG);
    assert(info.st_mtime.year >= 2020);
    assert(info.st_mtime.month >= 1 && info.st_mtime.month <= 12);
    assert((uint32_t)sceIoGetstat(NULL, &info) == 0x8001000e);
    assert((uint32_t)sceIoGetstat(path, NULL) == 0x8001000e);
    assert((uint32_t)sceIoGetstatByFd(fd, NULL) == 0x8001000e);
    struct stat known = {0};
    known.st_mode = S_IFDIR;
    known.st_mtim.tv_sec = 1704067200; /* 2024-01-01 UTC */
    assert(stat_to_sce(&info, &known) == 0);
    assert(info.st_mtime.year == 2024 && info.st_mtime.month == 1);
    assert(info.st_mtime.day == 1 && info.st_mtime.hour == 0);
    assert(info.st_attr == SCE_SO_IFDIR);
    assert(info.st_private[0] == 0);
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
    SceIoDirent entry;
    assert((uint32_t)sceIoDread(dfd, NULL) == 0x8001000e);
    int read_result, found = 0;
    while ((read_result = sceIoDread(dfd, &entry)) == 1) {
        if (!strcmp(entry.d_name, strrchr(path, '/') + 1)) {
            assert(entry.d_stat.st_size == 6);
            assert(entry.d_stat.st_mtime.year >= 2020);
            found = 1;
        }
    }
    assert(read_result == 0 && found);
    assert(sceIoDclose(dfd) == 0);
    check_bad_dir(dfd);
    assert(sceIoClose(fd) == 0);
    check_bad_file(fd);

    vita_opened_files_dict_clear(g_vita_opened_files);
    vita_opened_dirs_dict_clear(g_vita_opened_dirs);
    unlink(path);
    memset(&info, 0x55, sizeof(info));
    SceIoStat unchanged = info;
    assert((uint32_t)sceIoGetstat(path, &info) == 0x80010002);
    assert(memcmp(&info, &unchanged, sizeof(info)) == 0);
    puts("File I/O regression checks passed.");
    return 0;
}
