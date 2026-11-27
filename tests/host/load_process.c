#define TEST_FIXED_MEMORY
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "../../source/module.c"
#define load_image_close close_image_impl
#define load_image_publish publish_image_impl
#include "../../source/guest_tls.c"
#include "../../source/load_image.c"
#undef load_image_close
#undef load_image_publish
Result load_image_close(LoadImage *image);
int load_image_publish(LoadImage *image, ModuleGuestExports *exports);
#include "../../source/load_process.c"
#include "../../source/load_module_paths.c"

EXPORT(TestLibrary, 1, int, test_export, void) { return 0; }
DECLARE_LIBRARY(TestLibrary, 2);
static int prepared, published, bindings, closed, total;
static int fail_prepare=-1, fail_publish=-1, fail_bind=-1, fail_close=-1;
static int close_order[128];
static bool missing_main_entry;
static LoadProcessParameters main_parameters;
static char prepared_paths[3][LOAD_MODULE_PATH_SIZE];
void log_printf(const char *format, ...) { (void)format; }
Result load_fixed_close(LoadFixedMemory *memory) { (void)memory; return 0; }
Result jitClose(Jit *jit)
{
    if (jit->unused == fail_close) { fail_close = -1; return 1; }
    return 0;
}
static int prepare_image(LoadImage *image, const char *path, bool module)
{
    int index = prepared++;
    assert(path && module == (index != 0));
    if (index < 3) snprintf(prepared_paths[index],sizeof(prepared_paths[index]),"%s",path);
    if (!index) image->parameters=main_parameters;
    assert(published == 0 && bindings == 0);
    image->source_tag = 0x1234;
    image->data = malloc(4); assert(image->data);
    image->jit_active = true; image->jit.unused = index;
    image->entry = index == 0 && !missing_main_entry ? (void *)0x81000000 : NULL;
    if (index == fail_prepare) return LOAD_LAYOUT_CONFLICT;
    image->prepared = true; image->module = module;
    image->guest_tp_retargeted = module;
    ModuleGuestSymbol symbol = {11, MODULE_SYMBOL_FUNCTION, (void *)(uintptr_t)(0x1001 + index * 4)};
    assert(module_guest_exports_register(&image->exports, image, 100 + index, &symbol, 1) == 0);
    return 0;
}
int load_file_prepare(LoadImage *image, const char *path) { return prepare_image(image, path, false); }
int load_module_prepare(LoadImage *image, const char *path) { return prepare_image(image, path, true); }
int load_image_publish(LoadImage *image, ModuleGuestExports *exports)
{
    assert(prepared == total && bindings == 0);
    if (published++ == fail_publish) return -1;
    return publish_image_impl(image, exports);
}
int load_image_bind(LoadImage *image, ModuleGuestExports *exports)
{
    assert(prepared == total && published == total);
    for (int i = 0; i < total; ++i)
        assert(module_guest_exports_find(exports, 100+i, MODULE_SYMBOL_FUNCTION, 11, NULL));
    if (bindings++ == fail_bind) return -1;
    image->bound = true;
    return 0;
}
Result load_image_close(LoadImage *image)
{
    close_order[closed++] = image->jit.unused;
    return close_image_impl(image);
}
static void reset(int count)
{
    prepared=published=bindings=closed=0; total=count;
    fail_prepare=fail_publish=fail_bind=fail_close=-1;
    missing_main_entry=false;
    memset(&main_parameters,0,sizeof(main_parameters));
}
static void empty(LoadProcess *process)
{
    assert(!process->images && !process->count && !process->exports.libraries && !process->bound && !process->entry);
}
int main(void)
{
    LoadProcess process={0}; const char *modules[]={"libc.suprx", "libfios2.suprx"};
    reset(3);
    assert(load_process_load(&process, "eboot.bin", modules, 2) == 0);
    assert(process.bound && process.count == 3 && process.entry == (void *)0x81000000);
    assert(process.source_tag == 0x1234 && bindings == 3);
    assert(process.guest_tp_retargeted);
    assert(load_process_load(&process, "eboot.bin", NULL, 0) < 0);
    assert(load_process_close(&process) == 0); empty(&process);
    assert(closed == 3 && close_order[0] == 2 && close_order[1] == 1 && close_order[2] == 0);
    for (int phase=0; phase<3; ++phase) for (int index=0; index<3; ++index) {
        reset(3);
        if (phase == 0) fail_prepare=index;
        if (phase == 1) fail_publish=index;
        if (phase == 2) fail_bind=index;
        int result=load_process_load(&process, "eboot.bin", modules, 2);
        assert(result == (phase == 0 ? LOAD_LAYOUT_CONFLICT : -1));
        assert(closed == (phase == 0 ? index+1 : 3)); empty(&process);
        assert(process.source_tag == 0x1234);
    }
    reset(3); fail_bind=1; fail_close=2;
    assert(load_process_load(&process, "eboot.bin", modules, 2) == -1);
    assert(process.images && process.count == 3 && process.images[2].jit_active);
    assert(!process.exports.libraries && !process.bound && !process.entry);
    assert(load_process_load(&process, "eboot.bin", modules, 2) < 0);
    assert(load_process_close(&process) == 0); empty(&process);
    reset(3); missing_main_entry=true;
    assert(load_process_load(&process, "eboot.bin", modules, 2) < 0);
    assert(prepared == 1 && published == 0 && bindings == 0 && closed == 1);
    empty(&process);
    reset(1);
    assert(load_process_load(&process, "eboot.bin", NULL, 0) == 0);
    assert(!process.guest_tp_retargeted);
    assert(load_process_close(&process) == 0); empty(&process);
    assert(load_process_load(&process, "eboot.bin", NULL, 1) < 0);
    assert(load_process_load(&process, "eboot.bin", modules, LOAD_PROCESS_MAX_MODULES+1) < 0);
    const char *invalid[]={NULL};
    assert(load_process_load(&process, "eboot.bin", invalid, 1) < 0);
    assert(load_process_load(&process, "", NULL, 0) < 0);
    assert(load_process_close(&process) == 0);
    char root[]="/tmp/vita2hos-process-preloads-XXXXXX";
    assert(mkdtemp(root));
    char directory[1024], libc[1024], fios[1024];
    snprintf(directory,sizeof(directory),"%s/sce_module",root);
    snprintf(libc,sizeof(libc),"%s/sce_module/libc.suprx",root);
    snprintf(fios,sizeof(fios),"%s/sce_module/libfios2.suprx",root);
    assert(mkdir(directory,0700)==0);
    FILE *file=fopen(libc,"wb");assert(file && fclose(file)==0);
    file=fopen(fios,"wb");assert(file && fclose(file)==0);
    LoadModulePaths list={0};
    reset(3);main_parameters.version=6;
    assert(load_process_load_app(&process,"eboot.bin",root,&list)==0);
    assert(process.count==3 && !strcmp(prepared_paths[1],libc) && !strcmp(prepared_paths[2],fios));
    assert(load_process_close(&process)==0);empty(&process);
    reset(2);main_parameters.version=5;main_parameters.has_preload_disabled=true;
    main_parameters.preload_disabled=0x10000;
    assert(load_process_load_app(&process,"eboot.bin",root,&list)==0);
    assert(process.count==2 && !strcmp(prepared_paths[1],fios));
    assert(load_process_close(&process)==0);empty(&process);
    reset(1); /* Legacy parameters do not trigger preloads. */
    assert(load_process_load_app(&process,"eboot.bin",root,&list)==0);
    assert(process.count==1 && load_process_close(&process)==0);
    reset(1);main_parameters.version=6;list.present=true;
    assert(load_process_load_app(&process,"eboot.bin",root,&list)==0);
    assert(process.count==1 && load_process_close(&process)==0);
    reset(2);list.count=1;strcpy(list.paths[0],"explicit.suprx");
    main_parameters.version=6;
    assert(load_process_load_app(&process,"eboot.bin",root,&list)==0);
    assert(process.count==2 && !strcmp(prepared_paths[1],"explicit.suprx"));
    assert(load_process_close(&process)==0);
    reset(3);list.present=false;list.count=0;main_parameters.version=6;fail_prepare=1;
    assert(load_process_load_app(&process,"eboot.bin",root,&list)==LOAD_LAYOUT_CONFLICT);
    assert(prepared==2 && closed==2 && published==0);empty(&process);
    assert(unlink(fios)==0 && mkdir(fios,0700)==0);
    reset(3);main_parameters.version=6;
    assert(load_process_load_app(&process,"eboot.bin",root,&list)<0);
    assert(prepared==1 && closed==1 && published==0);empty(&process);
    assert(rmdir(fios)==0 && unlink(libc)==0 && rmdir(directory)==0 && rmdir(root)==0);
    puts("Process dependency staging, publication, binding and rollback checks passed.");
}
