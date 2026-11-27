#include "config.h"

#include <arpa/inet.h>
#include <assert.h>
#include <deko3d.h>
#include <elf.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>

#include "load_process.h"
#include "load_module_paths.h"
#include "load_retry.h"
#include "log.h"
#include "module.h"
#include "netlog.h"

#include "modules/SceCtrl.h"
#include "modules/SceDisplay.h"
#include "modules/SceGxm.h"
#include "modules/SceKernelThreadMgr.h"
#include "modules/SceKernelTime.h"
#include "modules/SceLibKernel.h"
#include "modules/SceSysmem.h"
#include "modules/SceTouch.h"

static void dk_debug_callback(void *userData, const char *context, DkResult result,
                              const char *message)
{
    char description[256];

    if (result == DkResult_Success) {
        LOG("deko3d debug callback: context: %s, message: %s, result %d", context, message, result);
    } else {
        snprintf(description, sizeof(description), "context: %s, message: %s, result %d", context,
                 message, result);
        fatal_error("deko3d fatal error.", description);
    }
}

static int guest_process_entry(SceSize args, void *argp)
{
    LoadProcess *process = NULL;
    if (args == sizeof(process) && argp) memcpy(&process, argp, sizeof(process));
    int result = load_process_run(process, 0, NULL);
    /* A returning main or startup failure must also wake the process waiter.
     * A guest calling sceKernelExitProcess itself never returns here. */
    sceKernelExitProcess(result);
    return result;
}

static int launch(LoadProcess *process)
{
    DkDeviceMaker device_maker;
    DkDevice dk_device;
    int ret;

    /* Initialize deko3d device */
    dkDeviceMakerDefaults(&device_maker);
    device_maker.userData = NULL;
    device_maker.cbDebug = dk_debug_callback;
    device_maker.flags = DkDeviceFlags_YAxisPointsDown;
    dk_device = dkDeviceCreate(&device_maker);

    /* Init modules */
    SceKernelTime_start_process();
    ret = SceSysmem_init(dk_device);
    if (ret != 0)
        goto done;
    ret = SceLibKernel_init();
    if (ret != 0)
        goto done;
    ret = SceKernelThreadMgr_init(&process->images[0].tls, process->guest_tp_retargeted);
    if (ret != 0)
        goto done;
    ret = SceDisplay_init(dk_device);
    if (ret != 0)
        goto done;
    ret = SceGxm_init(dk_device);
    if (ret != 0)
        goto done;
    ret = SceCtrl_init();
    if (ret != 0)
        goto done;
    ret = SceTouch_init();
    if (ret != 0)
        goto done;

    LOG("Entering SceKernelThreadMgr...");

    ret = SceKernelThreadMgr_main_entry_configured(guest_process_entry, sizeof(process), &process,
                                                   &process->images[0].parameters);

    LOG("SceKernelThreadMgr returned...");

    int gxm_cleanup_result = SceGxm_finish();
    if (gxm_cleanup_result != 0) {
        LOG("GXM shutdown failed: 0x%x", gxm_cleanup_result);
        return gxm_cleanup_result;
    }
    SceDisplay_finish();
    SceKernelThreadMgr_finish();

    SceSysmem_finish();

    dkDeviceDestroy(dk_device);

    LOG("Process finished! Returned: %d", ret);

done:
    return ret;
}

static void create_vita2hos_paths()
{
    mkdir(VITA2HOS_ROOT_PATH, 0755);
    mkdir(VITA2HOS_DUMP_PATH, 0755);
    mkdir(VITA2HOS_DUMP_SHADER_PATH, 0755);
}

static LoadRetryPacket read_layout_retry(void)
{
    LoadRetryPacket packet = {0};
    AppletStorage storage;
    if (R_SUCCEEDED(appletPopLaunchParameter(&storage, AppletLaunchParameterKind_UserChannel))) {
        s64 size;
        if (R_FAILED(appletStorageGetSize(&storage, &size)) || size != sizeof(packet) ||
            R_FAILED(appletStorageRead(&storage, 0, &packet, sizeof(packet))))
            memset(&packet, 0, sizeof(packet));
        appletStorageClose(&storage);
    }
    return packet;
}

int main(int argc, char *argv[])
{
    time_t now;
    char timestr[32];
    LoadProcess process = {0};
    int ret;
    LoadRetryPacket previous_retry = read_layout_retry();

    /* Initialize the socket library */
    socketInitializeDefault();

    /* Initialize netlog */
    netlog_init();

    create_vita2hos_paths();
#ifndef NDEBUG
    log_init(LOG_DEVICE_SVC | LOG_DEVICE_NETLOG | LOG_DEVICE_FILE);
#else
    log_init(LOG_DEVICE_SVC | LOG_DEVICE_NETLOG);
#endif

    LOG("vita2hos " VITA2HOS_MAJOR "." VITA2HOS_MINOR "." VITA2HOS_PATCH "-" VITA2HOS_HASH
        " (" __DATE__ " " __TIME__ ")");

    now = time(NULL);
    strftime(timestr, sizeof(timestr), "%c", localtime(&now));
    LOG("Starting at %s", timestr);

    LoadModulePaths modules;
    if (load_module_paths_file(&modules, VITA2HOS_MODULES_FILE, VITA2HOS_APP0_PATH) < 0)
        fatal_error("Error reading guest module list.",
                    "Check /vita2hos/modules.txt and /vita2hos/log.txt.");
    const char *module_paths[LOAD_PROCESS_MAX_MODULES];
    for (size_t i=0; i<modules.count; ++i) module_paths[i]=modules.paths[i];
    ret = load_process_load(&process, VITA2HOS_EXE_FILE, module_paths, modules.count);
    if (ret == LOAD_LAYOUT_CONFLICT) {
        u64 program_id = 0;
        LoadRetryPacket next;
        Result result = svcGetInfo(&program_id, InfoType_ProgramId, CUR_PROCESS_HANDLE, 0);
        LOG("Layout retry host program ID: 0x%016" PRIx64 ", query result 0x%x", program_id, result);
        if (R_SUCCEEDED(result) && appletGetAppletType() == AppletType_Application &&
            load_retry_next(&previous_retry, process.source_tag, program_id, &next)) {
            LOG("Restarting vita2hos for a compatible address layout: attempt %u/%u",
                next.attempts, LOAD_LAYOUT_RETRY_LIMIT);
            result = appletRestartProgram(&next, sizeof(next));
            LOG("Address-layout restart request returned: 0x%x", result);
            if (R_SUCCEEDED(result)) return 0;
        }
        LOG("Fixed-address layout unavailable; restart count %u, request result 0x%x",
            load_retry_count(&previous_retry, process.source_tag), result);
        fatal_error("Could not reserve memory for the Vita program.",
                    "Relaunch vita2hos to try another layout. Details: /vita2hos/log.txt");
    }
    if (ret == 0) {
        LOG("Compatible layout after %u restarts", load_retry_count(&previous_retry, process.source_tag));
        LOG("Launching PlayStation Vita executable!");

        /* Jump to Vita's executable entrypoint */
        ret = launch(&process);

        LOG("Returned from launch with result: %d", ret);

        /* Release guest code and data mappings after execution has stopped. */
        ret = load_process_close(&process);
        LOG("load_process_close() returned: 0x%x", ret);
    } else {
        fatal_error("Error loading PlayStation Vita executable.",
                    "Make sure to place it to '" VITA2HOS_EXE_FILE "'");
    }

    return 0;
}

void NORETURN fatal_error(const char *dialog_message, const char *fullscreen_message)
{
    extern u32 __nx_applet_exit_mode;
    ErrorApplicationConfig c;

    LOG("%s %s", dialog_message, fullscreen_message);

    errorApplicationCreate(&c, dialog_message, fullscreen_message);
    errorApplicationShow(&c);

    __nx_applet_exit_mode = 1;
    exit(1);
}

void NORETURN __assert_func(const char *file, int line, const char *func, const char *failedexpr)
{
    char message[256];

    snprintf(message, sizeof(message), "assertion \"%s\" failed: file \"%s\", line %d%s%s\n",
             failedexpr, file, line, func ? ", function: " : "", func ? func : "");

    fatal_error("Assertion failed.", message);
}
