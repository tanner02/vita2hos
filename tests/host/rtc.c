#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
static struct timeval clock_value;
static int clock_failure;
static int fake_gettimeofday(struct timeval *out,void *zone)
{ assert(!zone);if (clock_failure) return -1;*out=clock_value;return 0; }
#define gettimeofday fake_gettimeofday
#include "../../source/modules/SceRtc.c"
#undef gettimeofday
#include "../../source/module.c"

int main(void)
{
    _Static_assert(sizeof(SceDateTime)==16,"Calendar ABI");
    int (*set)(SceDateTime *,SceUInt64)=(void *)module_get_func_export(0x6704d3c6,0xA6C36B6A);
    int (*get)(const SceDateTime *,SceUInt64 *)=(void *)module_get_func_export(0x6704d3c6,0xC995DE02);
    int (*now)(SceDateTime *,int)=(void *)module_get_func_export(0x6704d3c6,0x70FDE8F1);
    assert(set && get && now);
    SceDateTime date;
    assert(!set(&date,0) && date.year==1970 && date.month==1 && date.day==1 && !date.microsecond);
    assert(!set(&date,951782400) && date.year==2000 && date.month==2 && date.day==29);
    assert(!set(&date,2147483648ULL) && date.year==2038 && date.month==1 && date.day==19 && date.second==8);
    assert(!set(&date,RTC_LAST_UNIX_SECOND) && date.year==9999 && date.month==12 && date.day==31 && date.hour==23 && date.minute==59 && date.second==59);
    /* Independent libc calendar oracle, with 64-bit host time_t. */
    for (uint64_t sample=0;sample<2000;++sample) {
        SceUInt64 seconds=(sample*UINT64_C(1000000007))%(RTC_LAST_UNIX_SECOND+1),roundtrip=0;
        assert(!set(&date,seconds));
        time_t native=seconds;struct tm expected;assert(gmtime_r(&native,&expected));
        assert(date.year==expected.tm_year+1900 && date.month==expected.tm_mon+1 && date.day==expected.tm_mday);
        assert(date.hour==expected.tm_hour && date.minute==expected.tm_min && date.second==expected.tm_sec);
        date.microsecond=999999;assert(!get(&date,&roundtrip) && roundtrip==seconds);
    }
    SceDateTime saved=date;
    assert((uint32_t)set(&date,UINT64_MAX)==SCE_RTC_ERROR_INVALID_VALUE && !memcmp(&date,&saved,16));
    assert((uint32_t)set(NULL,0)==SCE_RTC_ERROR_INVALID_POINTER);
    const SceDateTime bad[]={
        {.year=0,.month=1,.day=1},{.year=10000,.month=1,.day=1},
        {.year=2000,.month=0,.day=1},{.year=2100,.month=2,.day=29},
        {.year=2000,.month=1,.day=1,.hour=24},{.year=2000,.month=1,.day=1,.minute=60},
        {.year=2000,.month=1,.day=1,.second=60},{.year=2000,.month=1,.day=1,.microsecond=1000000},
        {.year=1969,.month=12,.day=31}};
    const uint32_t errors[]={SCE_RTC_ERROR_INVALID_YEAR,SCE_RTC_ERROR_INVALID_YEAR,
        SCE_RTC_ERROR_INVALID_MONTH,SCE_RTC_ERROR_INVALID_DAY,SCE_RTC_ERROR_INVALID_HOUR,
        SCE_RTC_ERROR_INVALID_MINUTE,SCE_RTC_ERROR_INVALID_SECOND,SCE_RTC_ERROR_INVALID_MICROSECOND,SCE_RTC_ERROR_INVALID_VALUE};
    for (unsigned i=0;i<sizeof(bad)/sizeof(*bad);++i) {
        SceUInt64 output=123;
        assert((uint32_t)get(&bad[i],&output)==errors[i] && output==123);
    }
    SceUInt64 output=123;
    assert((uint32_t)get(NULL,&output)==SCE_RTC_ERROR_INVALID_POINTER && output==123);
    assert((uint32_t)get(&date,NULL)==SCE_RTC_ERROR_INVALID_POINTER);
    clock_value=(struct timeval){.tv_sec=951868799,.tv_usec=123456};
    assert(!now(&date,1) && date.year==2000 && date.month==3 && date.day==1 && date.hour==0 && date.minute==59 && date.second==59 && date.microsecond==123456);
    saved=date;clock_failure=1;
    assert((uint32_t)now(&date,0)==SCE_RTC_ERROR_NOT_INITIALIZED && !memcmp(&date,&saved,16));
    clock_failure=0;clock_value.tv_sec=0;
    assert((uint32_t)now(&date,-1)==SCE_RTC_ERROR_INVALID_VALUE && !memcmp(&date,&saved,16));
    assert((uint32_t)now(NULL,0)==SCE_RTC_ERROR_INVALID_POINTER);
    puts("RTC: NID dispatch, fixed calendar ABI, UTC epochs, leap centuries, 2038, year bounds and failure preservation passed.");
}
