#include <switch.h>
#include "module.h"
#include "log.h"

/* HTTP library bring-up: LittleBigPlanet builds an HTTP template and request
 * chain during boot. Library NID 0xE8F15CDE per vitasdk db/360. No host
 * transport exists: object creation succeeds with local ids and requests
 * fail with SCE_HTTP_ERROR_NETWORK so the game falls back to offline mode. */

#define SCE_HTTP_ERROR_NETWORK     0x80431063
#define SCE_HTTP_ERROR_BEFORE_SEND 0x80431065
#define SCE_HTTP_ERROR_INVALID_ID   0x80431100
#define SCE_HTTP_ERROR_INVALID_VALUE 0x804311FE

static Mutex http_lock;
static bool http_initialized;
static int next_template_id,next_connection_id,next_request_id;

EXPORT(SceHttp,0x214926D9,int,sceHttpInit,unsigned int poolSize)
{
    mutexLock(&http_lock);
    if(http_initialized){mutexUnlock(&http_lock);return 0;}
    LOG("sceHttpInit poolSize=0x%x",(unsigned)poolSize);
    http_initialized=true;
    next_template_id=next_connection_id=next_request_id=0;
    mutexUnlock(&http_lock);
    return 0;
}

EXPORT(SceHttp,0xC9076666,int,sceHttpTerm,void)
{
    mutexLock(&http_lock);
    LOG("sceHttpTerm");
    http_initialized=false;
    next_template_id=next_connection_id=next_request_id=0;
    mutexUnlock(&http_lock);
    return 0;
}

/* Creates a template: returns a positive local id (0 is reserved by callers
 * that treat template ids as signed "invalid" sentinels). */
EXPORT(SceHttp,0x62241DAB,int,sceHttpCreateTemplate,const char *name,unsigned int httpVersion,unsigned int configMode)
{
    mutexLock(&http_lock);
    int result=http_initialized ? ++next_template_id : (int)SCE_HTTP_ERROR_BEFORE_SEND;
    mutexUnlock(&http_lock);
    LOG("sceHttpCreateTemplate name=%s httpVersion=0x%x configMode=%u -> id=%d",
        name?name:"(null)",httpVersion,configMode,result);
    return result;
}

EXPORT(SceHttp,0xEC85ECFB,int,sceHttpDeleteTemplate,int templateId)
{
    LOG("sceHttpDeleteTemplate id=%d",templateId);
    return 0;
}

EXPORT(SceHttp,0xAEB3307E,int,sceHttpCreateConnection,int templateId,const char *serverName,const char *scheme,unsigned short port,int enableKeepalive)
{
    mutexLock(&http_lock);
    int result=http_initialized ? ++next_connection_id : (int)SCE_HTTP_ERROR_BEFORE_SEND;
    mutexUnlock(&http_lock);
    LOG("sceHttpCreateConnection tmpl=%d server=%s scheme=%s port=%u keepalive=%d -> id=%d",
        templateId,serverName?serverName:"(null)",scheme?scheme:"(null)",(unsigned)port,enableKeepalive,result);
    return result;
}

EXPORT(SceHttp,0xF0F65C15,int,sceHttpDeleteConnection,int connectionId)
{
    LOG("sceHttpDeleteConnection id=%d",connectionId);
    return 0;
}

EXPORT(SceHttp,0xB0284270,int,sceHttpCreateRequest,int connectionId,int method,const char *path,unsigned long long contentLength)
{
    mutexLock(&http_lock);
    int result=http_initialized ? ++next_request_id : (int)SCE_HTTP_ERROR_BEFORE_SEND;
    mutexUnlock(&http_lock);
    LOG("sceHttpCreateRequest conn=%d method=%d path=%s contentLength=%llu -> id=%d",
        connectionId,method,path?path:"(null)",(unsigned long long)contentLength,result);
    return result;
}

EXPORT(SceHttp,0x3D3D29AD,int,sceHttpDeleteRequest,int requestId)
{
    LOG("sceHttpDeleteRequest id=%d",requestId);
    return 0;
}

/* No host transport: every send reports an unreachable network. LBP handles
 * request failure by degrading to offline play. */
EXPORT(SceHttp,0x9CA58B99,int,sceHttpSendRequest,int requestId,const void *postData,unsigned int size)
{
    LOG("sceHttpSendRequest id=%d postData=%p size=%u -> network unavailable",
        requestId,(void*)postData,(unsigned)size);
    return (int)SCE_HTTP_ERROR_NETWORK;
}

EXPORT(SceHttp,0xEA61662F,int,sceHttpAbortRequest,int requestId)
{
    LOG("sceHttpAbortRequest id=%d",requestId);
    return 0;
}

EXPORT(SceHttp,0x8AE3F008,int,sceHttpSetSendTimeOut,int templateId,unsigned int timeOutUSec)
{
    LOG("sceHttpSetSendTimeOut id=%d timeOutUSec=%u",templateId,timeOutUSec);
    return 0;
}

EXPORT(SceHttp,0x94BF196E,int,sceHttpSetRecvTimeOut,int templateId,unsigned int timeOutUSec)
{
    LOG("sceHttpSetRecvTimeOut id=%d timeOutUSec=%u",templateId,timeOutUSec);
    return 0;
}

EXPORT(SceHttp,0x237CA86E,int,sceHttpSetConnectTimeOut,int templateId,unsigned int timeOutUSec)
{
    LOG("sceHttpSetConnectTimeOut id=%d timeOutUSec=%u",templateId,timeOutUSec);
    return 0;
}

EXPORT(SceHttp,0x4E08167D,int,sceHttpSetRedirectCallback,int templateId,void *callback,void *userArg)
{
    LOG("sceHttpSetRedirectCallback id=%d callback=%p",templateId,callback);
    return 0;
}

EXPORT(SceHttp,0xE0A3A88D,int,sceHttpSetAuthInfoCallback,int templateId,void *callback,void *userArg)
{
    LOG("sceHttpSetAuthInfoCallback id=%d callback=%p",templateId,callback);
    return 0;
}

EXPORT(SceHttp,0xAEE573A3,int,sceHttpSetCookieEnabled,int templateId,int enable)
{
    LOG("sceHttpSetCookieEnabled id=%d enable=%d",templateId,enable);
    return 0;
}

EXPORT(SceHttp,0x7B51B122,int,sceHttpAddRequestHeader,int requestId,const char *name,const char *value)
{
    LOG("sceHttpAddRequestHeader id=%d name=%s value=%s",requestId,name?name:"(null)",value?value:"(null)");
    return 0;
}

/* Requests never complete without a transport, so response accessors report
 * the before-send state. */
EXPORT(SceHttp,0x27071691,int,sceHttpGetStatusCode,int requestId,int *statusCode)
{
    LOG("sceHttpGetStatusCode id=%d -> before send",requestId);
    return (int)SCE_HTTP_ERROR_BEFORE_SEND;
}

EXPORT(SceHttp,0xF580D304,int,sceHttpGetResponseContentLength,int requestId,unsigned long long *contentLength)
{
    LOG("sceHttpGetResponseContentLength id=%d -> before send",requestId);
    return (int)SCE_HTTP_ERROR_BEFORE_SEND;
}

EXPORT(SceHttp,0x11F6C27F,int,sceHttpGetAllResponseHeaders,int requestId,void *headerArea,unsigned int *headerSize)
{
    LOG("sceHttpGetAllResponseHeaders id=%d -> before send",requestId);
    return (int)SCE_HTTP_ERROR_BEFORE_SEND;
}

EXPORT(SceHttp,0x03A6C89E,int,sceHttpParseResponseHeader,const void *headerArea,unsigned int headerSize,void *container)
{
    LOG("sceHttpParseResponseHeader area=%p size=%u",headerArea,(unsigned)headerSize);
    return (int)SCE_HTTP_ERROR_BEFORE_SEND;
}

EXPORT(SceHttp,0x7EDE3979,int,sceHttpReadData,int requestId,void *buffer,unsigned int size)
{
    LOG("sceHttpReadData id=%d size=%u -> 0 (closed)",requestId,(unsigned)size);
    return 0;
}

EXPORT(SceHttp,0x2B79BDE0,int,sceHttpsGetSslError,int templateId,unsigned int *errCode)
{
    if(errCode)*errCode=0;
    LOG("sceHttpsGetSslError id=%d -> 0",templateId);
    return 0;
}

EXPORT(SceHttp,0xA0926037,int,sceHttpsSetSslCallback,int templateId,void *callback,void *userArg)
{
    LOG("sceHttpsSetSslCallback id=%d callback=%p",templateId,callback);
    return 0;
}

EXPORT(SceHttp,0xAE8D7C33,int,sceHttpsLoadCert,void)
{
    LOG("sceHttpsLoadCert (no host certificate store)");
    return 0;
}

EXPORT(SceHttp,0x47664424,int,sceHttpUriBuild,void *out,const unsigned int *outLen,const char *scheme,const char *hostname,const unsigned short port,const char *path,const char *urlParam,const unsigned int *option)
{
    LOG("sceHttpUriBuild scheme=%s host=%s port=%u",scheme?scheme:"(null)",hostname?hostname:"(null)",(unsigned)port);
    return (int)SCE_HTTP_ERROR_INVALID_VALUE;
}

DECLARE_LIBRARY(SceHttp,0xE8F15CDE);
