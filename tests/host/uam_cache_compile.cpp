#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <unistd.h>

unsigned test_compilations,test_outputs;
int test_output_mode;
uint32_t test_code_size=8;
std::string test_output_path,cache_directory,cache_revision="backend-A";
std::atomic<unsigned> active_compilers{0},overlapping_compilers{0};
void test_compiler_enter()
{
    if(active_compilers.fetch_add(1))++overlapping_compilers;
    for(unsigned i=0;i<100;i++)std::this_thread::yield();
}
void test_compiler_leave(){--active_compilers;}
#define UAM_TEMP_FILE_TEMPLATE "/tmp/vita2hos-uam-cache-output-XXXXXX"
#ifndef UAM_CAPTURE_PREFIX
#define UAM_CAPTURE_PREFIX "/tmp/vita2hos-uam-cache-test-pending"
#endif
#define UAM_CACHE_DIRECTORY cache_directory.c_str()
#define UAM_CACHE_REVISION cache_revision.c_str()
#include "../../source/uam_compiler_iface_c.cpp"

static std::string vertex_entry()
{
    DIR *directory=opendir(cache_directory.c_str());assert(directory);
    std::string path;dirent *entry;
    while((entry=readdir(directory)))if(!strncmp(entry->d_name,"00000000-",9))
        path=cache_directory+"/"+entry->d_name;
    assert(!closedir(directory)&&!path.empty());return path;
}
int main()
{
    char root[]="/tmp/vita2hos-uam-cache-integration-XXXXXX";assert(mkdtemp(root));
    cache_directory=std::string(root)+"/cache";
    const std::string working_directory=cache_directory;
    assert(access(UAM_CAPTURE_PREFIX ".enabled",F_OK));
    char buffer[64];memset(buffer,0x55,sizeof(buffer));uint32_t size=0;
    assert(uam_compiler_compile_glsl(pipeline_stage_vertex,"valid",buffer,64,&size));
    assert(size==32&&test_compilations==1&&test_outputs==1);
    char expected[32];memcpy(expected,buffer,32);
    assert(uam_compiler_compile_glsl(pipeline_stage_vertex,"valid",buffer,64,&size));
    assert(size==32&&test_compilations==1&&test_outputs==1&&!memcmp(buffer,expected,32));
    for(unsigned i=32;i<64;i++)assert(buffer[i]==0x55);
    memset(buffer,0x55,sizeof(buffer));
    assert(!uam_compiler_compile_glsl(pipeline_stage_vertex,"valid",buffer,31,&size)&&!size);
    for(char byte:buffer)assert(byte==0x55);
    unsigned calls=test_compilations;
    assert(!uam_compiler_compile_glsl(pipeline_stage_vertex,"valid",buffer,0,&size));
    assert(test_compilations==calls);
    void *data=(void *)1;
    assert(uam_compiler_compile_glsl_alloc(pipeline_stage_vertex,"valid",&data,&size));
    assert(size==32&&data&&!memcmp(data,expected,32)&&test_compilations==calls);free(data);
    assert(uam_compiler_compile_glsl(pipeline_stage_fragment,"valid",buffer,64,&size));
    assert(test_compilations==++calls);
    cache_revision="backend-B";
    assert(uam_compiler_compile_glsl(pipeline_stage_vertex,"valid",buffer,64,&size));
    assert(test_compilations==++calls);
    cache_revision="backend-A";
    assert(uam_compiler_compile_glsl(pipeline_stage_vertex,"valid",buffer,64,&size));
    assert(test_compilations==++calls);
    {std::ofstream marker(UAM_CAPTURE_PREFIX ".enabled");assert(marker.good());}
    assert(uam_compiler_compile_glsl(pipeline_stage_vertex,"valid",buffer,64,&size));
    assert(test_compilations==++calls);
    assert(access(UAM_CAPTURE_PREFIX ".glsl",F_OK)&&access(UAM_CAPTURE_PREFIX ".txt",F_OK));
    assert(!unlink(UAM_CAPTURE_PREFIX ".enabled"));
    std::string cached=vertex_entry();FILE *file=fopen(cached.c_str(),"r+b");assert(file);
    assert(!fseek(file,-1,SEEK_END));int byte=fgetc(file);assert(byte!=EOF);
    assert(!fseek(file,-1,SEEK_END)&&fputc(byte^0x80,file)!=EOF&&!fclose(file));
    test_output_mode=2;memset(buffer,0x55,sizeof(buffer));
    assert(!uam_compiler_compile_glsl(pipeline_stage_vertex,"valid",buffer,64,&size)&&!size);
    assert(test_compilations==++calls);for(char untouched:buffer)assert(untouched==0x55);
    test_output_mode=0;
    assert(uam_compiler_compile_glsl(pipeline_stage_vertex,"valid",buffer,64,&size));
    assert(test_compilations==++calls);
    assert(uam_compiler_compile_glsl(pipeline_stage_vertex,"valid",buffer,64,&size));
    assert(test_compilations==calls);
    for(unsigned i=0;i<2;i++) {
        data=(void *)1;
        assert(!uam_compiler_compile_glsl_alloc(pipeline_stage_vertex,"invalid",&data,&size)&&!data&&!size);
        assert(test_compilations==++calls);
    }
    cache_directory=std::string(root)+"/blocked";
    {std::ofstream blocker(cache_directory);blocker<<"keep";}
    for(unsigned i=0;i<2;i++) {
        assert(uam_compiler_compile_glsl(pipeline_stage_vertex,"valid",buffer,64,&size));
        assert(test_compilations==++calls);
    }
    assert(!unlink(cache_directory.c_str()));cache_directory=working_directory;
    cache_revision="concurrent-backend";
    std::vector<std::thread> callers;
    for(unsigned i=0;i<8;i++)callers.emplace_back([&] {
        void *output=nullptr;uint32_t bytes=0;
        assert(uam_compiler_compile_glsl_alloc(pipeline_stage_vertex,"valid",&output,&bytes));
        assert(bytes==32&&!memcmp(output,expected,32));free(output);
    });
    for(auto &caller:callers)caller.join();
    assert(test_compilations==calls+1&&!active_compilers&&!overlapping_compilers);
    DIR *directory=opendir(cache_directory.c_str());assert(directory);dirent *entry;
    while((entry=readdir(directory)))if(strcmp(entry->d_name,".")&&strcmp(entry->d_name,"..")) {
        std::string path=cache_directory+"/"+entry->d_name;assert(!unlink(path.c_str()));
    }
    assert(!closedir(directory)&&!rmdir(cache_directory.c_str())&&!rmdir(root));
    puts("Compiler cache: cold/hit allocation contracts, capacity, stage/revision, diagnostic bypass, corruption/IO fallback and concurrent cold callers passed.");
}
