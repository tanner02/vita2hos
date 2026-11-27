#include <psp2/io/fcntl.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <stdint.h>
#include <stddef.h>

/* build.py encodes this owned initialization word as a 4/16-byte Vita TLS
 * template. No compiler TLS helpers or libc startup are used by the probe. */
const uint32_t probe_tls_seed __attribute__((used)) = 0x13579bdf;
extern void *probe_read_tp_arm(void);
extern void *probe_read_tp_thumb(void);
static SceUID report = -1;
static int report_failed;
static volatile uintptr_t main_tls;

static void record(const char *message)
{
    unsigned size=0;
    while (message[size]) ++size;
    if (report >= 0 && sceIoWrite(report,message,size)!=(int)size) report_failed=1;
}
static void result(const char *message,uint32_t value)
{
    char hex[13]=" 0x00000000\n";
    for (unsigned i=0;i<8;++i) hex[3+i]="0123456789abcdef"[(value>>(28-i*4))&15];
    record(message);record(hex);
}
static int check_thread(int worker)
{
    void *kernel=sceKernelGetTLSAddr(0);
    if (!kernel) return 1;
    uintptr_t expected=(uintptr_t)kernel+0x800;
    void *arm=probe_read_tp_arm(), *thumb=probe_read_tp_thumb();
    result(worker ? "worker_tp" : "main_tp",(uintptr_t)arm);
    /* Check ownership before reading or writing memory through a thread pointer.
     * An absent/stale sidecar must fail instead of touching Horizon TLS. */
    if ((uintptr_t)arm!=expected || (uintptr_t)thumb!=expected) return 2;
    if (worker && expected==main_tls) return 3;
    uint32_t *tls=(uint32_t *)expected;
    if (tls[0]!=0x13579bdf) return 4;
    if (tls[1] || tls[2] || tls[3]) return 5;
    tls[1]=worker ? 0x33333333 : 0x77777777;
    if (!worker) main_tls=expected;
    for (unsigned i=0;i<32;++i) {
        if (sceKernelDelayThread(1000)<0) return 6;
        if ((uintptr_t)probe_read_tp_arm()!=expected ||
            (uintptr_t)probe_read_tp_thumb()!=expected) return 7;
        if (tls[0]!=0x13579bdf || tls[1]!=(worker ? 0x33333333u : 0x77777777u)) return 8;
    }
    record(worker ? "PASS worker_tls\n" : "PASS main_tls\n");
    return 0;
}
static int worker_entry(SceSize args,void *argp)
{
    (void)args;(void)argp;
    int status=check_thread(1);
    if (status) result("FAIL worker",status);
    return status; /* Exercises register restoration on normal guest return. */
}
int module_start(SceSize args,void *argp)
{
    (void)args;(void)argp;
    report=sceIoOpen("app0:/tls-probe.txt",SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0600);
    if (report<0) { sceKernelExitProcess(20);return 20; }
    record("tls_probe v1\n");
    int status=check_thread(0);
    if (status) { result("FAIL main",status);goto done; }
    SceUID thread=sceKernelCreateThread("tls_probe_worker",worker_entry,0x10000100,16384,0,0,NULL);
    if (thread<0) { result("FAIL create",thread);status=21;goto done; }
    int operation=sceKernelStartThread(thread,0,NULL);
    if (operation<0) { result("FAIL start",operation);sceKernelDeleteThread(thread);status=22;goto done; }
    int worker_status=-1;
    operation=sceKernelWaitThreadEnd(thread,&worker_status,NULL);
    if (operation<0) {
        result("FAIL wait",operation);record("FAIL tls_probe: join unverified; close title\n");
        sceIoClose(report);report=-1;
        /* Do not request image teardown without a verified worker join. */
        for (;;) sceKernelDelayThread(1000000);
    }
    operation=sceKernelDeleteThread(thread);
    if (operation<0 || worker_status) { result("FAIL worker_status",worker_status);status=24;goto done; }
    if ((uintptr_t)probe_read_tp_arm()!=main_tls ||
        (uintptr_t)probe_read_tp_thumb()!=main_tls || ((uint32_t *)main_tls)[1]!=0x77777777) {
        status=25;goto done;
    }
    record("PASS thread_isolation\n");
done:
    if (report_failed) status=26;
    record(status ? "FAIL tls_probe\n" : "PASS tls_probe\n");
    if (sceIoClose(report)<0 || report_failed) status=26;
    sceKernelExitProcess(status);
    return status;
}
