#define TEST_FIXED_MEMORY
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../../source/load_module_paths.c"

static int parse(LoadModulePaths *paths, const char *text, size_t size, const char *root)
{
    FILE *file=tmpfile(); assert(file);
    assert(fwrite(text, 1, size, file) == size); rewind(file);
    int result=load_module_paths_read(paths, file, root);
    assert(fclose(file) == 0);
    return result;
}
int main(void)
{
    LoadModulePaths paths;
    const char *good=" # Startup order\r\n\n sce_module/libc.suprx \r\n\tsce_module/libfios2.suprx\n";
    assert(parse(&paths, good, strlen(good), "/app0") == 0);
    assert(paths.count == 2 && paths.present);
    assert(!strcmp(paths.paths[0], "/app0/sce_module/libc.suprx"));
    assert(!strcmp(paths.paths[1], "/app0/sce_module/libfios2.suprx"));
    good="libult.suprx";
    assert(parse(&paths, good, strlen(good), "/app0/") == 0 && paths.count == 1);
    assert(!strcmp(paths.paths[0], "/app0/libult.suprx"));
    const char *bad[]={"../libc.suprx", "/libc.suprx", "a/../libc.suprx", "a/./libc.suprx",
        "a//libc.suprx", "app0:libc.suprx", "a\\libc.suprx", "a.txt",
        "a.suprx\na.suprx", "a.suprx\n../bad.suprx", "a\001.suprx"};
    for (size_t i=0; i<sizeof(bad)/sizeof(*bad); ++i) {
        assert(parse(&paths, bad[i], strlen(bad[i]), "/app0") < 0);
        assert(paths.count == 0);
    }
    const char embedded[]="a.suprx\0b.suprx";
    assert(parse(&paths, embedded, sizeof(embedded)-1, "/app0") < 0 && !paths.count);
    char long_line[LOAD_MODULE_PATH_SIZE+1]; memset(long_line, 'a', sizeof(long_line));
    assert(parse(&paths, long_line, sizeof(long_line), "/app0") < 0);
    char long_root[LOAD_MODULE_PATH_SIZE]; memset(long_root, 'a', sizeof(long_root)-1);
    long_root[sizeof(long_root)-1]=0;
    assert(parse(&paths, good, strlen(good), long_root) < 0);
    FILE *file=tmpfile(); assert(file);
    for (int i=0; i<LOAD_PROCESS_MAX_MODULES+1; ++i) fprintf(file, "module%d.suprx\n", i);
    rewind(file);
    assert(load_module_paths_read(&paths, file, "/app0") < 0 && !paths.count);
    fclose(file);
    assert(parse(&paths, "", 0, "/app0") == 0 && !paths.count);
    assert(paths.present);
    assert(load_module_paths_file(&paths, "/dev/null/not-a-directory", "/app0") < 0);
    assert(load_module_paths_file(&paths, "/dev/null", "/app0") == 0 && !paths.count);
    char missing[]="/tmp/vita2hos-modules-XXXXXX";
    int descriptor=mkstemp(missing); assert(descriptor >= 0);
    assert(close(descriptor) == 0 && unlink(missing) == 0);
    assert(load_module_paths_file(&paths, missing, "/app0") == 0 && !paths.count);
    assert(!paths.present);
    char root[]="/tmp/vita2hos-preloads-XXXXXX";
    assert(mkdtemp(root));
    char directory[1024], libc[1024], fios[1024];
    snprintf(directory,sizeof(directory),"%s/sce_module",root);
    snprintf(libc,sizeof(libc),"%s/sce_module/libc.suprx",root);
    snprintf(fios,sizeof(fios),"%s/sce_module/libfios2.suprx",root);
    assert(mkdir(directory,0700)==0);
    LoadProcessParameters params={.version=5};
    assert(load_module_paths_preloads(&paths,root,&params)==0 && !paths.count);
    file=fopen(libc,"wb");assert(file && fclose(file)==0);
    file=fopen(fios,"wb");assert(file && fclose(file)==0);
    assert(load_module_paths_preloads(&paths,root,&params)==0 && paths.count==2);
    assert(!paths.present && !strcmp(paths.paths[0],libc) && !strcmp(paths.paths[1],fios));
    params.has_preload_disabled=true;
    params.preload_disabled=0x10000;
    assert(load_module_paths_preloads(&paths,root,&params)==0 && paths.count==1);
    assert(!strcmp(paths.paths[0],fios));
    params.preload_disabled=0x200000;
    assert(load_module_paths_preloads(&paths,root,&params)==0 && paths.count==1);
    assert(!strcmp(paths.paths[0],libc));
    params.preload_disabled=0x210000;
    assert(load_module_paths_preloads(&paths,root,&params)==0 && !paths.count);
    params.version=0;params.preload_disabled=0;
    assert(load_module_paths_preloads(&paths,root,&params)==0 && !paths.count);
    params.version=6;
    assert(load_module_paths_preloads(&paths,root,&params)==0 && paths.count==2);
    assert(unlink(fios)==0 && mkdir(fios,0700)==0);
    assert(load_module_paths_preloads(&paths,root,&params)<0 && !paths.count);
    assert(rmdir(fios)==0 && unlink(libc)==0 && rmdir(directory)==0);
    file=fopen(directory,"wb");assert(file && fclose(file)==0);
    assert(load_module_paths_preloads(&paths,root,&params)<0 && !paths.count);
    assert(unlink(directory)==0 && rmdir(root)==0);
    assert(load_module_paths_preloads(&paths,long_root,&params)<0 && !paths.count);
    puts("Ordered guest module path parsing and failure checks passed.");
}
