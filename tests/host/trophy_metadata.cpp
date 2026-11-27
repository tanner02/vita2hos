#include <cassert>
#include <cstring>
#include <string>
#include <iostream>
#include <cstdlib>
#include <pugixml.hpp>
#include "trophy_metadata.h"
static const char *valid="<trophyconf><npcommid>NPWR00001_00</npcommid><group id='001'/><trophy id='000' gid='000' ttype='P' hidden='no'/><trophy id='001' gid='001' ttype='B' hidden='yes'/></trophyconf>";
static void invalid(std::string xml)
{
    TrophyMetadata output;std::memset(&output,0xa5,sizeof(output));auto saved=output;
    assert(trophy_metadata_parse(xml.data(),xml.size(),&output)==TROPHY_METADATA_INVALID);
    assert(!std::memcmp(&output,&saved,sizeof(output)));
}
static std::string change(const char *from,const char *to)
{
    std::string xml=valid;auto pos=xml.find(from);assert(pos!=std::string::npos);xml.replace(pos,std::strlen(from),to);return xml;
}
static void *no_memory(size_t){return nullptr;}
int main()
{
    TrophyMetadata metadata{};assert(!trophy_metadata_parse(valid,std::strlen(valid),&metadata));
    assert(!std::strcmp(metadata.communication_id,"NPWR00001_00"));
    assert(metadata.trophy_count==2 && metadata.group_count==1 && metadata.groups==3);
    assert(metadata.grade_counts[1]==1 && metadata.grade_counts[4]==1);
    assert(metadata.trophies[0].present && metadata.trophies[0].grade==1);
    assert(metadata.trophies[1].hidden && metadata.trophies[1].group==1);
    auto base_without_gid=change(" gid='000'", "");
    assert(!trophy_metadata_parse(base_without_gid.data(),base_without_gid.size(),&metadata) && metadata.trophies[0].group==0);
    invalid(change("gid='001'", "gid='001' gid='002'"));
    auto padded=std::string(valid)+std::string(4,'\0');
    assert(!trophy_metadata_parse(padded.data(),padded.size(),&metadata));
    invalid(std::string(valid)+std::string(1,'\0')+"bad");
    auto old_allocate=pugi::get_memory_allocation_function();
    auto old_deallocate=pugi::get_memory_deallocation_function();
    auto saved=metadata;
    pugi::set_memory_management_functions(no_memory,std::free);
    assert(trophy_metadata_parse(valid,std::strlen(valid),&metadata)==TROPHY_METADATA_NO_MEMORY);
    pugi::set_memory_management_functions(old_allocate,old_deallocate);
    assert(!std::memcmp(&metadata,&saved,sizeof(saved)));
    invalid("<trophyconf>");invalid(std::string(valid)+"<another/>");
    invalid(change("NPWR00001_00","../invalid_00"));
    invalid(change("id='001'/>","id='016'/>"));
    invalid(change("id='001' gid", "id='128' gid"));
    invalid(change("id='001' gid", "id='000' gid"));
    invalid(change("gid='001'", "gid='002'"));
    invalid(change("ttype='B'", "ttype='P'"));
    invalid(change("hidden='yes'", "hidden='true'"));
    invalid(change("id='001' gid", "id='9999999999999999999999' gid"));
    invalid(change("id='001' gid", "id='-1' gid"));
    invalid(change("id='001' gid", "id='1x' gid"));
    invalid(change("ttype='B'", "ttype='BB'"));
    invalid(change("ttype='B'", "ttype='B' ttype='G'"));
    invalid(std::string("<!DOCTYPE trophyconf []>")+valid);
    assert(trophy_metadata_parse(nullptr,1,&metadata)==TROPHY_METADATA_INVALID);
    std::cout<<"Trophy metadata: XML structure, identity, bounded unique IDs, groups, grades and failure preservation passed.\n";
}
