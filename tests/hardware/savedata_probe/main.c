#include <psp2/apputil.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/kernel/processmgr.h>
#include <stdint.h>
#include <stddef.h>
void *memset(void *out,int value,size_t size)
{unsigned char *p=out;for(size_t i=0;i<size;i++)p[i]=(unsigned char)value;return out;}
void *memcpy(void *out,const void *in,size_t size)
{unsigned char *p=out;const unsigned char *q=in;for(size_t i=0;i<size;i++)p[i]=q[i];return out;}
static SceUID report=-1;
static void record(const char *text)
{
    unsigned size=0;while(text[size])size++;
    while(size && report>=0){int n=sceIoWrite(report,text,size);if(n<=0){report=-1;return;}text+=n;size-=(unsigned)n;}
}
static void failure(const char *text,int code)
{
    const char hex[]="0123456789abcdef";char value[12]="0x00000000\n";
    for(unsigned i=0;i<8;i++)value[2+i]=hex[((uint32_t)code>>(28-i*4))&15];
    record("FAIL ");record(text);record(" ");record(value);
}
int module_start(SceSize args,void *argp)
{
    (void)args;(void)argp;int status=1,active=0,code;SceUID fd=-1;
    const unsigned slot_id=0x56483253;
    report=sceIoOpen("app0:/savedata-probe.txt",SCE_O_WRONLY|SCE_O_CREAT|SCE_O_TRUNC,0600);
    if(report<0){sceKernelExitProcess(1);return 1;}
    record("savedata_probe v1\nRUNNING - require final PASS and clean runtime exit\n");
#define CHECK(expression) do{code=(expression);if(code){failure(#expression,code);goto done;}}while(0)
    /* Refuse game metadata: the probe may write only its owned namespace. */
    unsigned char actual_psf[65],owned_psf[64]={0};
    owned_psf[1]='P';owned_psf[2]='S';owned_psf[3]='F';owned_psf[4]=1;owned_psf[5]=1;
    owned_psf[8]=36;owned_psf[12]=48;owned_psf[16]=1;owned_psf[22]=4;owned_psf[23]=2;
    owned_psf[24]=10;owned_psf[28]=16;memcpy(owned_psf+36,"TITLE_ID",9);memcpy(owned_psf+48,"VHOS00001",10);
    fd=sceIoOpen("app0:/sce_sys/param.sfo",SCE_O_RDONLY,0);
    if(fd<0){failure("owned_param_missing",fd);goto done;}
    code=sceIoRead(fd,actual_psf,sizeof(actual_psf));
    if(code!=64){failure("owned_param_size",code);goto done;}
    for(unsigned i=0;i<64;i++)if(actual_psf[i]!=owned_psf[i]){failure("owned_param_identity",(int)i);goto done;}
    CHECK(sceIoClose(fd));fd=-1;record("PASS owned_metadata_identity\n");
    SceAppUtilInitParam init={0};SceAppUtilBootParam boot={0};
    SceAppUtilSaveDataSlotParam param={0},output={0};param.userParam=0x12345678;
    SceAppUtilSaveDataFileSlot slot={0};slot.id=slot_id;slot.slotParam=&param;
    SceAppUtilSaveDataFile item={0};item.filePath="probe-save.bin";item.buf="abcdef";item.bufSize=6;
    SceSize required=0xfeed;
    CHECK(sceAppUtilInit(&init,&boot));active=1;
    code=sceAppUtilSaveDataSlotDelete(slot_id,NULL);
    if(code && (uint32_t)code!=SCE_APPUTIL_ERROR_SAVEDATA_SLOT_NOT_FOUND){failure("reset_owned_slot",code);goto done;}
    CHECK(sceAppUtilSaveDataDataSave(&slot,&item,1,NULL,&required));
    if(required){failure("required_size_on_success",(int)required);goto done;}
    CHECK(sceAppUtilSaveDataSlotGetParam(slot_id,&output,NULL));
    if(output.userParam!=param.userParam || output.status){failure("slot_roundtrip",output.userParam);goto done;}
    record("PASS first_save_and_slot\n");
    /* Updates require replacing an existing metadata record on the actual FS. */
    param.userParam=0x76543210;item.offset=2;item.buf="XY";item.bufSize=2;
    CHECK(sceAppUtilSaveDataDataSave(&slot,&item,1,NULL,NULL));
    CHECK(sceAppUtilShutdown());active=0;
    CHECK(sceAppUtilInit(&init,&boot));active=1;
    CHECK(sceAppUtilSaveDataSlotGetParam(slot_id,&output,NULL));
    if(output.userParam!=param.userParam){failure("updated_slot_reopen",output.userParam);goto done;}
    fd=sceIoOpen("savedata0:/probe-save.bin",SCE_O_RDONLY,0);
    if(fd<0){failure("reopen_file",fd);goto done;}
    char bytes[6];code=sceIoRead(fd,bytes,6);
    if(code!=6){failure("read_file",code);goto done;}
    const char expected[]="abXYef";
    for(unsigned i=0;i<6;i++)if(bytes[i]!=expected[i]){failure("offset_bytes",(int)i);goto done;}
    CHECK(sceIoClose(fd));fd=-1;record("PASS update_flush_and_reopen\n");
    item.mode=1;item.buf="Z";item.bufSize=1;
    CHECK(sceAppUtilSaveDataDataSave(NULL,&item,1,NULL,NULL));
    SceIoStat info={0};CHECK(sceIoGetstat("savedata0:/probe-save.bin",&info));
    if(info.st_size!=3){failure("truncate_size",(int)info.st_size);goto done;}
    fd=sceIoOpen("savedata0:/probe-save.bin",SCE_O_RDONLY,0);
    if(fd<0){failure("truncate_reopen",fd);goto done;}
    code=sceIoRead(fd,bytes,4);
    if(code!=3 || bytes[0]!='a' || bytes[1]!='b' || bytes[2]!='Z'){failure("truncate_bytes",code);goto done;}
    CHECK(sceIoClose(fd));fd=-1;
    record("PASS truncate\n");
    CHECK(sceIoRemove("savedata0:/probe-save.bin"));
    CHECK(sceAppUtilSaveDataSlotDelete(slot_id,NULL));
    CHECK(sceAppUtilShutdown());active=0;
    code=sceIoOpen("savedata0:/probe-save.bin",SCE_O_RDONLY,0);
    if((uint32_t)code!=0x80010013){if(code>=0)sceIoClose(code);failure("revoked_mount",code);goto done;}
    status=0;
done:
    if(fd>=0)sceIoClose(fd);
    if(active && sceAppUtilShutdown())status=1;
    record(status ? "FAIL savedata_probe\n" : "PASS savedata_probe\n");
    if(report>=0)sceIoClose(report);
    sceKernelExitProcess(status);return status;
}
