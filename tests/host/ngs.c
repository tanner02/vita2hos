#define TEST_THREADED_MUTEX
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>
static void *ngs_test_malloc(size_t size);
static void *ngs_test_calloc(size_t count,size_t size);
#define malloc ngs_test_malloc
#define calloc ngs_test_calloc
#include "../../source/modules/SceNgs.c"
#undef malloc
#undef calloc
static unsigned fail_malloc,fail_calloc;
static void *ngs_test_malloc(size_t size)
{if(fail_malloc && !--fail_malloc)return NULL;return malloc(size);}
static void *ngs_test_calloc(size_t count,size_t size)
{if(fail_calloc && !--fail_calloc)return NULL;return calloc(count,size);}
#include "../../source/module.c"
void log_printf(const char *format,...){(void)format;}

_Static_assert(sizeof(NgsHandle)==4,"NGS handles are guest u32 values");
_Static_assert(sizeof(NgsVoiceInfo)==24,"GetInfo writes all six guest words");
_Static_assert(sizeof(NgsPatchSetup)==20,"Routing is a five-word setup");
_Static_assert(sizeof(NgsModuleParamHeader)==8 && sizeof(NgsParamsDescriptor)==8,"Parameter record headers");
typedef int (*StateAbi)(NgsHandle,uint32_t,void *,uint32_t);
typedef int (*RouteAbi)(const NgsPatchSetup *,NgsHandle *);
typedef int (*MatrixAbi)(NgsHandle,const NgsVolumeMatrix *);
typedef int (*UnaryAbi)(NgsHandle);
typedef int (*LockAbi)(NgsHandle,uint32_t,uint32_t,NgsBufferInfo *);
_Static_assert(__builtin_types_compatible_p(__typeof__(&sceNgsVoiceGetStateData),StateAbi),"StateData has four arguments");
_Static_assert(__builtin_types_compatible_p(__typeof__(&sceNgsPatchCreateRouting),RouteAbi),"CreateRouting has two arguments");
_Static_assert(__builtin_types_compatible_p(__typeof__(&sceNgsVoicePatchSetVolumesMatrix),MatrixAbi),"Matrix has two arguments");
_Static_assert(__builtin_types_compatible_p(__typeof__(&sceNgsVoicePlay),UnaryAbi),"Play has one argument");
_Static_assert(__builtin_types_compatible_p(__typeof__(&sceNgsPatchRemoveRouting),UnaryAbi),"RemoveRouting has one argument");
_Static_assert(__builtin_types_compatible_p(__typeof__(&sceNgsVoiceLockParams),LockAbi),"Lock writes BufferInfo");

typedef struct {NgsHandle handle;void *memory;uint32_t size;} Owned;
static Owned system_create(void)
{
    NgsSystemParams params={.max_racks=8,.max_voices=32,.granularity=64,.sample_rate=48000};
    uint32_t size=0;
    assert(!sceNgsSystemGetRequiredMemorySize(&params,&size) && size);
    Owned result={.memory=malloc(size),.size=size};assert(result.memory);
    NgsHandle untouched=UINT32_C(0xcccccccc);
    assert(sceNgsSystemInit(result.memory,size-1,&params,&untouched)!=0 && untouched==UINT32_C(0xcccccccc));
    assert(!sceNgsSystemInit(result.memory,size,&params,&result.handle) && result.handle);
    return result;
}
static Owned rack_create(NgsHandle system,const NgsVoiceDefinition *definition,unsigned count)
{
    NgsRackDescription desc={.definition=definition,.voice_count=(int32_t)count,
        .channels_per_voice=2,.max_patches_per_input=4,.patches_per_output=4};
    uint32_t size=0;assert(!sceNgsRackGetRequiredMemorySize(system,&desc,&size) && size);
    Owned rack={.memory=malloc(size),.size=size};assert(rack.memory);
    NgsBufferInfo buffer={rack.memory,size-1};NgsHandle untouched=12345;
    assert(sceNgsRackInit(system,&buffer,&desc,&untouched)!=0 && untouched==12345);
    buffer.size=size;
    assert(!sceNgsRackInit(system,&buffer,&desc,&rack.handle) && rack.handle);
    return rack;
}
static NgsHandle voice(NgsHandle rack,unsigned index)
{NgsHandle result=0;assert(!sceNgsRackGetVoiceHandle(rack,index,&result) && result);return result;}
static void guard(const unsigned char *bytes,unsigned first,unsigned last,unsigned total)
{
    for(unsigned i=0;i<total;++i)if(i<first || i>=last)assert(bytes[i]==0xa5);
}
static void state_bounds(NgsHandle handle,unsigned module,unsigned count,int zero)
{
    unsigned char data[320];memset(data,0xa5,sizeof(data));
    StateAbi get=(StateAbi)module_get_func_export(0xb01598d9,0xc9b8c0b4);assert(get);
    assert(!get(handle,module,data+16,count));guard(data,16,16+count,sizeof(data));
    if(zero)for(unsigned i=16;i<16+count;++i)assert(!data[i]);
}
static void progress(NgsHandle handle,unsigned updates)
{
    uint32_t actual[6]={0},expected[6]={updates*256,0,updates*128,updates*256,updates*128,updates*256};
    assert(!sceNgsVoiceGetStateData(handle,0,actual,sizeof(actual)));
    assert(!memcmp(actual,expected,sizeof(actual)));
}
static void info_state(NgsHandle handle,unsigned expected,unsigned updates)
{
    NgsVoiceInfo info;memset(&info,0xcc,sizeof(info));
    assert(!sceNgsVoiceGetInfo(handle,&info));
    assert(info.voice_state==expected && info.num_modules==6 && info.num_inputs==1 && info.num_outputs==2);
    assert(info.num_patches_per_output==4 && info.update_passed==updates);
}
static void allocation_failures(void)
{
    NgsSystemParams params={.max_racks=8,.max_voices=32,.granularity=64,.sample_rate=48000};
    uint32_t bytes=0;assert(!sceNgsSystemGetRequiredMemorySize(&params,&bytes));
    void *system_memory=malloc(bytes);assert(system_memory);
    NgsHandle unchanged=12345;uint64_t cursor=ngs_next_handle;
    unsigned systems=ngs_system_count;
    fail_calloc=1;
    assert(sceNgsSystemInit(system_memory,bytes,&params,&unchanged)!=0 && unchanged==12345);
    assert(ngs_next_handle==cursor && ngs_system_count==systems);fail_calloc=0;free(system_memory);
    Owned system=system_create();
    NgsRackDescription desc={.definition=sceNgsVoiceDefGetSimpleVoice(),.voice_count=2,
        .channels_per_voice=2,.max_patches_per_input=4,.patches_per_output=4};
    assert(!sceNgsRackGetRequiredMemorySize(system.handle,&desc,&bytes));
    void *rack_memory=malloc(bytes);assert(rack_memory);NgsBufferInfo buffer={rack_memory,bytes};
    unsigned racks=ngs_rack_count,voices=ngs_voice_count;cursor=ngs_next_handle;
    for(unsigned failure=1;failure<=2;++failure) {
        fail_calloc=failure;unchanged=12345;
        assert(sceNgsRackInit(system.handle,&buffer,&desc,&unchanged)!=0 && unchanged==12345);
        assert(ngs_next_handle==cursor && ngs_rack_count==racks && ngs_voice_count==voices);
        fail_calloc=0;
    }
    free(rack_memory);
    Owned rack=rack_create(system.handle,desc.definition,2);
    NgsHandle a=voice(rack.handle,0),b=voice(rack.handle,1);
    NgsBufferInfo borrow={(void *)(uintptr_t)0x1234,123},saved=borrow;
    fail_calloc=1;
    assert(sceNgsVoiceLockParams(a,1,UINT32_C(0x02015cec),&borrow)!=0);
    assert(!memcmp(&borrow,&saved,sizeof(borrow)) && !find_voice(a)->module[1].data);fail_calloc=0;
    assert(!sceNgsVoiceLockParams(a,1,UINT32_C(0x02015cec),&borrow));
    assert(!sceNgsVoiceUnlockParams(a,1));
    unsigned char records[48]={0};
    NgsModuleParamHeader header={0,0};NgsParamsDescriptor descriptor={UINT32_C(0x01015ce6),16};
    memcpy(records,&header,8);memcpy(records+8,&descriptor,8);
    header.module_id=2;descriptor.id=UINT32_C(0x01015ce3);
    memcpy(records+24,&header,8);memcpy(records+32,&descriptor,8);
    for(unsigned failure=1;failure<=2;++failure) {
        fail_calloc=failure;int32_t errors=77;
        assert(sceNgsVoiceSetParamsBlock(b,(const void *)records,sizeof(records),&errors)!=0 && errors==1);
        assert(!find_voice(b)->module[0].data && !find_voice(b)->module[2].data);fail_calloc=0;
    }
    fail_malloc=1;int32_t errors=77;
    assert(sceNgsVoiceSetParamsBlock(b,(const void *)records,sizeof(records),&errors)!=0 && errors==1);
    assert(!find_voice(b)->module[0].data && !find_voice(b)->module[2].data);fail_malloc=0;
    assert(!sceNgsVoiceSetParamsBlock(b,(const void *)records,sizeof(records),&errors) && !errors);
    NgsPatchSetup setup={.source=a,.source_output_index=0,.source_output_subindex=-1,.dest=b,.dest_input_index=0};
    unsigned patches=ngs_patch_count;cursor=ngs_next_handle;unchanged=12345;fail_calloc=1;
    assert(sceNgsPatchCreateRouting(&setup,&unchanged)!=0 && unchanged==12345);
    assert(ngs_patch_count==patches && ngs_next_handle==cursor);fail_calloc=0;
    assert(!sceNgsPatchCreateRouting(&setup,&unchanged));
    assert(!sceNgsSystemRelease(system.handle));free(rack.memory);free(system.memory);
}
int main(void)
{
    const uint32_t eq=UINT32_C(0x02015cec);
    NgsSystemParams invalid={.max_racks=8,.max_voices=32,.granularity=0,.sample_rate=48000};
    uint32_t unchanged=UINT32_C(0x12345678);
    assert(sceNgsSystemGetRequiredMemorySize(NULL,&unchanged)!=0 && unchanged==UINT32_C(0x12345678));
    assert(sceNgsSystemGetRequiredMemorySize(&invalid,&unchanged)!=0 && unchanged==UINT32_C(0x12345678));
    Owned one=system_create(),two=system_create();assert(one.handle!=two.handle);
    assert(!sceNgsSystemSetFlags(one.handle,0));
    Owned simple=rack_create(one.handle,sceNgsVoiceDefGetSimpleVoice(),2);
    Owned master=rack_create(one.handle,sceNgsVoiceDefGetMasterBuss(),1);
    Owned reverb=rack_create(one.handle,sceNgsVoiceDefGetReverbBuss(),1);
    Owned remote=rack_create(two.handle,sceNgsVoiceDefGetSimpleVoice(),1);
    NgsHandle a=voice(simple.handle,0),b=voice(simple.handle,1),m=voice(master.handle,0),r=voice(reverb.handle,0),other=voice(remote.handle,0);
    assert(a==voice(simple.handle,0) && a!=b && a!=m && a!=r && a!=other);
    NgsHandle unchanged_handle=12345;
    assert(sceNgsRackGetVoiceHandle(simple.handle,2,&unchanged_handle)!=0 && unchanged_handle==12345);
    NgsVoiceInfo bad,before;memset(&bad,0xa5,sizeof(bad));before=bad;
    assert(sceNgsVoiceGetInfo(0,&bad)==NGS_INVALID_HANDLE && !memcmp(&bad,&before,sizeof(bad)));
    info_state(a,0,0);info_state(b,0,0);progress(a,0);progress(b,0);
    const NgsHandle buses[]={m,r};
    for(unsigned i=0;i<2;++i) {
        NgsVoiceInfo bus;memset(&bus,0xcc,sizeof(bus));
        assert(!sceNgsVoiceGetInfo(buses[i],&bus));
        assert(!bus.voice_state && bus.num_modules==2 && bus.num_inputs==1 && bus.num_outputs==1);
        assert(bus.num_patches_per_output==4 && !bus.update_passed);
    }
    assert(!sceNgsVoicePlay(a));info_state(a,NGS_VOICE_ACTIVE,0);
    assert(!sceNgsSystemUpdate(one.handle));info_state(a,NGS_VOICE_ACTIVE,1);info_state(b,0,0);progress(a,1);progress(b,0);
    assert(!sceNgsSystemUpdate(two.handle));info_state(a,NGS_VOICE_ACTIVE,1);
    assert(!sceNgsVoicePause(a));info_state(a,NGS_VOICE_ACTIVE|NGS_VOICE_PAUSED,1);
    assert(!sceNgsSystemUpdate(one.handle));info_state(a,NGS_VOICE_ACTIVE|NGS_VOICE_PAUSED,1);progress(a,1);
    assert(!sceNgsVoiceResume(a));assert(!sceNgsSystemUpdate(one.handle));info_state(a,NGS_VOICE_ACTIVE,2);progress(a,2);
    assert(!sceNgsVoiceKill(a));info_state(a,0,2);progress(a,2);
    assert(sceNgsVoicePause(a)==NGS_INVALID_STATE && sceNgsVoiceResume(a)==NGS_INVALID_STATE);

    NgsBufferInfo lock={(void *)(uintptr_t)0x1234,123},saved=lock;
    assert(sceNgsVoiceLockParams(a,6,eq,&lock)!=0 && !memcmp(&lock,&saved,sizeof(lock)));
    assert(sceNgsVoiceLockParams(m,2,eq,&lock)!=0 && !memcmp(&lock,&saved,sizeof(lock)));
    assert(!sceNgsVoiceLockParams(a,1,eq,&lock) && lock.data && lock.size==88);
    NgsParamsDescriptor descriptor;memcpy(&descriptor,lock.data,sizeof(descriptor));
    assert(descriptor.id==eq && descriptor.size==88);
    unsigned char snapshot[88];memcpy(snapshot,lock.data,sizeof(snapshot));
    for(unsigned i=8;i<88;++i)assert(!snapshot[i]);
    uint32_t marker=UINT32_C(0x76543210);memcpy((char *)lock.data+0x54,&marker,4);
    NgsBufferInfo duplicate={(void *)(uintptr_t)0x9876,456},duplicate_saved=duplicate;
    assert(sceNgsVoiceLockParams(a,1,eq,&duplicate)!=0 && !memcmp(&duplicate,&duplicate_saved,sizeof(duplicate)));
    assert(sceNgsRackRelease(simple.handle,NULL)!=0);
    assert(sceNgsSystemRelease(one.handle)!=0);
    assert(!sceNgsVoiceUnlockParams(a,1));assert(sceNgsVoiceUnlockParams(a,1)!=0);
    assert(!sceNgsVoiceLockParams(a,1,eq,&lock));
    uint32_t got=0;memcpy(&got,(char *)lock.data+0x54,4);assert(got==marker);
    assert(!sceNgsVoiceUnlockParams(a,1));
    assert(!sceNgsVoiceLockParams(b,1,eq,&lock));memcpy(&got,(char *)lock.data+0x54,4);assert(!got);
    assert(!sceNgsVoiceUnlockParams(b,1));
    const uint32_t interfaces[]={0x01015ce6,0x01015cec,0x01015ce3,0x01015ce5,0x01015ce4,0x01015ce4};
    const unsigned capacities[]={84,88,72,16,28,28};
    for(unsigned module=0;module<6;++module) {
        assert(!sceNgsVoiceLockParams(b,module,interfaces[module],&lock));
        assert(lock.data && lock.size==capacities[module]);
        NgsParamsDescriptor desc;memcpy(&desc,lock.data,sizeof(desc));
        assert(desc.id==interfaces[module] && desc.size<=lock.size && desc.size>=8);
        assert(!sceNgsVoiceUnlockParams(b,module));
    }
    assert(!sceNgsVoiceLockParams(r,1,UINT32_C(0x01015ce7),&lock) && lock.size==76);
    assert(!sceNgsVoiceUnlockParams(r,1));
    assert(!sceNgsVoiceBypassModule(a,1,2));assert(!sceNgsVoiceBypassModule(a,1,0));
    assert(sceNgsVoiceBypassModule(a,1,1)!=0 && sceNgsVoiceBypassModule(a,6,0)!=0);

    /* Record payload includes its 8-byte descriptor, not the module header. */
    unsigned char records[2*(8+88)];memset(records,0,sizeof(records));
    NgsModuleParamHeader header={.module_id=1,.channel=0};
    memcpy(records,&header,8);memcpy(records+8,&descriptor,8);
    marker=UINT32_C(0xaabbccdd);memcpy(records+8+0x54,&marker,4);
    int32_t errors=77;
    assert(!sceNgsVoiceSetParamsBlock(a,(const void *)records,96,&errors) && !errors);
    assert(!sceNgsVoiceLockParams(a,1,eq,&lock));memcpy(&got,(char *)lock.data+0x54,4);assert(got==marker);
    assert(!sceNgsVoiceUnlockParams(a,1));
    assert(!sceNgsVoiceLockParams(b,1,eq,&lock));memcpy(&got,(char *)lock.data+0x54,4);assert(!got);
    assert(!sceNgsVoiceUnlockParams(b,1));
    /* An invalid later record must reject the whole block before record one writes. */
    marker=UINT32_C(0x11223344);memcpy(records+8+0x54,&marker,4);
    header.module_id=6;memcpy(records+96,&header,8);memcpy(records+104,&descriptor,8);
    errors=77;assert(sceNgsVoiceSetParamsBlock(a,(const void *)records,sizeof(records),&errors)!=0 && errors==1);
    assert(!sceNgsVoiceLockParams(a,1,eq,&lock));memcpy(&got,(char *)lock.data+0x54,4);assert(got==UINT32_C(0xaabbccdd));
    assert(!sceNgsVoiceUnlockParams(a,1));
    errors=77;assert(sceNgsVoiceSetParamsBlock(a,(const void *)records,95,&errors)!=0 && errors==1);

    /* Positive two-record block commits independent module payloads. */
    header.module_id=2;memcpy(records+96,&header,8);
    NgsParamsDescriptor envelope={UINT32_C(0x01015ce3),72};memcpy(records+104,&envelope,8);
    marker=UINT32_C(0x1234abcd);memcpy(records+104+68,&marker,4);
    errors=77;assert(!sceNgsVoiceSetParamsBlock(a,(const void *)records,176,&errors) && !errors);
    assert(!sceNgsVoiceLockParams(a,1,eq,&lock));memcpy(&got,(char *)lock.data+0x54,4);
    assert(got==UINT32_C(0x11223344));assert(!sceNgsVoiceUnlockParams(a,1));
    assert(!sceNgsVoiceLockParams(a,2,envelope.id,&lock));memcpy(&got,(char *)lock.data+68,4);
    assert(got==marker);assert(!sceNgsVoiceUnlockParams(a,2));
    assert(!sceNgsVoiceLockParams(b,2,envelope.id,&lock));memcpy(&got,(char *)lock.data+68,4);
    assert(!got);assert(!sceNgsVoiceUnlockParams(b,2));

    /* Public alias case: an unlocked borrowed EQ buffer is itself a stream.
     * Compare both committed modules to a separate immutable copy. */
    assert(!sceNgsVoiceLockParams(a,1,eq,&lock));
    unsigned char alias_stream[40]={0};
    NgsModuleParamHeader alias_header={1,0};NgsParamsDescriptor alias_desc={eq,16};
    memcpy(alias_stream,&alias_header,8);memcpy(alias_stream+8,&alias_desc,8);
    marker=UINT32_C(0x654321ab);memcpy(alias_stream+16,&marker,4);
    alias_header.module_id=2;alias_desc=(NgsParamsDescriptor){UINT32_C(0x01015ce3),8};
    memcpy(alias_stream+24,&alias_header,8);memcpy(alias_stream+32,&alias_desc,8);
    memcpy(lock.data,alias_stream,sizeof(alias_stream));void *alias=lock.data;
    assert(!sceNgsVoiceUnlockParams(a,1));errors=77;
    assert(!sceNgsVoiceSetParamsBlock(a,alias,sizeof(alias_stream),&errors) && !errors);
    assert(!sceNgsVoiceLockParams(a,1,eq,&lock));
    assert(!memcmp((char *)lock.data+8,alias_stream+16,8));assert(!sceNgsVoiceUnlockParams(a,1));
    assert(!memcmp(find_voice(a)->module[2].data,alias_stream+32,8));
    assert(!sceNgsVoiceLockParams(a,2,alias_desc.id,&lock));
    NgsParamsDescriptor refreshed;memcpy(&refreshed,lock.data,8);
    assert(refreshed.id==alias_desc.id && refreshed.size==72); /* Lock selects the full interface. */
    assert(!sceNgsVoiceUnlockParams(a,2));

    state_bounds(m,1,64*4,1);state_bounds(m,1,5,1);state_bounds(m,1,0,1);
    state_bounds(a,0,24,0);state_bounds(a,0,3,0);state_bounds(a,0,64,0);
    unsigned char player[64];memset(player,0xcc,sizeof(player));
    assert(!sceNgsVoiceGetStateData(a,0,player,sizeof(player)));
    uint32_t expected_progress[6]={512,0,256,512,256,512};
    assert(!memcmp(player,expected_progress,sizeof(expected_progress)));
    for(unsigned i=24;i<sizeof(player);++i)assert(!player[i]);
    unsigned char partial[3];assert(!sceNgsVoiceGetStateData(a,0,partial,3));
    assert(!memcmp(partial,expected_progress,3));
    assert(!sceNgsVoiceGetStateData(a,0,NULL,0));state_bounds(a,2,20,1);
    unsigned char protected[32];memset(protected,0xa5,sizeof(protected));
    assert(sceNgsVoiceGetStateData(a,6,protected+8,16)!=0);
    guard(protected,0,0,sizeof(protected));
    assert(sceNgsVoiceGetStateData(a,0,protected+8,UINT32_C(1048577))!=0);
    guard(protected,0,0,sizeof(protected));

    assert(!sceNgsVoicePlay(a));info_state(a,NGS_VOICE_ACTIVE,2);
    uint32_t replay[6]={0},replay_expected[6]={0,0,0,0,256,512};
    assert(!sceNgsVoiceGetStateData(a,0,replay,sizeof(replay)));
    assert(!memcmp(replay,replay_expected,sizeof(replay)));
    assert(!sceNgsSystemUpdate(one.handle));info_state(a,NGS_VOICE_ACTIVE,3);
    const uint32_t after_replay[6]={256,0,128,256,384,768};
    assert(!sceNgsVoiceGetStateData(a,0,replay,sizeof(replay)));
    assert(!memcmp(replay,after_replay,sizeof(replay)));assert(!sceNgsVoiceKill(a));
    errors=77;assert(sceNgsVoiceSetParamsBlock(a,(const void *)records,UINT32_C(1048577),&errors)!=0 && errors==1);

    NgsPatchSetup setup={.source=a,.source_output_index=0,.source_output_subindex=-1,.dest=m,.dest_input_index=0};
    RouteAbi create=(RouteAbi)module_get_func_export(0xb01598d9,0xd668b49c);assert(create);
    NgsHandle patch=0;assert(!create(&setup,&patch) && patch);
    /* There is no matrix getter: inspect the owned record to verify the
     * zero initial routing gain and a copy, rather than a borrowed pointer. */
    const NgsVolumeMatrix zero_matrix={0};
    assert(!memcmp(&find_patch(patch)->matrix,&zero_matrix,sizeof(zero_matrix)));
    NgsVolumeMatrix matrix={.matrix={{1,0},{0,1}}};
    MatrixAbi set_matrix=(MatrixAbi)module_get_func_export(0xb01598d9,0xa0f5402d);assert(set_matrix);
    assert(!set_matrix(patch,&matrix));
    NgsVolumeMatrix matrix_snapshot=matrix;matrix.matrix[0][0]=0.5f;
    assert(!memcmp(&find_patch(patch)->matrix,&matrix_snapshot,sizeof(matrix_snapshot)));
    assert(set_matrix(a,&matrix)==NGS_INVALID_HANDLE);
    setup.source_output_subindex=0;NgsHandle occupied=999;
    assert(create(&setup,&occupied)==NGS_INVALID_STATE && occupied==999);
    setup.source_output_subindex=4;
    assert(create(&setup,&occupied)!=0 && occupied==999);
    setup.source_output_subindex=-1;
    NgsHandle route_unchanged=999;
    setup.source_output_index=2;
    assert(create(&setup,&route_unchanged)!=0 && route_unchanged==999);
    setup.source_output_index=0;setup.dest=other;
    assert(create(&setup,&route_unchanged)!=0 && route_unchanged==999);
    setup.dest=m;setup.dest_input_index=1;
    assert(create(&setup,&route_unchanged)!=0 && route_unchanged==999);
    setup.dest_input_index=0;
    assert(!sceNgsPatchRemoveRouting(patch));assert(sceNgsPatchRemoveRouting(patch)==NGS_INVALID_HANDLE);
    assert(!create(&setup,&patch));
    assert(!sceNgsRackRelease(simple.handle,NULL));
    assert(sceNgsVoicePlay(a)==NGS_INVALID_HANDLE && sceNgsVoiceGetInfo(b,&bad)==NGS_INVALID_HANDLE);
    assert(set_matrix(patch,&matrix)==NGS_INVALID_HANDLE);
    assert(!sceNgsVoiceGetInfo(other,&bad));
    assert(!sceNgsSystemRelease(one.handle));assert(sceNgsVoiceGetInfo(m,&bad)==NGS_INVALID_HANDLE);
    assert(!sceNgsVoiceGetInfo(other,&bad));
    SceNgs_finish();assert(sceNgsVoiceGetInfo(other,&bad)==NGS_INVALID_HANDLE);
    assert(sceNgsSystemUpdate(two.handle)==NGS_INVALID_HANDLE);
    allocation_failures();SceNgs_finish();
    free(simple.memory);free(master.memory);free(reverb.memory);free(remote.memory);free(one.memory);free(two.memory);
    puts("NGS ABI, owned handles, atomic parameters, state bounds, routing and silent lifecycle checks passed.");
}
