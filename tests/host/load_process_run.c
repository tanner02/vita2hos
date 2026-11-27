#define TEST_FIXED_MEMORY
#include <assert.h>
#include <stdio.h>
#include "../../source/load_process_run.c"
/* This unit test isolates loader ordering; the threaded integration test uses
 * the production thread runner, TLS and explicit exits. */
int SceKernelThreadMgr_run_module_start(SceKernelThreadEntry entry) { return entry(0,NULL); }


static int calls[8], call_count, failed_module, failure_result, main_result;
void log_printf(const char *format, ...) { (void)format; }
static int first(uint32_t args, void *argp)
{
    assert(!args && !argp); calls[call_count++]=1;
    return failed_module == 1 ? failure_result : 0;
}
static int second(uint32_t args, void *argp)
{
    assert(!args && !argp); calls[call_count++]=2;
    return failed_module == 2 ? failure_result : 0;
}
static int guest_main(uint32_t args, void *argp)
{
    assert(args == 4 && argp && *(int *)argp == 42);
    calls[call_count++]=3;
    return main_result;
}
static void reset(LoadProcess *process, LoadImage *images)
{
    call_count=failed_module=0; failure_result=0; main_result=123;
    for (int i=0; i<4; ++i) {
        images[i]=(LoadImage){.bound=true};
        atomic_store(&images[i].module_state,SCE_KERNEL_MODULE_STATE_READY);
    }
    images[1].lifecycle.start=first;
    /* The middle dependency deliberately has no startup function. */
    images[3].lifecycle.start=second;
    *process=(LoadProcess){.images=images, .count=4, .bound=true, .entry=guest_main};
}
int main(void)
{
    LoadProcess process; LoadImage images[4]; int argument=42;
    reset(&process, images);
    assert(load_process_run(&process, 4, &argument) == 123);
    assert(call_count == 3 && calls[0] == 1 && calls[1] == 2 && calls[2] == 3);
    assert(process.run_started && process.modules_started == 2);
    assert(atomic_load(&images[0].module_state)==SCE_KERNEL_MODULE_STATE_ENDED);
    for (int i=1;i<4;++i) assert(atomic_load(&images[i].module_state)==SCE_KERNEL_MODULE_STATE_STARTED);
    assert(load_process_run(&process, 4, &argument) == (int)SCE_KERNEL_ERROR_MODULEMGR_START_FAILED);
    assert(call_count == 3);
    int failures[]={SCE_KERNEL_START_NO_RESIDENT, SCE_KERNEL_START_FAILED, -123};
    for (int module=1; module<=2; ++module) for (int i=0; i<3; ++i) {
        reset(&process, images); failed_module=module; failure_result=failures[i];
        assert(load_process_run(&process, 4, &argument) == (int)SCE_KERNEL_ERROR_MODULEMGR_START_FAILED);
        assert(call_count == module && process.modules_started == (size_t)(module-1));
        assert(process.run_started);
        assert(atomic_load(&images[0].module_state)==SCE_KERNEL_MODULE_STATE_READY);
        assert(atomic_load(&images[module==1 ? 1 : 3].module_state)==SCE_KERNEL_MODULE_STATE_ENDED);
    }
    reset(&process, images); images[3].bound=false;
    assert(load_process_run(&process, 4, &argument) < 0 && !call_count && !process.run_started);
    reset(&process, images); process.bound=false;
    assert(load_process_run(&process, 4, &argument) < 0 && !call_count);
    reset(&process, images);
    assert(load_process_run(&process, 4, NULL) < 0 && !call_count);
    assert(load_process_run(NULL, 0, NULL) < 0);
    process.count=1; main_result=-321;
    assert(load_process_run(&process, 4, &argument) == -321 && call_count == 1);
    assert(!process.modules_started && calls[0] == 3);
    reset(&process,images);process.dependencies_only=true;
    assert(!load_process_run(&process,0,NULL));
    assert(call_count==2 && calls[0]==1 && calls[1]==2 && process.modules_started==2);
    assert(atomic_load(&images[0].module_state)==SCE_KERNEL_MODULE_STATE_READY);
    assert(load_process_run(&process,0,NULL)<0 && call_count==2);
    reset(&process,images);process.dependencies_only=true;failed_module=1;failure_result=-1;
    assert(load_process_run(&process,0,NULL)<0 && call_count==1 && !process.modules_started);
    assert(atomic_load(&images[0].module_state)==SCE_KERNEL_MODULE_STATE_READY);
    reset(&process,images);process.dependencies_only=true;process.count=1;
    assert(!load_process_run(&process,0,NULL) && !call_count && !process.modules_started);
    puts("Guest module startup ordering, failure gating and main-result checks passed.");
}
