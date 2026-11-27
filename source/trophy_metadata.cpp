#include <cstring>
#include <pugixml.hpp>
#include "trophy_metadata.h"
namespace {
bool number(const char *text,unsigned limit,unsigned &value)
{
    if(!text || !*text)return false;
    unsigned result=0;
    for(;*text;text++) {
        if(*text<'0' || *text>'9')return false;
        unsigned digit=static_cast<unsigned>(*text-'0');
        if(digit>limit || result>(limit-digit)/10)return false;
        result=result*10+digit;
    }
    value=result;return true;
}
const char *attribute(pugi::xml_node node,const char *name)
{
    const char *result=nullptr;
    for(auto attr:node.attributes())if(!std::strcmp(attr.name(),name)) {
        if(result)return nullptr;
        result=attr.value();
    }
    return result;
}
bool comm_id(const char *text)
{
    if(std::strlen(text)!=12 || std::memcmp(text,"NPWR",4) || text[9]!='_')return false;
    for(unsigned i=4;i<12;i++)if(i!=9 && (text[i]<'0' || text[i]>'9'))return false;
    return true;
}
}
extern "C" int trophy_metadata_parse(const void *data,size_t size,TrophyMetadata *output)
{
    if(!data || !output || !size || size>1024*1024)return TROPHY_METADATA_INVALID;
    /* Configuration support is UTF-8. Permit terminal NUL padding, but never
     * let an embedded NUL hide a second malformed document from the parser. */
    const char *begin=static_cast<const char *>(data);
    const char *zero=static_cast<const char *>(std::memchr(data,0,size));
    if(zero) {
        for(size_t i=static_cast<size_t>(zero-begin);i<size;i++)if(begin[i])return TROPHY_METADATA_INVALID;
        size=static_cast<size_t>(zero-begin);
        if(!size)return TROPHY_METADATA_INVALID;
    }
    pugi::xml_document document;
    auto result=document.load_buffer(data,size,pugi::parse_default|pugi::parse_doctype,pugi::encoding_utf8);
    if(!result)return result.status==pugi::status_out_of_memory ? TROPHY_METADATA_NO_MEMORY : TROPHY_METADATA_INVALID;
    auto root=document.document_element();
    if(std::strcmp(root.name(),"trophyconf"))return TROPHY_METADATA_INVALID;
    unsigned roots=0;
    for(auto node:document.children()) {
        if(node.type()==pugi::node_doctype)return TROPHY_METADATA_INVALID;
        if(node.type()==pugi::node_element)++roots;
    }
    if(roots!=1)return TROPHY_METADATA_INVALID;
    TrophyMetadata pending{};pending.groups=1;
    unsigned explicit_groups=0,communications=0;
    for(auto node:root.children()) {
        if(!std::strcmp(node.name(),"npcommid")) {
            const char *text=node.child_value();
            if(++communications!=1 || !comm_id(text))return TROPHY_METADATA_INVALID;
            std::memcpy(pending.communication_id,text,13);
        } else if(!std::strcmp(node.name(),"group")) {
            unsigned id;
            if(!number(attribute(node,"id"),TROPHY_METADATA_MAX_GROUPS-1,id) || (explicit_groups&(1u<<id)))
                return TROPHY_METADATA_INVALID;
            explicit_groups|=1u<<id;pending.groups|=1u<<id;
            if(id)++pending.group_count;
        } else if(!std::strcmp(node.name(),"trophy")) {
            unsigned id,group=0;
            const char *group_text=attribute(node,"gid");
            if((node.attribute("gid") && !group_text) ||
               !number(attribute(node,"id"),TROPHY_METADATA_MAX_TROPHIES-1,id) ||
               (group_text && !number(group_text,TROPHY_METADATA_MAX_GROUPS-1,group)) || pending.trophies[id].present)
                return TROPHY_METADATA_INVALID;
            const char *type=attribute(node,"ttype"),*hidden=attribute(node,"hidden");
            if(!type || !type[0] || type[1] || !hidden)return TROPHY_METADATA_INVALID;
            unsigned grade=type[0]=='P' ? 1 : type[0]=='G' ? 2 : type[0]=='S' ? 3 : type[0]=='B' ? 4 : 0;
            if(!grade || (std::strcmp(hidden,"yes") && std::strcmp(hidden,"no")))return TROPHY_METADATA_INVALID;
            pending.trophies[id]={static_cast<uint8_t>(grade),static_cast<uint8_t>(group),
                static_cast<uint8_t>(!std::strcmp(hidden,"yes")),1};
            ++pending.grade_counts[grade];++pending.trophy_count;
        }
    }
    if(communications!=1 || !pending.trophy_count || pending.grade_counts[1]>1)return TROPHY_METADATA_INVALID;
    for(const auto &trophy:pending.trophies)if(trophy.present && !(pending.groups&(1u<<trophy.group)))
        return TROPHY_METADATA_INVALID;
    *output=pending;return TROPHY_METADATA_OK;
}
