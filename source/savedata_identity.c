#include "savedata_identity.h"
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
static uint32_t u32(const unsigned char *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint16_t u16(const unsigned char *p){return (uint16_t)p[0]|(uint16_t)p[1]<<8;}
static bool slug(const unsigned char *p,size_t count,bool title)
{
    if(!count || count>63 || (title && count!=9))return false;
    for(size_t i=0;i<count;i++) {
        unsigned char c=p[i];
        if((c>='A' && c<='Z') || (c>='0' && c<='9'))continue;
        if(!title && ((c>='a' && c<='z') || c=='_' || c=='-'))continue;
        return false;
    }
    return true;
}
int savedata_identity_parse(const void *data,size_t size,SavedataIdentity *identity)
{
    if(!data || !identity || size<20 || size>1024*1024)return -EINVAL;
    const unsigned char *p=data;
    if(memcmp(p,"\0PSF",4) || u32(p+4)!=0x101)return -EINVAL;
    uint32_t keys=u32(p+8),values=u32(p+12),count=u32(p+16);
    if(count>4096 || count>(size-20)/16 || keys<20+count*16 ||
       values<keys || values>size)return -EINVAL;
    SavedataIdentity found={0};bool has_title=false,has_directory=false;
    for(uint32_t i=0;i<count;i++) {
        const unsigned char *entry=p+20+i*16;
        uint32_t key=u16(entry),offset=u32(entry+12),length=u32(entry+4),capacity=u32(entry+8);
        if(key>=values-keys || length>capacity || offset>size-values || capacity>size-values-offset)
            return -EINVAL;
        const char *name=(const char *)p+keys+key;
        if(!memchr(name,0,values-keys-key))return -EINVAL;
        bool title=!strcmp(name,"TITLE_ID"),directory=!strcmp(name,"INSTALL_DIR_SAVEDATA");
        if(!title && !directory)continue;
        if((title && has_title) || (directory && has_directory) || u16(entry+2)!=0x204 ||
           !length || p[values+offset+length-1] ||
           !slug(p+values+offset,length-1,title))return -EINVAL;
        if(title){memcpy(found.title,p+values+offset,length);has_title=true;}
        else {memcpy(found.directory,p+values+offset,length);has_directory=true;}
    }
    if(!has_title)return -ENOENT;
    if(!has_directory)memcpy(found.directory,found.title,sizeof(found.title));
    *identity=found;return 0;
}
