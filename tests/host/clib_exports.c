#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
static bool fail_format;
static int checked_vsnprintf(char *dst,size_t size,const char *format,va_list args)
{ return fail_format ? -1 : vsnprintf(dst,size,format,args); }
#define vsnprintf checked_vsnprintf
#include "../../source/modules/SceClib.c"
#undef vsnprintf
#include "../../source/module.c"
DECLARE_LIBRARY(SceLibKernel,0xcae9ace6);

static int (*format_v)(char *,SceSize,const char *,va_list);
static int via_va_list(char *dst,SceSize size,const char *format,...)
{
    va_list args;va_start(args,format);int result=format_v(dst,size,format,args);va_end(args);return result;
}
static void formats(int (*format)(char *,SceSize,const char *,...))
{
    struct { char text[64];uint32_t guard; } buffer={.guard=0x13579bdf};
    assert(format(buffer.text,64,"%s:%d:%08x","file",-42,0xabcd)==17);
    assert(!strcmp(buffer.text,"file:-42:0000abcd"));
    assert(format(buffer.text,4,"%s","abcdef")==6 && !strcmp(buffer.text,"abc"));
    memset(buffer.text,0x5a,sizeof(buffer.text));
    assert(format(buffer.text,1,"%s","abc")==3 && !buffer.text[0] && buffer.text[1]==0x5a);
    buffer.text[0]='Z';assert(format(buffer.text,0,"%s","abc")==3 && buffer.text[0]=='Z');
    assert(format(NULL,0,"%s","abc")==3);
    assert(format(buffer.text,64,"%s","")==0 && !buffer.text[0]);
    assert(format(buffer.text,64,"%.0d",0)==0 && !buffer.text[0]);
    assert(format(buffer.text,64,"%*.*s %lld %u",6,3,"abcdef",-1234567890123LL,7u)==23);
    assert(!strcmp(buffer.text,"   abc -1234567890123 7"));
    assert(format(buffer.text,64,"%.2f %d",3.14159,42)==7 && !strcmp(buffer.text,"3.14 42"));
    assert(buffer.guard==0x13579bdf);
}
int main(void)
{
    int (*format)(char *,SceSize,const char *,...)=(void *)module_get_func_export(0xcae9ace6,0x8CBA03D5);
    format_v=(void *)module_get_func_export(0xcae9ace6,0xFA6BE467);assert(format && format_v);
    formats(format);formats(via_va_list);
    int (*compare)(const void *,const void *,size_t)=(void *)module_get_func_export(0xcae9ace6,0x9CC2BFDF);
    int (*compare_n)(const char *,const char *,size_t)=(void *)module_get_func_export(0xcae9ace6,0x660D1F6D);
    char *(*copy)(char *,const char *,size_t)=(void *)module_get_func_export(0xcae9ace6,0xC458D60A);
    char *(*last)(const char *,int)=(void *)module_get_func_export(0xcae9ace6,0x6E728AAE);
    int (*insensitive)(const char *,const char *,SceSize)=(void *)module_get_func_export(0xcae9ace6,0xB54C0BE4);
    assert(compare && compare_n && copy && last && insensitive);
    unsigned char high[]={0xff},low[]={0x7f};assert(compare(high,low,1)>0);
    assert(!compare_n("abx","aby",2) && compare_n("abx","aby",3)<0);
    char padded[8];memset(padded,'!',sizeof(padded));assert(copy(padded,"ab",5)==padded);
    assert(padded[0]=='a' && padded[1]=='b' && !padded[2] && !padded[4] && padded[5]=='!');
    const char path[]="a/b/c";assert(last(path,'/')==path+3 && last(path,0)==path+5);
    assert(!insensitive("AbC","aBc",3) && !insensitive("X","Y",0));
    assert(insensitive("aB","Ac",2)<0 && insensitive("\xff","\x7f",1)>0);
    char guard[4]="abc";
    assert((uint32_t)format(guard,4,NULL)==SCE_KERNEL_ERROR_ILLEGAL_ADDR && !strcmp(guard,"abc"));
    assert((uint32_t)format(NULL,1,"x")==SCE_KERNEL_ERROR_ILLEGAL_ADDR);
    fail_format=true;
    assert((uint32_t)format(guard,4,"x")==SCE_KERNEL_ERROR_INVALID_ARGUMENT && !strcmp(guard,"abc"));
    assert((uint32_t)via_va_list(guard,4,"x")==SCE_KERNEL_ERROR_INVALID_ARGUMENT);
    puts("Clib imports: strings, unsigned comparisons, bounded formatting, va_list and truncation passed.");
}
