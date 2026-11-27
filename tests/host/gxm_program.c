#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../source/gxm_program.c"
#include "../../source/module.c"
DECLARE_LIBRARY(SceGxm, 0xf76b66bd);

static void fixture(const char *name, SceGxmProgramType type, const char *const *names,
                    unsigned count)
{
    char path[256];
    snprintf(path,sizeof(path),"external/Vita3K/tools/native-tool/src/shaders/%s.gxp",name);
    FILE *file=fopen(path,"rb");assert(file);
    assert(fseek(file,0,SEEK_END)==0);long size=ftell(file);assert(size>0);
    rewind(file);
    SceGxmProgram *program=malloc((size_t)size);assert(program);
    assert(fread(program,1,(size_t)size,file)==(size_t)size && fclose(file)==0);
    assert(sceGxmProgramCheck(program)==0 && sceGxmProgramGetSize(program)==(unsigned)size);
    assert(sceGxmProgramGetType(program)==type && sceGxmProgramGetParameterCount(program)==count);
    for (unsigned i=0;i<count;++i) {
        const SceGxmProgramParameter *p=sceGxmProgramGetParameter(program,i);assert(p);
        assert(sceGxmProgramFindParameterByName(program,names[i])==p);
        assert(!strcmp((const char *)p+p->name_offset,names[i]));
        if (p->semantic!=SCE_GXM_PARAMETER_SEMANTIC_NONE) {
            assert(sceGxmProgramFindParameterBySemantic(program,p->semantic,p->semantic_index)==p);
            assert(_sceGxmProgramFindParameterBySemantic(program,p->semantic,p->semantic_index)==p);
        }
    }
    assert(!sceGxmProgramGetParameter(program,count));
    assert(!sceGxmProgramGetParameter(program,UINT32_MAX));
    assert(!sceGxmProgramFindParameterByName(program,"missing"));
    assert(!sceGxmProgramFindParameterBySemantic(program,SCE_GXM_PARAMETER_SEMANTIC_NONE,0));
    assert(!sceGxmProgramFindParameterBySemantic(program,SCE_GXM_PARAMETER_SEMANTIC_POSITION,255));
    if (count) {
        SceGxmProgramParameter *p=(void *)sceGxmProgramGetParameter(program,0);
        int32_t old=p->name_offset;
        p->name_offset=(int32_t)((char *)program-(char *)p);
        assert(sceGxmProgramFindParameterByName(program,"GXP")==p);
        p->name_offset=INT32_MIN;assert(!sceGxmProgramFindParameterByName(program,names[0]));
        p->name_offset=INT32_MAX;assert(!sceGxmProgramFindParameterByName(program,names[0]));
        p->name_offset=(int32_t)((char *)program+size-1-(char *)p);
        ((char *)program)[size-1]='X';
        assert(!sceGxmProgramFindParameterByName(program,"X"));
        p->name_offset=old;
    }
    program->parameter_count=UINT32_MAX;
    assert(!sceGxmProgramGetParameter(program,0));
    program->parameter_count=1;program->parameters_offset=UINT32_MAX;
    assert(!sceGxmProgramGetParameter(program,0));
    program->parameters_offset=1;
    assert(!sceGxmProgramGetParameter(program,0));
    program->size=sizeof(*program)-1;
    assert(!sceGxmProgramGetParameter(program,0));
    program->magic=0;
    assert((uint32_t)sceGxmProgramCheck(program)==SCE_GXM_ERROR_NULL_PROGRAM);
    free(program);
}

int main(void)
{
    const char *color[]={"aPosition","aColor","wvp"};
    const char *texture[]={"aPosition","aTexcoord","wvp"};
    const char *clear[]={"uClearColor"},*tex[]={"tex"};
    fixture("color_v",SCE_GXM_VERTEX_PROGRAM,color,3);
    fixture("texture_v",SCE_GXM_VERTEX_PROGRAM,texture,3);
    fixture("clear_f",SCE_GXM_FRAGMENT_PROGRAM,clear,1);
    fixture("texture_f",SCE_GXM_FRAGMENT_PROGRAM,tex,1);
    fixture("color_f",SCE_GXM_FRAGMENT_PROGRAM,NULL,0);
    assert((uint32_t)sceGxmProgramCheck(NULL)==SCE_GXM_ERROR_INVALID_POINTER);
    assert(!sceGxmProgramGetParameter(NULL,0));
    assert(!sceGxmProgramFindParameterByName(NULL,"x"));
    assert(!sceGxmProgramFindParameterByName(NULL,NULL));
    assert(!sceGxmProgramFindParameterBySemantic(NULL,SCE_GXM_PARAMETER_SEMANTIC_POSITION,0));
    assert(!sceGxmProgramGetParameterCount(NULL) && !sceGxmProgramGetSize(NULL));
    const uint32_t nids[]={0xED8B6C69,0x04BB3C59,0xBF5E2090,0xD5D5FCCD,0x06FF9151,
                           0x633CAA54,0x7FFFDD7A,0x277794C4};
    for (unsigned i=0;i<sizeof(nids)/sizeof(*nids);++i)
        assert(module_get_func_export(0xf76b66bd,nids[i]));
    puts("GXP program queries, pinned shader fixtures, malformed ranges and exports passed.");
}
