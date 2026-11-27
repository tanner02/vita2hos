#include <psp2/rtc.h>
#include <sys/time.h>
#include <string.h>
#include "module.h"

#define RTC_LAST_UNIX_SECOND UINT64_C(253402300799)
static int rtc_valid_date(const SceDateTime *date)
{
    if (!date) return SCE_RTC_ERROR_INVALID_POINTER;
    if (!date->year || date->year>9999) return SCE_RTC_ERROR_INVALID_YEAR;
    if (!date->month || date->month>12) return SCE_RTC_ERROR_INVALID_MONTH;
    static const unsigned lengths[]={31,28,31,30,31,30,31,31,30,31,30,31};
    unsigned y=date->year;
    bool leap=y%4==0 && (y%100!=0 || y%400==0);
    if (!date->day || date->day>lengths[date->month-1]+(date->month==2 && leap))
        return SCE_RTC_ERROR_INVALID_DAY;
    if (date->hour>23) return SCE_RTC_ERROR_INVALID_HOUR;
    if (date->minute>59) return SCE_RTC_ERROR_INVALID_MINUTE;
    if (date->second>59) return SCE_RTC_ERROR_INVALID_SECOND;
    if (date->microsecond>999999) return SCE_RTC_ERROR_INVALID_MICROSECOND;
    return 0;
}
static int rtc_from_unix(SceDateTime *output,uint64_t seconds,unsigned usec)
{
    if (!output) return SCE_RTC_ERROR_INVALID_POINTER;
    if (seconds>RTC_LAST_UNIX_SECOND || usec>999999) return SCE_RTC_ERROR_INVALID_VALUE;
    /* Gregorian civil date from days since 1970; independent of native time_t,
     * process timezone, DST and C library normalization of invalid dates. */
    uint64_t z=seconds/86400+719468;
    unsigned era=z/146097,doe=z%146097;
    unsigned yoe=(doe-doe/1460+doe/36524-doe/146096)/365;
    unsigned year=yoe+era*400;
    unsigned doy=doe-(365*yoe+yoe/4-yoe/100),mp=(5*doy+2)/153;
    unsigned day=doy-(153*mp+2)/5+1,month=mp<10 ? mp+3 : mp-9;
    year+=month<=2;
    unsigned within=seconds%86400;
    SceDateTime result={.year=year,.month=month,.day=day,.hour=within/3600,
        .minute=within/60%60,.second=within%60,.microsecond=usec};
    *output=result;
    return 0;
}
EXPORT(SceRtcUser,0xA6C36B6A,int,sceRtcSetTime64_t,SceDateTime *date,SceUInt64 seconds)
{
    return rtc_from_unix(date,seconds,0);
}
EXPORT(SceRtcUser,0xC995DE02,int,sceRtcGetTime64_t,const SceDateTime *date,SceUInt64 *seconds)
{
    if (!seconds) return SCE_RTC_ERROR_INVALID_POINTER;
    int result=rtc_valid_date(date);
    if (result) return result;
    /* Unsigned POSIX conversion currently supports epoch through year 9999.
     * Pre-epoch wrap behavior is intentionally not guessed. */
    if (date->year<1970) return SCE_RTC_ERROR_INVALID_VALUE;
    unsigned year=date->year-(date->month<=2),era=year/400,yoe=year-era*400;
    unsigned month=date->month>2 ? date->month-3 : date->month+9;
    unsigned doy=(153*month+2)/5+date->day-1;
    uint64_t days=(uint64_t)era*146097+yoe*365+yoe/4-yoe/100+doy-719468;
    *seconds=days*86400+date->hour*3600+date->minute*60+date->second;
    return 0;
}
EXPORT(SceRtcUser,0x70FDE8F1,int,sceRtcGetCurrentClock,SceDateTime *date,int timezone)
{
    if (!date) return SCE_RTC_ERROR_INVALID_POINTER;
    struct timeval native;
    if (gettimeofday(&native,NULL)<0) return SCE_RTC_ERROR_NOT_INITIALIZED;
    /* The pinned reference interprets this argument as hours. Nonzero zones
     * remain a firmware-validation item; do not inherit the host timezone. */
    int64_t seconds=(int64_t)native.tv_sec;
    int64_t offset=(int64_t)timezone*3600;
    if (seconds<0 || (uint64_t)seconds>RTC_LAST_UNIX_SECOND ||
        native.tv_usec<0 || native.tv_usec>999999)
        return SCE_RTC_ERROR_INVALID_VALUE;
    seconds+=offset; /* Both bounded operands fit int64_t. */
    if (seconds<0) return SCE_RTC_ERROR_INVALID_VALUE;
    return rtc_from_unix(date,(uint64_t)seconds,native.tv_usec);
}
DECLARE_LIBRARY(SceRtcUser,0x6704D3C6);

/* ZCode batch additions: remaining RtcUser and SceRtc library functions.
 * Tick epoch is 0001-01-01 00:00:00 UTC with a 1,000,000 Hz resolution. */

#define RTC_TICK_EPOCH_SECONDS UINT64_C(62135596800)
#define RTC_TICK_RESOLUTION UINT64_C(1000000)

EXPORT(SceRtcUser,0x811313B3,unsigned int,sceRtcGetTickResolution,void)
{
    return (unsigned)RTC_TICK_RESOLUTION;
}

EXPORT(SceRtcUser,0x23F79274,int,sceRtcGetCurrentTick,SceRtcTick *tick)
{
    if(!tick)return SCE_RTC_ERROR_INVALID_POINTER;
    struct timeval native;
    if(gettimeofday(&native,NULL)<0)return SCE_RTC_ERROR_NOT_INITIALIZED;
    tick->tick=(uint64_t)native.tv_sec*RTC_TICK_RESOLUTION+(uint64_t)native.tv_usec
        +RTC_TICK_EPOCH_SECONDS*RTC_TICK_RESOLUTION;
    return 0;
}

EXPORT(SceRtcUser,0xCDDD25FE,int,sceRtcGetCurrentNetworkTick,SceRtcTick *tick)
{
    /* No network time source; the console clock is authoritative here. */
    return sceRtcGetCurrentTick(tick);
}

EXPORT(SceRtcUser,0x0572EDDC,int,sceRtcGetCurrentClockLocalTime,SceDateTime *date)
{
    /* No host timezone model; the console clock reads as UTC. */
    return sceRtcGetCurrentClock(date,0);
}

EXPORT(SceRtcUser,0x3A332F81,int,sceRtcSetTime_t,SceDateTime *date,time_t seconds)
{
    if(seconds<0)return SCE_RTC_ERROR_INVALID_VALUE;
    return rtc_from_unix(date,(uint64_t)seconds,0);
}

EXPORT(SceRtcUser,0x2F3531EB,int,sceRtcGetDayOfWeek,int year,int month,int day)
{
    /* Days-from-civil (Howard Hinnant), then 1970-01-01 was a Thursday. */
    int y=(int)year-(month<=2),era=(y>=0?y:y-399)/400,yoe=era*400+(y-era*400);
    unsigned m=(int)month+(month>2?-3:9),doy=(153*m+2)/5+day-1,doe=yoe*365+yoe/4-yoe/100+doy;
    uint64_t days=(uint64_t)((int64_t)doe-719468);
    return (unsigned)((days+4)%7);
}

EXPORT(SceRtcUser,0xF2B238E2,int,sceRtcGetTick,const SceDateTime *time,SceRtcTick *tick)
{
    if(!tick)return SCE_RTC_ERROR_INVALID_POINTER;
    int valid=rtc_valid_date(time);
    if(valid)return valid;
    int y=(int)time->year-(time->month<=2),era=(y>=0?y:y-399)/400,yoe=era*400+(y-era*400);
    unsigned m=(unsigned)((int)time->month+(time->month>2?-3:9));
    unsigned doy=(153*m+2)/5+time->day-1,doe=yoe*365+yoe/4-yoe/100+doy;
    uint64_t days=(uint64_t)((int64_t)doe-719468);
    tick->tick=(days*86400ULL+time->hour*3600ULL+time->minute*60ULL+time->second)
        *RTC_TICK_RESOLUTION+time->microsecond
        +RTC_TICK_EPOCH_SECONDS*RTC_TICK_RESOLUTION;
    return 0;
}

EXPORT(SceRtc,0x258BE8EC,SceULong64,sceRtcGetAccumulativeTime,void)
{
    static struct timeval start;
    static bool started;
    struct timeval now;
    gettimeofday(&now,NULL);
    if(!started){start=now;started=true;}
    return (SceULong64)(now.tv_sec-start.tv_sec);
}

DECLARE_LIBRARY(SceRtc,0x3503487E);
