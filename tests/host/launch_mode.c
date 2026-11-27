#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../../source/launch_mode.c"
static void write_mode(const char *path,const void *text,size_t size)
{FILE *f=fopen(path,"wb");assert(f && fwrite(text,1,size,f)==size && !fclose(f));}
int main(void)
{
    char path[]="/tmp/vita2hos-launch-mode-XXXXXX";int fd=mkstemp(path);assert(fd>=0);close(fd);
    bool mode=true;
    write_mode(path,"normal\n",7);assert(!launch_mode_file(path,&mode) && !mode);
    write_mode(path,"dependencies-only\r\n",19);assert(!launch_mode_file(path,&mode) && mode);
    write_mode(path,"dependencies-only",17);assert(!launch_mode_file(path,&mode) && mode);
    const char *bad[]={"","normal extra","normal\n\n","dependencies-only ","UNKNOWN"};
    for(unsigned i=0;i<5;i++){write_mode(path,bad[i],strlen(bad[i]));mode=true;assert(launch_mode_file(path,&mode)<0 && mode);}
    char nul[]={ 'n','o','r','m','a','l',0,'x' };write_mode(path,nul,sizeof(nul));assert(launch_mode_file(path,&mode)<0 && mode);
    char long_mode[64];memset(long_mode,'x',sizeof(long_mode));write_mode(path,long_mode,sizeof(long_mode));assert(launch_mode_file(path,&mode)<0 && mode);
    assert(launch_mode_file(NULL,&mode)<0 && launch_mode_file(path,NULL)<0);
    char child[128];snprintf(child,sizeof(child),"%s/child",path);
    assert(launch_mode_file(child,&mode)<0 && mode); /* ENOTDIR is not absence. */
    unlink(path);assert(!launch_mode_file(path,&mode) && !mode);
    puts("Launch mode: explicit diagnostics, absent normal default, bounded parsing and failure preservation passed.");
}
