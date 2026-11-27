#include <psp2/io/fcntl.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <stdint.h>
#include <stddef.h>
const uint32_t probe_tls_seed __attribute__((used))=0x13579bdf;
extern void *probe_read_tp_arm(void);
extern void *probe_read_tp_thumb(void);
extern uint32_t dependency_value(uint32_t value);
extern volatile uint32_t dependency_state[4];
static SceUID report=-1;
static int report_failed;
static void record(const char *text)
{
    unsigned size=0;while (text[size]) ++size;
    if (report<0 || sceIoWrite(report,text,size)!=(int)size) report_failed=1;
}
int module_start(SceSize args,void *argp)
{
    report=sceIoOpen("app0:/module-probe.txt",SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0600);
    if (report<0) { sceKernelExitProcess(20);return 20; }
    record("module_probe v1\n");
    int status=1;
    void *kernel=sceKernelGetTLSAddr(0);
    uintptr_t expected=(uintptr_t)kernel+0x800;
    if (args || argp || !kernel || (uintptr_t)probe_read_tp_arm()!=expected ||
        (uintptr_t)probe_read_tp_thumb()!=expected) {
        record("FAIL main_thread_pointer\n");goto done;
    }
    uint32_t *tls=(void *)expected;
    if (tls[0]!=0x13579bdf || tls[1] || tls[2] || tls[3]) {
        record("FAIL main_tls_template\n");goto done;
    }
    record("PASS main_tls\n");
    /* The recorded child address is compared only, never dereferenced after
     * its startup thread has exited and been joined/deleted. */
    if (dependency_state[0]!=0x51a7c0de || !dependency_state[1] ||
        dependency_state[1]==(uintptr_t)kernel || dependency_state[2]!=1) {
        record("FAIL dependency_start\n");goto done;
    }
    record("PASS dependency_exit_thread_start\n");
    if (dependency_value(0x1234)!=(0x51a7c0de^0x1234)) {
        record("FAIL function_import\n");goto done;
    }
    dependency_state[3]=0x98765432;
    if (dependency_value(0)!=(0x51a7c0de^0x98765432)) {
        record("FAIL variable_import_alias\n");goto done;
    }
    record("PASS function_import\nPASS variable_import_alias\n");
    status=0;
done:
    if (report_failed) status=21;
    record(status ? "FAIL module_probe\n" : "PASS module_probe\n");
    if (sceIoClose(report)<0 || report_failed) status=22;
    sceKernelExitProcess(status);
    return status;
}
