/* Owned silent microphone ABI and pacing probe; no Vita libc startup. */
#include <stdint.h>
#include <psp2/audioin.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/processmgr.h>

#define REPORT "app0:/audio-in-probe-v1.txt"
static unsigned passes, failures;
static unsigned char pcm[1540] __attribute__((aligned(4)));
static int write_all(int fd,const char *text,unsigned count)
{
    while(count) {
        int n=sceIoWrite(fd,text,count);
        if(n<=0||(unsigned)n>count)return -1;
        text+=n;count-=(unsigned)n;
    }
    return 0;
}
static void report(const char *tag,int ok,uint32_t detail)
{
    char text[128];unsigned n=0;
    const char *status=ok?"PASS ":"FAIL ";
    while(*status)text[n++]=*status++;
    while(*tag&&n<100)text[n++]=*tag++;
    text[n++]=' ';text[n++]='0';text[n++]='x';
    for(unsigned i=0;i<8;i++)text[n++]="0123456789abcdef"[(detail>>(28-4*i))&15];
    text[n++]='\n';
    int fd=sceIoOpen(REPORT,SCE_O_WRONLY|SCE_O_APPEND,0);
    int io=fd<0?-1:write_all(fd,text,n);
    if(fd>=0&&sceIoClose(fd)<0)io=-1;
    if(ok&&!io)passes++;else failures++;
}
static void fill(void)
{ for(unsigned i=0;i<sizeof(pcm);i++)pcm[i]=0xa5; }
static int guarded_silence(unsigned grain)
{
    for(unsigned i=0;i<sizeof(pcm);i++)
        if(pcm[i]!=(i>=2&&i<2+grain*2?0:0xa5))return 0;
    return 1;
}
int module_start(unsigned args,void *argp)
{
    (void)args;(void)argp;
    int fd=sceIoOpen(REPORT,SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0666);
    const char header[]="AUDIO_IN_PROBE version=1\n";
    if(fd<0){sceKernelExitProcess(1);return -1;}
    int io=write_all(fd,header,sizeof(header)-1);
    if(sceIoClose(fd)<0)io=-1;
    if(io){sceKernelExitProcess(1);return -1;}
    report("adopt",sceAudioInGetAdopt(0)==1&&sceAudioInGetAdopt(2)==1,0);
    report("unmuted",sceAudioInGetStatus(1)==0,0);
    report("invalid_freq",sceAudioInOpenPort(0,256,48000,0)==(int)SCE_AUDIO_IN_ERROR_INVALID_SAMPLE_FREQ,0);
    const int configs[][3]={{0,256,16000},{0,512,16000},{2,256,16000},{2,768,48000}};
    int previous=0;
    for(unsigned c=0;c<4;c++) {
        int grain=configs[c][1];
        int port=sceAudioInOpenPort(configs[c][0],grain,configs[c][2],0);
        report("open",port>previous,(uint32_t)port);
        if(port<=previous)break;
        previous=port;
        report("single_port",sceAudioInOpenPort(0,256,16000,0)==(int)SCE_AUDIO_IN_ERROR_PORT_FULL,0);
        report("null_buffer",sceAudioInInput(port,0)==(int)SCE_AUDIO_IN_ERROR_INVALID_POINTER,0);
        /* No logging between capture calls: three successive periods must wait. */
        int rc=0,good=1;
        uint64_t begin=sceKernelGetProcessTimeWide();
        for(unsigned i=0;i<4;i++) {
            fill();
            int read_rc=sceAudioInInput(port,pcm+2);
            if(read_rc)rc=read_rc;
            good=good&&guarded_silence((unsigned)grain);
        }
        uint64_t elapsed=sceKernelGetProcessTimeWide()-begin;
        uint64_t period=(uint64_t)grain*1000000/(unsigned)configs[c][2];
        report("capture",rc==0&&good,(uint32_t)rc);
        report("paced_us",elapsed>=period*3&&elapsed<=2000000,(uint32_t)elapsed);
        report("release",sceAudioInReleasePort(port)==0,0);
        fill();
        rc=sceAudioInInput(port,pcm+2);
        good=1;for(unsigned i=0;i<sizeof(pcm);i++)if(pcm[i]!=0xa5)good=0;
        report("closed_read",rc==(int)SCE_AUDIO_IN_ERROR_NOT_OPENED&&good,(uint32_t)rc);
    }
    report("batch_complete",failures==0&&passes==31,passes);
    sceKernelExitProcess(failures?1:0);
    return failures?-1:0;
}
