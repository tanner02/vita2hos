#define TEST_THREAD_RUNTIME
#include <setjmp.h>
#include <assert.h>
#include <stdio.h>

#include "../../source/guest_tls.c"
#include "../../source/modules/SceKernelThreadMgr.c"

static jmp_buf thread_return;
static void *tls_value;
static Result start_result, wait_result;
static UEvent exit_event;
static int process_exit_status;
static uintptr_t writable_tp;
static unsigned tp_exchanges;
static bool expect_guest_tp;
uintptr_t guest_tp_exchange(uintptr_t value)
{
    uintptr_t previous=writable_tp;writable_tp=value;++tp_exchanges;return previous;
}
void log_printf(const char *format, ...) { (void)format; }
SceUID SceSysmem_get_next_uid(void) { static SceUID next = 1; return next++; }
UEvent *SceLibKernel_get_process_exit_uevent(void) { return &exit_event; }
int SceLibKernel_get_process_exit_res(void) { return process_exit_status; }
Result threadCreate(Thread *thread, void (*entry)(void *), void *arg, void *stack,
                    size_t size, int priority, int cpu)
{
    (void)stack; (void)size; (void)priority; (void)cpu;
    thread->entry = entry;
    thread->argument = arg;
    return 0;
}
Result threadStart(Thread *thread) { (void)thread; return start_result; }
Result threadClose(Thread *thread) { (void)thread; return 0; }
int threadTlsAlloc(void (*destructor)(void *)) { (void)destructor; return 1; }
void threadTlsFree(int slot) { (void)slot; }
void threadTlsSet(int slot, void *value) { (void)slot; tls_value = value; }
void *threadTlsGet(int slot) { (void)slot; return tls_value; }
Result waitSingle(Waiter waiter, uint64_t ns) { (void)waiter; (void)ns; return wait_result; }
void svcSleepThread(s64 ns) { (void)ns; }
void threadExit(void) { longjmp(thread_return, 1); }

static int entry(SceSize size, void *argument)
{
    assert(size == 4 && memcmp(argument, "test", 4) == 0);
    if (expect_guest_tp)
        assert(writable_tp == (uintptr_t)SceKernelThreadMgr_get_thread_info()->vita_user_tls);
    return 1234;
}

int main(void)
{
    GuestTlsTemplate tls={0}; uint32_t seed=0x12345678;
    assert(guest_tls_template_create(&tls, &seed, 4, 16) == 0);
    seed=0; /* Threads use the immutable snapshot, not mutable source storage. */
    assert(SceKernelThreadMgr_init(&tls, false) == 0);
    assert((uint32_t)sceKernelGetThreadCurrentPriority() == SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    assert(resolve_vita_priority(SCE_KERNEL_DEFAULT_PRIORITY) == 160);
    assert(resolve_vita_priority(SCE_KERNEL_DEFAULT_PRIORITY + 3) == 163);
    assert(vita_priority_to_hos_priority(64) == vita_priority_to_hos_priority(65));
    assert((uint32_t)resolve_vita_priority(63) == SCE_KERNEL_ERROR_ILLEGAL_PRIORITY);
    assert((uint32_t)sceKernelStartThread(-1, 0, NULL) == SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    assert((uint32_t)sceKernelDeleteThread(-1) == SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    assert(sceKernelGetThreadTLSAddr(-1, 0) == NULL);
    assert((uint32_t)create_thread(NULL, entry, 160, 4096) == SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    SceUID uid = create_thread("worker", entry, SCE_KERNEL_DEFAULT_PRIORITY, 4096);
    assert(uid > 0);
    VitaThreadInfo *info = get_thread_info_for_uid(uid);
    assert(info->priority == 160);
    for (unsigned int i = 0; i < SCE_KERNEL_TLS_SIZE; i++)
        assert(((unsigned char *)info->vita_tls)[i] == 0);
    assert(info->vita_user_tls == (char *)info->vita_tls + GUEST_KERNEL_TLS_SIZE);
    assert(*(uint32_t *)info->vita_user_tls == 0x12345678);
    for (int i=4; i<16; ++i) assert(((unsigned char *)info->vita_user_tls)[i] == 0);
    assert(sceKernelGetThreadTLSAddr(uid, 0x100) == (char *)info->vita_tls + 0x400);
    *(uint32_t *)info->vita_user_tls=99;
    SceUID other=create_thread("independent TLS", entry, 160, 4096);
    assert(other > 0);
    assert(*(uint32_t *)get_thread_info_for_uid(other)->vita_user_tls == 0x12345678);
    assert(sceKernelDeleteThread(other) == 0);
    assert((uint32_t)start_thread(uid, 4, NULL) == SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    start_result = 1;
    assert((uint32_t)start_thread(uid, 4, "test") == SCE_KERNEL_ERROR_THREAD_ERROR);
    assert(info->argp == NULL && info->arglen == 0);
    start_result = 0;
    assert(start_thread(uid, 4, "test") == 0);
    void *saved_arguments = info->argp;
    assert((uint32_t)start_thread(uid, 4, "oops") == SCE_KERNEL_ERROR_NOT_DORMANT);
    assert(info->argp == saved_arguments && memcmp(info->argp, "test", 4) == 0);
    if (setjmp(thread_return) == 0)
        info->thread.entry(info->thread.argument);
    assert(info->return_status == 1234);
    assert(tp_exchanges == 0);
    assert(sceKernelGetThreadCurrentPriority() == 160);
    int status = -1;
    assert(sceKernelWaitThreadEnd(uid, &status, NULL) == 0 && status == 1234);
    wait_result = 0xEA01;
    SceUInt timeout = 1;
    assert((uint32_t)sceKernelWaitThreadEnd(uid, NULL, &timeout) == SCE_KERNEL_ERROR_WAIT_TIMEOUT);
    wait_result = 0;
    assert(sceKernelDeleteThread(uid) == 0);
    assert((uint32_t)sceKernelWaitThreadEnd(uid, NULL, NULL) == SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID);
    /* The event wait and thread join succeeding do not mean the guest passed.
     * Preserve the status set by sceKernelExitProcess, including negative values. */
    const int exits[] = {0, 1, -7};
    for (unsigned i = 0; i < sizeof(exits) / sizeof(exits[0]); ++i) {
        process_exit_status = exits[i];
        assert(SceKernelThreadMgr_main_entry(entry, 0, NULL) == exits[i]);
    }
    start_result = 1;
    assert((uint32_t)SceKernelThreadMgr_main_entry(entry, 0, NULL) == SCE_KERNEL_ERROR_THREAD_ERROR);
    assert(SceKernelThreadMgr_finish() == 0);
    assert(SceKernelThreadMgr_init(&tls, true) == 0);
    start_result=0;writable_tp=0x12345678;expect_guest_tp=true;
    uid=create_thread("retargeted TLS", entry, 160, 4096);assert(uid > 0);
    info=get_thread_info_for_uid(uid);
    assert(start_thread(uid, 4, "test") == 0);
    if (setjmp(thread_return) == 0) info->thread.entry(info->thread.argument);
    assert(tp_exchanges == 2 && writable_tp == 0x12345678);
    assert(sceKernelDeleteThread(uid) == 0);
    assert(SceKernelThreadMgr_finish() == 0);
    guest_tls_template_clear(&tls);
    puts("Thread runtime regression checks passed.");
    return 0;
}
