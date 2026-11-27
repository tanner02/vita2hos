#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../source/gxm_draw.c"

int main(void)
{
    /* Distinct faces survive temporary one-sided rendering. Other rasterizer
     * bits and another context's independent state must remain untouched. */
    DkRasterizerState stored={.rasterizerEnable=1,.depthClampEnable=1,
        .polygonModeFront=DkPolygonMode_Line,.polygonModeBack=DkPolygonMode_Point,
        .cullMode=DkFace_Back,.frontFace=1,.depthBiasEnableMask=7};
    DkRasterizerState saved=stored,other={.polygonModeFront=DkPolygonMode_Fill,
                                       .polygonModeBack=DkPolygonMode_Fill};
    DkRasterizerState one=gxm_effective_rasterizer(&stored,false);
    assert(one.polygonModeFront==DkPolygonMode_Line && one.polygonModeBack==DkPolygonMode_Line);
    one.polygonModeBack=DkPolygonMode_Point;
    assert(!memcmp(&one,&saved,sizeof(one)) && !memcmp(&stored,&saved,sizeof(stored)));
    DkRasterizerState two=gxm_effective_rasterizer(&stored,true);
    assert(!memcmp(&two,&saved,sizeof(two)));
    DkRasterizerState separate=gxm_effective_rasterizer(&other,false);
    assert(separate.polygonModeFront==DkPolygonMode_Fill && separate.polygonModeBack==DkPolygonMode_Fill);
    const SceGxmPolygonMode polygon_modes[]={SCE_GXM_POLYGON_MODE_TRIANGLE_FILL,
        SCE_GXM_POLYGON_MODE_LINE,SCE_GXM_POLYGON_MODE_POINT_10UV,SCE_GXM_POLYGON_MODE_POINT,
        SCE_GXM_POLYGON_MODE_POINT_01UV,SCE_GXM_POLYGON_MODE_TRIANGLE_LINE,SCE_GXM_POLYGON_MODE_TRIANGLE_POINT};
    const DkPolygonMode native_modes[]={DkPolygonMode_Fill,DkPolygonMode_Line,DkPolygonMode_Point,
        DkPolygonMode_Point,DkPolygonMode_Point,DkPolygonMode_Line,DkPolygonMode_Point};
    for (unsigned i=0;i<7;++i) {
        DkPolygonMode native=DkPolygonMode_Fill;
        assert(!gxm_translate_polygon(polygon_modes[i],&native) && native==native_modes[i]);
    }
    DkPolygonMode native=DkPolygonMode_Line;
    assert((uint32_t)gxm_translate_polygon(0x38000,&native)==SCE_GXM_ERROR_INVALID_VALUE && native==DkPolygonMode_Line);
    assert((uint32_t)gxm_translate_polygon(SCE_GXM_POLYGON_MODE_TRIANGLE_FILL,NULL)==SCE_GXM_ERROR_INVALID_POINTER);
    DkFace face;
    assert(gxm_translate_cull(SCE_GXM_CULL_NONE, &face) == 0 && face == DkFace_None);
    assert(gxm_translate_cull(SCE_GXM_CULL_CW, &face) == 0 && face == DkFace_Front);
    assert(gxm_translate_cull(SCE_GXM_CULL_CCW, &face) == 0 && face == DkFace_Back);
    assert((uint32_t)gxm_translate_cull((SceGxmCullMode)3, &face) == SCE_GXM_ERROR_INVALID_VALUE);
    assert(face == DkFace_Back);
    assert((uint32_t)gxm_translate_cull(SCE_GXM_CULL_NONE, NULL) == SCE_GXM_ERROR_INVALID_POINTER);
    SceGxmVertexStream streams[SCE_GXM_MAX_VERTEX_STREAMS] = {0};
    SceGxmVertexAttribute attributes[SCE_GXM_MAX_VERTEX_ATTRIBUTES] = {0};
    for (unsigned int i = 0; i < SCE_GXM_MAX_VERTEX_ATTRIBUTES; ++i) {
        attributes[i].streamIndex = i;
        attributes[i].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
        attributes[i].componentCount = 4;
    }
    assert(gxm_validate_vertex_layout(attributes, 16, streams, 16) == 0);
    assert(gxm_validate_vertex_layout(NULL, 0, NULL, 0) == 0);
    assert((uint32_t)gxm_validate_vertex_layout(attributes, 17, streams, 16) == SCE_GXM_ERROR_INVALID_VALUE);
    assert((uint32_t)gxm_validate_vertex_layout(attributes, 16, streams, 17) == SCE_GXM_ERROR_INVALID_VALUE);
    assert((uint32_t)gxm_validate_vertex_layout(NULL, 1, streams, 1) == SCE_GXM_ERROR_INVALID_POINTER);
    assert((uint32_t)gxm_validate_vertex_layout(attributes, 1, NULL, 1) == SCE_GXM_ERROR_INVALID_POINTER);
    assert((uint32_t)gxm_validate_vertex_layout(attributes, 1, NULL, 0) == SCE_GXM_ERROR_INVALID_VALUE);
    attributes[0].streamIndex = 1;
    assert((uint32_t)gxm_validate_vertex_layout(attributes, 1, streams, 1) == SCE_GXM_ERROR_INVALID_VALUE);
    attributes[0].streamIndex = 0;
    attributes[0].componentCount = 0;
    assert((uint32_t)gxm_validate_vertex_layout(attributes, 1, streams, 1) == SCE_GXM_ERROR_INVALID_VALUE);
    attributes[0].componentCount = 5;
    assert((uint32_t)gxm_validate_vertex_layout(attributes, 1, streams, 1) == SCE_GXM_ERROR_INVALID_VALUE);
    attributes[0].componentCount = 4;
    attributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED;
    assert(gxm_validate_vertex_layout(attributes, 1, streams, 1) == 0);
    attributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED + 1;
    assert((uint32_t)gxm_validate_vertex_layout(attributes, 1, streams, 1) == SCE_GXM_ERROR_INVALID_VALUE);
    attributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
    attributes[0].offset = 0x4000;
    assert((uint32_t)gxm_validate_vertex_layout(attributes, 1, streams, 1) == SCE_GXM_ERROR_INVALID_VALUE);
    attributes[0].offset = 0x3fff;
    assert(gxm_validate_vertex_layout(attributes, 1, streams, 1) == 0);
    streams[0].indexSource = 4;
    assert((uint32_t)gxm_validate_vertex_layout(attributes, 1, streams, 1) == SCE_GXM_ERROR_INVALID_VALUE);
    uint32_t divisor = 123;
    assert(gxm_stream_divisor(SCE_GXM_INDEX_SOURCE_INDEX_16BIT, &divisor) == 0 && divisor == 0);
    assert(gxm_stream_divisor(SCE_GXM_INDEX_SOURCE_INDEX_32BIT, &divisor) == 0 && divisor == 0);
    assert(gxm_stream_divisor(SCE_GXM_INDEX_SOURCE_INSTANCE_16BIT, &divisor) == 0 && divisor == 1);
    assert(gxm_stream_divisor(SCE_GXM_INDEX_SOURCE_INSTANCE_32BIT, &divisor) == 0 && divisor == 1);
    divisor = 123;
    assert((uint32_t)gxm_stream_divisor((SceGxmIndexSource)4, &divisor) == SCE_GXM_ERROR_INVALID_VALUE);
    assert(divisor == 123);
    uint32_t indices = 123, instances = 456;
    assert((uint32_t)gxm_instanced_counts(10, 0, &indices, &instances) == SCE_GXM_ERROR_INVALID_VALUE);
    assert(indices == 123 && instances == 456);
    assert(gxm_instanced_counts(12, 3, &indices, &instances) == 0 && indices == 3 && instances == 4);
    assert(gxm_instanced_counts(13, 3, &indices, &instances) == 0 && indices == 3 && instances == 4);
    assert(gxm_instanced_counts(0, 3, &indices, &instances) == 0 && instances == 0);
    assert(gxm_instanced_counts(UINT32_MAX, 1, &indices, &instances) == 0 && instances == UINT32_MAX);
    const struct { SceGxmPrimitiveType vita; DkPrimitive gpu; } primitives[] = {
        {SCE_GXM_PRIMITIVE_POINTS, DkPrimitive_Points},
        {SCE_GXM_PRIMITIVE_LINES, DkPrimitive_Lines},
        {SCE_GXM_PRIMITIVE_TRIANGLES, DkPrimitive_Triangles},
        {SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, DkPrimitive_TriangleStrip},
        {SCE_GXM_PRIMITIVE_TRIANGLE_FAN, DkPrimitive_TriangleFan},
    };
    for (size_t i = 0; i < sizeof(primitives) / sizeof(primitives[0]); ++i) {
        DkPrimitive gpu;
        assert(gxm_translate_primitive(primitives[i].vita, &gpu) == 0);
        assert(gpu == primitives[i].gpu);
    }
    DkPrimitive gpu = DkPrimitive_TriangleFan;
    assert((uint32_t)gxm_translate_primitive((SceGxmPrimitiveType)123, &gpu) ==
           SCE_GXM_ERROR_INVALID_VALUE && gpu == DkPrimitive_TriangleFan);
    assert((uint32_t)gxm_translate_primitive(SCE_GXM_PRIMITIVE_POINTS, NULL) ==
           SCE_GXM_ERROR_INVALID_POINTER);
    uint32_t offset = 123;
    assert(gxm_validate_index_range(SCE_GXM_INDEX_FORMAT_U16, 0x1002, 0x1000, 10, 4, &offset) == 0);
    assert(offset == 2);
    assert(gxm_validate_index_range(SCE_GXM_INDEX_FORMAT_U32, 0x1004, 0x1000, 12, 2, &offset) == 0);
    assert(offset == 4);
    offset = 123;
    assert((uint32_t)gxm_validate_index_range(SCE_GXM_INDEX_FORMAT_U32, 0x1004, 0x1000, 11, 2, &offset) ==
           SCE_GXM_ERROR_INVALID_VALUE);
    assert(offset == 123);
    assert((uint32_t)gxm_validate_index_range(SCE_GXM_INDEX_FORMAT_U16, 0x1001, 0x1000, 10, 1, &offset) ==
           SCE_GXM_ERROR_INVALID_ALIGNMENT);
    assert((uint32_t)gxm_validate_index_range(SCE_GXM_INDEX_FORMAT_U32, 0x1002, 0x1000, 10, 1, &offset) ==
           SCE_GXM_ERROR_INVALID_ALIGNMENT);
    assert((uint32_t)gxm_validate_index_range(SCE_GXM_INDEX_FORMAT_U16, 0xffe, 0x1000, 10, 1, &offset) ==
           SCE_GXM_ERROR_INVALID_POINTER);
    assert((uint32_t)gxm_validate_index_range(SCE_GXM_INDEX_FORMAT_U16, 0x1010, 0x1000, 10, 0, &offset) ==
           SCE_GXM_ERROR_INVALID_VALUE);
    assert((uint32_t)gxm_validate_index_range((SceGxmIndexFormat)123, 0x1000, 0x1000, 10, 1, &offset) ==
           SCE_GXM_ERROR_INVALID_VALUE);
    assert((uint32_t)gxm_validate_index_range(SCE_GXM_INDEX_FORMAT_U16, 0, 0, 10, 1, &offset) ==
           SCE_GXM_ERROR_INVALID_POINTER);
    assert((uint32_t)gxm_validate_index_range(SCE_GXM_INDEX_FORMAT_U16, 0x1000, 0x1000, 10, 1, NULL) ==
           SCE_GXM_ERROR_INVALID_POINTER);
    assert((uint32_t)gxm_validate_index_range(SCE_GXM_INDEX_FORMAT_U32, 0x1000, 0x1000, UINT32_MAX,
                                            UINT32_MAX, &offset) == SCE_GXM_ERROR_INVALID_VALUE);
    /* Subtraction-based checks also work at the address-space boundary. */
    assert(gxm_validate_index_range(SCE_GXM_INDEX_FORMAT_U32, UINTPTR_MAX - 3,
                                    UINTPTR_MAX - 7, 8, 1, &offset) == 0 && offset == 4);
    /* Native format enums are not sequential by component count. Check all
     * widths, signedness/normalization classes and the raw 32-bit format. */
    const struct { SceGxmAttributeFormat format; DkVtxAttribType type; unsigned width; } formats[] = {
        {SCE_GXM_ATTRIBUTE_FORMAT_U8,DkVtxAttribType_Uint,0},
        {SCE_GXM_ATTRIBUTE_FORMAT_S8,DkVtxAttribType_Sint,0},
        {SCE_GXM_ATTRIBUTE_FORMAT_U16,DkVtxAttribType_Uint,1},
        {SCE_GXM_ATTRIBUTE_FORMAT_S16,DkVtxAttribType_Sint,1},
        {SCE_GXM_ATTRIBUTE_FORMAT_U8N,DkVtxAttribType_Unorm,0},
        {SCE_GXM_ATTRIBUTE_FORMAT_S8N,DkVtxAttribType_Snorm,0},
        {SCE_GXM_ATTRIBUTE_FORMAT_U16N,DkVtxAttribType_Unorm,1},
        {SCE_GXM_ATTRIBUTE_FORMAT_S16N,DkVtxAttribType_Snorm,1},
        {SCE_GXM_ATTRIBUTE_FORMAT_F16,DkVtxAttribType_Float,1},
        {SCE_GXM_ATTRIBUTE_FORMAT_F32,DkVtxAttribType_Float,2},
        {SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED,DkVtxAttribType_Uint,2}
    };
    const DkVtxAttribSize sizes[3][4] = {
        {DkVtxAttribSize_1x8,DkVtxAttribSize_2x8,DkVtxAttribSize_3x8,DkVtxAttribSize_4x8},
        {DkVtxAttribSize_1x16,DkVtxAttribSize_2x16,DkVtxAttribSize_3x16,DkVtxAttribSize_4x16},
        {DkVtxAttribSize_1x32,DkVtxAttribSize_2x32,DkVtxAttribSize_3x32,DkVtxAttribSize_4x32}
    };
    for (unsigned f=0;f<sizeof(formats)/sizeof(formats[0]);++f) {
        for (unsigned components=1;components<=4;++components) {
            DkVtxAttribType type=DkVtxAttribType_None;
            DkVtxAttribSize size=DkVtxAttribSize_1x8;
            assert(!gxm_translate_vertex_format(formats[f].format,components,true,&type,&size));
            assert(type==formats[f].type && size==sizes[formats[f].width][components-1]);
        }
    }
    for (unsigned f=0;f<4;++f) {
        DkVtxAttribType converted;
        DkVtxAttribSize size;
        assert(!gxm_translate_vertex_format(formats[f].format,3,false,&converted,&size));
        assert(converted==(f==0 || f==2 ? DkVtxAttribType_Uscaled : DkVtxAttribType_Sscaled));
        assert(size==sizes[formats[f].width][2]);
    }
    /* Normalization and ordinary floating-point fetch remain explicit format
     * semantics rather than inheriting the unnormalized integer conversion. */
    for (unsigned f=4;f<sizeof(formats)/sizeof(formats[0]);++f) {
        DkVtxAttribType converted;
        DkVtxAttribSize size;
        assert(!gxm_translate_vertex_format(formats[f].format,4,false,&converted,&size));
        assert(converted==formats[f].type && size==sizes[formats[f].width][3]);
    }
    DkVtxAttribType type=DkVtxAttribType_Snorm;
    DkVtxAttribSize size=DkVtxAttribSize_3x8;
    for (unsigned components=0;components<=5;components+=5) {
        assert((uint32_t)gxm_translate_vertex_format(SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED,components,true,&type,&size)==SCE_GXM_ERROR_INVALID_VALUE);
        assert(type==DkVtxAttribType_Snorm && size==DkVtxAttribSize_3x8);
    }
    assert((uint32_t)gxm_translate_vertex_format((SceGxmAttributeFormat)-1,1,true,&type,&size)==SCE_GXM_ERROR_INVALID_VALUE);
    assert((uint32_t)gxm_translate_vertex_format(SCE_GXM_ATTRIBUTE_FORMAT_UNTYPED+1,1,true,&type,&size)==SCE_GXM_ERROR_INVALID_VALUE);
    assert(type==DkVtxAttribType_Snorm && size==DkVtxAttribSize_3x8);
    assert((uint32_t)gxm_translate_vertex_format(SCE_GXM_ATTRIBUTE_FORMAT_F32,1,true,NULL,&size)==SCE_GXM_ERROR_INVALID_POINTER);
    assert(size==DkVtxAttribSize_3x8);
    assert((uint32_t)gxm_translate_vertex_format(SCE_GXM_ATTRIBUTE_FORMAT_F32,1,true,&type,NULL)==SCE_GXM_ERROR_INVALID_POINTER);
    assert(type==DkVtxAttribType_Snorm);
    SceGxmVertexAttribute reordered[3] = {
        {.streamIndex=1,.offset=12,.format=SCE_GXM_ATTRIBUTE_FORMAT_U8,.componentCount=3},
        {.streamIndex=0,.offset=8,.format=SCE_GXM_ATTRIBUTE_FORMAT_F32,.componentCount=2},
        {.streamIndex=0,.offset=16,.format=SCE_GXM_ATTRIBUTE_FORMAT_S16,.componentCount=4}
    };
    GxmVertexInput inputs[3] = {{.location=5}, {.location=UINT32_MAX}, {.location=0,.integer_input=true}};
    DkVtxAttribState state[SCE_GXM_MAX_VERTEX_ATTRIBUTES];
    assert(!gxm_build_vertex_attribute_state(reordered,3,inputs,state));
    assert(!state[5].isFixed && state[5].bufferId==1 && state[5].offset==12 &&
           state[5].type==DkVtxAttribType_Uscaled && state[5].size==DkVtxAttribSize_3x8);
    assert(!state[0].isFixed && state[0].type==DkVtxAttribType_Sint &&
           state[0].size==DkVtxAttribSize_4x16 && state[0].offset==16);
    for (unsigned i=1;i<SCE_GXM_MAX_VERTEX_ATTRIBUTES;++i)
        if (i!=5) assert(state[i].isFixed);
    DkVtxAttribState preserved[SCE_GXM_MAX_VERTEX_ATTRIBUTES];
    memcpy(preserved,state,sizeof(state));
    inputs[2].location=5;
    assert((uint32_t)gxm_build_vertex_attribute_state(reordered,3,inputs,state)==SCE_GXM_ERROR_INVALID_VALUE);
    assert(!memcmp(preserved,state,sizeof(state)));
    inputs[2].location=SCE_GXM_MAX_VERTEX_ATTRIBUTES;
    assert((uint32_t)gxm_build_vertex_attribute_state(reordered,3,inputs,state)==SCE_GXM_ERROR_INVALID_VALUE);
    assert(!memcmp(preserved,state,sizeof(state)));
    assert((uint32_t)gxm_build_vertex_attribute_state(NULL,1,inputs,state)==SCE_GXM_ERROR_INVALID_POINTER);
    assert(!memcmp(preserved,state,sizeof(state)));
    assert((uint32_t)gxm_build_vertex_attribute_state(reordered,3,NULL,state)==SCE_GXM_ERROR_INVALID_POINTER);
    assert((uint32_t)gxm_build_vertex_attribute_state(reordered,3,inputs,NULL)==SCE_GXM_ERROR_INVALID_POINTER);
    assert((uint32_t)gxm_build_vertex_attribute_state(reordered,SCE_GXM_MAX_VERTEX_ATTRIBUTES+1,inputs,state)==SCE_GXM_ERROR_INVALID_VALUE);
    assert(!memcmp(preserved,state,sizeof(state)));
    /* A new program with no supplied attributes must clear every stale fetch. */
    assert(!gxm_build_vertex_attribute_state(NULL,0,NULL,state));
    for (unsigned i=0;i<SCE_GXM_MAX_VERTEX_ATTRIBUTES;++i) assert(state[i].isFixed);
    SceGxmVertexAttribute packed = {
        .streamIndex=2,.offset=32,.format=SCE_GXM_ATTRIBUTE_FORMAT_F32,.componentCount=1
    };
    GxmVertexInput raw = {.location=3,.register_format=true,.register_component_bytes=2,.register_components=4};
    assert(!gxm_build_vertex_attribute_state(&packed,1,&raw,state));
    assert(state[3].type==DkVtxAttribType_Uint && state[3].size==DkVtxAttribSize_4x16);
    /* Signed and half-float register inputs carry unsigned bits. Normalized
     * guest formats must not normalize them before the translator packs them. */
    packed.format=SCE_GXM_ATTRIBUTE_FORMAT_S8N;
    raw.register_component_bytes=1;
    assert(!gxm_build_vertex_attribute_state(&packed,1,&raw,state));
    assert(state[3].type==DkVtxAttribType_Uint && state[3].size==DkVtxAttribSize_4x8);
    raw.register_component_bytes=4; raw.register_components=8;
    assert(!gxm_build_vertex_attribute_state(&packed,1,&raw,state));
    assert(state[3].type==DkVtxAttribType_Uint && state[3].size==DkVtxAttribSize_4x32 && state[3].offset==32);
    assert(state[4].type==DkVtxAttribType_Uint && state[4].size==DkVtxAttribSize_4x32 && state[4].offset==48);
    /* A five-byte register input reads only the final byte at its second
     * location; unused vec4 lanes must not extend the memory read. */
    raw.register_component_bytes=1; raw.register_components=5;
    assert(!gxm_build_vertex_attribute_state(&packed,1,&raw,state));
    assert(state[3].size==DkVtxAttribSize_4x8 && state[4].size==DkVtxAttribSize_1x8 && state[4].offset==36);
    memcpy(preserved,state,sizeof(state));
    SceGxmVertexAttribute overlap_attributes[2] = {packed, packed};
    GxmVertexInput overlap_inputs[2] = {raw, {.location=4}};
    assert((uint32_t)gxm_build_vertex_attribute_state(overlap_attributes,2,overlap_inputs,state)==SCE_GXM_ERROR_INVALID_VALUE);
    assert(!memcmp(preserved,state,sizeof(state)));
    raw.location=SCE_GXM_MAX_VERTEX_ATTRIBUTES-1;
    assert((uint32_t)gxm_build_vertex_attribute_state(&packed,1,&raw,state)==SCE_GXM_ERROR_INVALID_VALUE);
    assert(!memcmp(preserved,state,sizeof(state)));
    raw.location=3; packed.offset=0x3ffe;
    assert((uint32_t)gxm_build_vertex_attribute_state(&packed,1,&raw,state)==SCE_GXM_ERROR_INVALID_VALUE);
    assert(!memcmp(preserved,state,sizeof(state)));
    packed.offset=32; raw.register_component_bytes=3;
    assert((uint32_t)gxm_build_vertex_attribute_state(&packed,1,&raw,state)==SCE_GXM_ERROR_INVALID_VALUE);
    assert(!memcmp(preserved,state,sizeof(state)));
    raw.register_component_bytes=4; raw.register_components=0;
    assert((uint32_t)gxm_build_vertex_attribute_state(&packed,1,&raw,state)==SCE_GXM_ERROR_INVALID_VALUE);
    assert(!memcmp(preserved,state,sizeof(state)));
    raw.register_components=UINT32_MAX;
    assert((uint32_t)gxm_build_vertex_attribute_state(&packed,1,&raw,state)==SCE_GXM_ERROR_INVALID_VALUE);
    assert(!memcmp(preserved,state,sizeof(state)));
    /* Numeric arrays retain one location per vector, including vec1/vec2/vec3.
     * Offsets advance by guest storage width, not padded native vec4 width. */
    SceGxmVertexAttribute array_attr = {.streamIndex=1,.offset=12,
        .format=SCE_GXM_ATTRIBUTE_FORMAT_F32,.componentCount=3};
    GxmVertexInput array_input = {.location=2,.array_elements=3};
    for (unsigned f=0;f<sizeof(formats)/sizeof(formats[0]);++f) {
        array_attr.format=formats[f].format;
        array_input.integer_input=true;
        for (unsigned components=1;components<=4;++components) {
            array_attr.componentCount=components;
            assert(!gxm_build_vertex_attribute_state(&array_attr,1,&array_input,state));
            for (unsigned slot=0;slot<3;++slot)
                assert(state[2+slot].offset==12+slot*components*(1U<<formats[f].width) &&
                    state[2+slot].size==sizes[formats[f].width][components-1] &&
                    state[2+slot].type==formats[f].type);
        }
    }
    array_attr.format=SCE_GXM_ATTRIBUTE_FORMAT_F32; array_attr.componentCount=3;
    array_input.integer_input=false;
    assert(!gxm_build_vertex_attribute_state(&array_attr,1,&array_input,state));
    for (unsigned i=0;i<3;++i)
        assert(!state[2+i].isFixed && state[2+i].bufferId==1 && state[2+i].offset==12+i*12 &&
               state[2+i].type==DkVtxAttribType_Float && state[2+i].size==DkVtxAttribSize_3x32);
    array_attr.format=SCE_GXM_ATTRIBUTE_FORMAT_S8N;
    array_attr.componentCount=2;
    assert(!gxm_build_vertex_attribute_state(&array_attr,1,&array_input,state));
    assert(state[3].offset==14 && state[4].offset==16 &&
           state[4].type==DkVtxAttribType_Snorm && state[4].size==DkVtxAttribSize_2x8);
    array_attr.format=SCE_GXM_ATTRIBUTE_FORMAT_U16;
    array_attr.componentCount=1;
    assert(!gxm_build_vertex_attribute_state(&array_attr,1,&array_input,state));
    assert(state[3].offset==14 && state[4].type==DkVtxAttribType_Uscaled && state[4].size==DkVtxAttribSize_1x16);
    array_input.integer_input=true;
    assert(!gxm_build_vertex_attribute_state(&array_attr,1,&array_input,state));
    assert(state[4].type==DkVtxAttribType_Uint);
    memcpy(preserved,state,sizeof(state));
    overlap_attributes[0]=array_attr; overlap_attributes[1]=array_attr;
    overlap_inputs[0]=array_input; overlap_inputs[1]=(GxmVertexInput){.location=4};
    assert((uint32_t)gxm_build_vertex_attribute_state(overlap_attributes,2,overlap_inputs,state)==SCE_GXM_ERROR_INVALID_VALUE);
    assert(!memcmp(preserved,state,sizeof(state)));
    array_input.location=15;
    assert((uint32_t)gxm_build_vertex_attribute_state(&array_attr,1,&array_input,state)==SCE_GXM_ERROR_INVALID_VALUE);
    assert(!memcmp(preserved,state,sizeof(state)));
    array_input.location=2; array_input.array_elements=UINT32_MAX;
    assert((uint32_t)gxm_build_vertex_attribute_state(&array_attr,1,&array_input,state)==SCE_GXM_ERROR_INVALID_VALUE);
    assert(!memcmp(preserved,state,sizeof(state)));
    array_input.array_elements=3; array_attr.offset=0x3ffe;
    assert((uint32_t)gxm_build_vertex_attribute_state(&array_attr,1,&array_input,state)==SCE_GXM_ERROR_INVALID_VALUE);
    assert(!memcmp(preserved,state,sizeof(state)));
    puts("GXM index range regression checks passed.");
    return 0;
}
