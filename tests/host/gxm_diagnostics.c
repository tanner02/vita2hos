#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../../source/gxm_diagnostics.c"
#include "../../source/module.c"
DECLARE_LIBRARY(SceGxm,0xf76b66bd);
static char output[256];
static unsigned calls;
void log_printf(const char *format,...)
{ va_list args;va_start(args,format);vsnprintf(output,sizeof(output),format,args);va_end(args);++calls; }
int main(void)
{
    int (*push)(SceGxmContext *,const char *)=(void *)module_get_func_export(0xf76b66bd,0x3276C475);
    int (*set)(SceGxmContext *,const char *)=(void *)module_get_func_export(0xf76b66bd,0xC7A8CB77);
    int (*pop)(SceGxmContext *)=(void *)module_get_func_export(0xf76b66bd,0x4FA073A6);
    int (*heartbeat)(const SceGxmColorSurface *,SceGxmSyncObject *)=(void *)module_get_func_export(0xf76b66bd,0x3D25FCE9);
    assert(push && set && pop && heartbeat);
    SceGxmContext *context=(void *)0x1000;
    assert((uint32_t)push(NULL,"tag")==SCE_GXM_ERROR_INVALID_POINTER);
    assert((uint32_t)set(context,NULL)==SCE_GXM_ERROR_INVALID_POINTER);
    assert((uint32_t)pop(NULL)==SCE_GXM_ERROR_INVALID_POINTER);assert(!calls);
    assert(!push(context,"frame%08x"));assert(strstr(output,"operation=push tag=frame%08x"));
    char tag[63];memset(tag,'x',sizeof(tag));
    assert(!set(context,tag));char *value=strstr(output,"tag=");assert(value && strlen(value+4)==63);
    assert(!pop(context) && strstr(output,"operation=pop"));
    assert(!set(context,""));assert(calls==4);
    SceGxmColorSurface surface;memset(&surface,0xab,sizeof(surface));SceGxmColorSurface copy=surface;
    unsigned char sync[64],prior[64];memset(sync,0xcd,sizeof(sync));memcpy(prior,sync,sizeof(sync));
    assert((uint32_t)heartbeat(NULL,(void *)sync)==SCE_GXM_ERROR_INVALID_POINTER);
    assert((uint32_t)heartbeat(&surface,NULL)==SCE_GXM_ERROR_INVALID_POINTER);
    assert(!heartbeat(&surface,(void *)sync));
    assert(!memcmp(&surface,&copy,sizeof(surface)) && !memcmp(sync,prior,sizeof(sync)) && calls==4);
    puts("GXM diagnostics: NIDs, bounded literal tags, annotations and side-effect-free heartbeat passed.");
}
