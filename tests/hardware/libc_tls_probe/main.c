#include <psp2/io/fcntl.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <stdint.h>
#include <stddef.h>

/* The builder publishes this owned seed as a 4/16-byte TLS template. */
const uint32_t probe_tls_seed __attribute__((used)) = 0x13579bdf;
typedef struct { uint32_t module, offset; } TlsIndex;
static TlsIndex seed_index = { UINT32_MAX, 0 };
static TlsIndex marker_index = { UINT32_MAX, 4 };
extern void *libc_tls_address(TlsIndex *) __asm__("__tls_get_addr");
static SceUID report = -1;
static int report_failed;
static volatile uintptr_t main_address;

static void record(const char *text)
{
    unsigned size = 0;
    while (text[size]) ++size;
    if (report >= 0 && sceIoWrite(report, text, size) != (int)size) report_failed = 1;
}

static void value(const char *label, uint32_t number)
{
    char text[13] = " 0x00000000\n";
    for (unsigned i = 0; i < 8; ++i) text[3+i] = "0123456789abcdef"[(number >> (28-i*4)) & 15];
    record(label); record(text);
}

static int check_thread(int worker)
{
    record(worker ? "CALL worker_resolve\n" : "CALL main_resolve\n");
    uint32_t *seed = libc_tls_address(&seed_index);
    value(worker ? "worker_address" : "main_address", (uintptr_t)seed);
    value("registered_module", seed_index.module);
    if (!seed || (uintptr_t)seed % 4) return 1;
    if (*seed != probe_tls_seed) return 2;
    uint32_t *marker = libc_tls_address(&marker_index);
    if (marker != seed + 1 || *marker) return 3;
    if (worker && (uintptr_t)seed == main_address) return 4;
    if (!worker) main_address = (uintptr_t)seed;
    const uint32_t expected = worker ? 0x33333333 : 0x77777777;
    *marker = expected;
    for (unsigned i = 0; i < 32; ++i) {
        if (sceKernelDelayThread(1000) < 0) return 5;
        if (libc_tls_address(&seed_index) != seed || libc_tls_address(&marker_index) != marker)
            return 6;
        if (*seed != probe_tls_seed || *marker != expected) return 7;
    }
    record(worker ? "PASS worker_libc_tls\n" : "PASS main_libc_tls\n");
    return 0;
}

static int worker_entry(SceSize args, void *argp)
{
    (void)args; (void)argp;
    int result = check_thread(1);
    if (result) value("FAIL worker", result);
    return result;
}

int module_start(SceSize args, void *argp)
{
    (void)args; (void)argp;
    report = sceIoOpen("app0:/libc-tls-probe.txt", SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC, 0600);
    if (report < 0) { sceKernelExitProcess(20); return 20; }
    record("libc_tls_probe v1\n");
    int status = check_thread(0);
    if (status) { value("FAIL main", status); goto done; }
    SceUID thread = sceKernelCreateThread("libc_tls_worker", worker_entry, 0x10000100, 16384, 0, 0, NULL);
    if (thread < 0) { value("FAIL create", thread); status = 21; goto done; }
    int result = sceKernelStartThread(thread, 0, NULL);
    if (result < 0) { value("FAIL start", result); sceKernelDeleteThread(thread); status = 22; goto done; }
    int worker_status = -1;
    result = sceKernelWaitThreadEnd(thread, &worker_status, NULL);
    if (result < 0) {
        value("FAIL wait", result); record("FAIL libc_tls_probe: join unverified; close title\n");
        sceIoClose(report); report = -1;
        for (;;) sceKernelDelayThread(1000000);
    }
    result = sceKernelDeleteThread(thread);
    if (result < 0 || worker_status) { value("FAIL worker_status", worker_status); status = 23; goto done; }
    uint32_t *seed = libc_tls_address(&seed_index);
    if ((uintptr_t)seed != main_address || seed[0] != probe_tls_seed || seed[1] != 0x77777777) {
        status = 24; goto done;
    }
    record("PASS libc_thread_isolation\n");
done:
    if (report_failed) status = 25;
    record(status ? "FAIL libc_tls_probe\n" : "PASS libc_tls_probe\n");
    if (sceIoClose(report) < 0 || report_failed) status = 25;
    sceKernelExitProcess(status);
    return status;
}
