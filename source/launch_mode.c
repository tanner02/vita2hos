#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "launch_mode.h"
int launch_mode_file(const char *path,bool *dependencies_only)
{
    if(!path || !dependencies_only)return -1;
    FILE *file=fopen(path,"rb");
    if(!file){if(errno==ENOENT){*dependencies_only=false;return 0;}return -1;}
    char text[33];size_t size=fread(text,1,32,file);
    int failed=ferror(file);int extra=fgetc(file);
    if(ferror(file))failed=1;
    if(fclose(file))failed=1;
    if(failed || extra!=EOF || memchr(text,0,size))return -1;
    if(size && text[size-1]=='\n'){--size;if(size && text[size-1]=='\r')--size;}
    text[size]=0;
    bool mode;
    if(!strcmp(text,"normal"))mode=false;
    else if(!strcmp(text,"dependencies-only"))mode=true;
    else return -1;
    *dependencies_only=mode;return 0;
}
