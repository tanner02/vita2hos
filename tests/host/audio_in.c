#define TEST_THREADED_MUTEX
#define TEST_KERNEL_TIME
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static uint64_t now;
static unsigned sleeps;
static void (*sleep_hook)(void);
static uint64_t armGetSystemTick(void) { return now; }
static uint64_t armTicksToNs(uint64_t tick) { return tick; }
static void svcSleepThread(int64_t duration);
#include "../../source/modules/SceAudioInOut.c"
void log_printf(const char *format,...) { (void)format; }
#define IN_ERROR(name) ((int)SCE_AUDIO_IN_ERROR_##name)
static const uint64_t period=UINT64_C(16000000);
static int port,reopened;
static unsigned char buffer[1540],overlap[1540];
static void unchanged(const unsigned char *data,size_t size) {
    for(size_t i=0;i<size;i++)assert(data[i]==0xa5);
}
static void captured(int handle,int grain) {
    memset(buffer,0xa5,sizeof(buffer));
    assert(!sceAudioInInput(handle,buffer+2));
    unchanged(buffer,2);
    for(int i=0;i<grain*2;i++)assert(buffer[i+2]==0);
    unchanged(buffer+2+grain*2,sizeof(buffer)-2-grain*2);
}
static void overlapping_read(void) {
    memset(overlap,0xa5,sizeof(overlap));
    assert(sceAudioInInput(port,overlap+2)==IN_ERROR(BUSY));
    unchanged(overlap,sizeof(overlap));
}
static void replace_port(void) {
    assert(!sceAudioInReleasePort(port));
    reopened=sceAudioInOpenPort(0,256,16000,0);
    assert(reopened>port);
}
static void svcSleepThread(int64_t duration) {
    assert(duration>0);
    assert(!pthread_mutex_trylock(&audio_lock));
    assert(!pthread_mutex_unlock(&audio_lock));
    void (*hook)(void)=sleep_hook;sleep_hook=NULL;
    if(hook)hook();
    now+=(uint64_t)duration;sleeps++;
}
int main(void) {
    assert(sceAudioInGetAdopt(0)==1&&sceAudioInGetAdopt(2)==1);
    assert(sceAudioInGetAdopt(1)==IN_ERROR(INVALID_PORT_TYPE));
    assert(!sceAudioInGetStatus(1));
    assert(sceAudioInGetStatus(0)==IN_ERROR(INVALID_PARAMETER));
    assert(sceAudioInGetStatus(2)==IN_ERROR(INVALID_PARAMETER));
    assert(sceAudioInOpenPort(1,256,16000,0)==IN_ERROR(INVALID_PORT_TYPE));
    assert(sceAudioInOpenPort(0,256,16000,1)==IN_ERROR(INVALID_PORT_PARAM));
    assert(sceAudioInOpenPort(0,256,48000,0)==IN_ERROR(INVALID_SAMPLE_FREQ));
    assert(sceAudioInOpenPort(2,256,44100,0)==IN_ERROR(INVALID_SAMPLE_FREQ));
    assert(sceAudioInOpenPort(0,768,16000,0)==IN_ERROR(INVALID_PARAMETER));
    assert(sceAudioInOpenPort(2,512,16000,0)==IN_ERROR(INVALID_PARAMETER));
    assert(sceAudioInOpenPort(2,256,48000,0)==IN_ERROR(INVALID_PARAMETER));
    assert(sceAudioInInput(1,buffer)==IN_ERROR(NOT_OPENED));
    int output=sceAudioOutOpenPort(0,256,48000,0);assert(output>0);
    port=sceAudioInOpenPort(0,256,16000,0);assert(port>output);
    assert(!sceAudioInGetStatus(1));
    assert(sceAudioInOpenPort(2,768,48000,0)==IN_ERROR(PORT_FULL));
    assert(sceAudioInInput(output,buffer)==IN_ERROR(INVALID_PORT));
    assert(sceAudioInReleasePort(output)==IN_ERROR(INVALID_PORT));
    assert(sceAudioOutOutput(port,buffer)==(int)SCE_AUDIO_OUT_ERROR_INVALID_PORT);
    assert(sceAudioOutReleasePort(port)==(int)SCE_AUDIO_OUT_ERROR_INVALID_PORT);
    assert(sceAudioInInput(port,NULL)==IN_ERROR(INVALID_POINTER));
    captured(port,256);assert(now==period&&sleeps==1);
    captured(port,256);assert(now==2*period&&sleeps==2);
    now+=10*period;
    captured(port,256);assert(now==12*period&&sleeps==2);
    sleep_hook=overlapping_read;
    captured(port,256);assert(now==13*period&&sleeps==3);
    sleep_hook=replace_port;
    memset(buffer,0xa5,sizeof(buffer));
    assert(sceAudioInInput(port,buffer+2)==IN_ERROR(NOT_OPENED));
    unchanged(buffer,sizeof(buffer));
    assert(sceAudioInInput(port,buffer)==IN_ERROR(INVALID_PORT));
    assert(sceAudioInReleasePort(port)==IN_ERROR(INVALID_PORT));
    captured(reopened,256);
    assert(!sceAudioInReleasePort(reopened));
    assert(sceAudioInInput(reopened,buffer)==IN_ERROR(NOT_OPENED));
    assert(sceAudioInReleasePort(reopened)==IN_ERROR(NOT_OPENED));
    const int configs[][3]={{0,512,16000},{2,256,16000},{2,768,48000}};
    int previous=reopened;
    for(unsigned i=0;i<sizeof(configs)/sizeof(configs[0]);i++) {
        port=sceAudioInOpenPort(configs[i][0],configs[i][1],configs[i][2],0);
        assert(port>previous);previous=port;
        uint64_t before=now;unsigned count=sleeps;
        captured(port,configs[i][1]);
        assert(now-before==(i==0?2*period:period)&&sleeps==count+1);
        assert(!sceAudioInReleasePort(port));
    }
    port=sceAudioInOpenPort(0,256,16000,0);assert(port>previous);
    SceAudioInOut_finish();
    assert(sceAudioInInput(port,buffer)==IN_ERROR(NOT_OPENED));
    assert(sceAudioInReleasePort(port)==IN_ERROR(NOT_OPENED));
    assert(sceAudioOutReleasePort(output)==(int)SCE_AUDIO_OUT_ERROR_INVALID_PORT);
    assert(!sceAudioInGetStatus(1));
    reopened=sceAudioInOpenPort(0,256,16000,0);assert(reopened>port);
    captured(reopened,256);SceAudioInOut_finish();
    puts("Silent input: configurations, guarded silence, pacing, reentrant reads and stale handles passed.");
}
