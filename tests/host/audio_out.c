#define TEST_THREADED_MUTEX
#define TEST_KERNEL_TIME
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static uint64_t now;
static unsigned sleeps;
static uint64_t armGetSystemTick(void) { return now; }
static uint64_t armTicksToNs(uint64_t tick) { return tick; }
static void svcSleepThread(int64_t duration) {
    assert(duration>0);now+=(uint64_t)duration;sleeps++;
}
#include "../../source/modules/SceAudioInOut.c"
void log_printf(const char *format,...) { (void)format; }
int main(void) {
    assert(sceAudioOutOpenPort(3,256,48000,1)==(int)SCE_AUDIO_OUT_ERROR_INVALID_PORT_TYPE);
    assert(sceAudioOutOpenPort(0,65,48000,1)==(int)SCE_AUDIO_OUT_ERROR_INVALID_SIZE);
    assert(sceAudioOutOpenPort(0,256,44100,1)==(int)SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ);
    assert(sceAudioOutOpenPort(0,256,48000,2)==(int)SCE_AUDIO_OUT_ERROR_INVALID_FORMAT);
    int a=sceAudioOutOpenPort(0,256,48000,1),b=sceAudioOutOpenPort(1,256,48000,1);
    assert(a>0&&b>0&&a!=b);
    uint32_t sample=0;
    uint64_t period=(UINT64_C(256000000000)+47999)/48000;
    assert(!sceAudioOutOutput(a,&sample)&&now==0&&!sleeps);
    assert(!sceAudioOutOutput(b,&sample)&&now==0&&!sleeps);
    assert(!sceAudioOutOutput(a,&sample)&&now==period&&sleeps==1);
    assert(!sceAudioOutOutput(b,&sample)&&now==period&&sleeps==1);
    assert(!sceAudioOutOutput(a,NULL)&&now==2*period&&sleeps==2);
    assert(!sceAudioOutOutput(a,NULL)&&now==2*period&&sleeps==2);
    now+=period*10;
    assert(!sceAudioOutOutput(a,&sample)&&sleeps==2);
    assert(!sceAudioOutOutput(a,NULL)&&now==13*period&&sleeps==3);
    int volume[2]={32768,0};
    assert(!sceAudioOutSetVolume(a,3,volume));
    assert(sceAudioOutSetVolume(a,3,NULL)==(int)SCE_AUDIO_OUT_ERROR_INVALID_POINTER);
    volume[1]=32769;
    assert(sceAudioOutSetVolume(a,3,volume)==(int)SCE_AUDIO_OUT_ERROR_INVALID_VOLUME);
    assert(!sceAudioOutReleasePort(a));
    assert(sceAudioOutOutput(a,&sample)==(int)SCE_AUDIO_OUT_ERROR_INVALID_PORT);
    int c=sceAudioOutOpenPort(0,256,48000,1);assert(c>0&&c!=a);
    assert(!sceAudioOutOutput(c,&sample));
    SceAudioInOut_finish();
    assert(sceAudioOutOutput(b,&sample)==(int)SCE_AUDIO_OUT_ERROR_INVALID_PORT);
    assert(sceAudioOutReleasePort(c)==(int)SCE_AUDIO_OUT_ERROR_INVALID_PORT);
    puts("Silent audio: typed ports, independent queue pacing, drains, validation and stale handles passed.");
}
