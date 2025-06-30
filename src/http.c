#include "http.h"

#include "pd_api.h"
#include "utility.h"

static enable_cb_t _cb;
static void* _ud;
static bool permission = false;
char* _domain = NULL;
char* _reason = NULL;

struct HTTPUD {
    http_result_cb;
    char* domain;
    char* path;
    char* data;
    size_t data_len;
    int timeout;
    unsigned flags;
    void* ud;
};

void http_cleanup(HTTPConnection* connection)
{
    struct HTTPUD* httpud = playdate->network->http->getUserdata(connection);
    playdate->network->http->setUserdata(connection, NULL);
    
    if (httpud)
    {
        if (httpud->cb)
        {
            // resort to error if cleanup and cb not yet called
            httpud->cb(
                HTTP_ERROR | httpud->flags,
                httpud->ud;
            );
        }
        
        if (httpud->data) free(httpud->data);
        free(httpud->domain);
        free(httpud->path);
        free(httpud);
    }
    
    playdate->network->http->release(
        connection
    );
    playdate->network->http->close(connection);
}

void CB_Header(HTTPConnection* connection, const char* key, const char* value)
{
    printf("Header received: \"%s\": \"%s\"\n", key, value);
}

void CB_HeadersRead(HTTPConnection* connection)
{
    printf("Headers read\n");
}

void CB_Closed(HTTPConnection* connection)
{
    http_cleanup(connection);
}

void CB_RequestComplete(HTTPConnection* connection)
{
    struct HTTPUD* httpud = playdate->network->http->getUserdata(connection);
    http_cleanup(connection);
}

void CB_Permission(unsigned flags, void* ud)
{
    struct HTTPUD* httpud = ud;
    
    httpud->flags = flags;
    bool allowed = (flags & ~HTTP_ENABLE_ASKED) == 0;
    
    if (allowed)
    {
        HTTPConnection* connection = playdate->network->http->newConnection(
            httpud->domain, 0, USE_SSL
        );
        
        if (!connection) goto fail;
        
        // 10 seconds
        playdate->network->http->setConnectTimeout(connection, httpud->timeout);
        
        playdate->network->http->setUserdata(connection, httpud);
        playdate->network->http->retain(
            connection
        );
        
        playdate->network->http->setHeaderReceivedCallback(connection, CB_Header);
        playdate->network->http->setHeadersReadCallback(
            connection, CB_HeadersRead
        );
        playdate->network->http->setConnectionClosedCallback(
            connection, CB_Closed
        );
        playdate->network->http->setResponseCallback(connection, CB_Response);
        playdate->network->http->setRequestCompleteCallback(connection, CB_RequestComplete);
        
        PDNetErr err = playdate->network->http->get(connection, httpud->path, NULL, 0);
        if (err != NET_OK)
        {
            flags |= HTTP_ERROR;
            goto release_and_fail;
        }
        
        printf("HTTP get, no immediate error\n");
        
        return;
        
    release_and_fail:
        httpud->cb(flags, httpud->ud);
        httpud->cb = NULL;
        http_cleanup(connection);
    }
    else
    {
    fail:
        httpud->cb(flags, httpud->ud);
        httpud->cb = NULL;
        http_cleanup(connection);
    }
}

void http_get(
    const char* domain,
    const char* path,
    const char* reason,
    http_result_cb cb,
    int timeout,
    void* ud
) {
    struct HTTPUD* httpud = malloc(sizeof(HTTPUD));
    if (!httpud)
    {
        cb(HTTP_MEM_ERROR, ud);
        return;
    }
    
    memset(httpud, 0, sizeof(*httpud));
    httpud->cb = cb;
    httpud->ud = ud;
    httpud->timeout = timeout;
    httpud->domain = strdup(domain);
    httpud->path = strdup(path);
    
    enable_http(
        domain, reason, CB_Permission, httpud
    );
}

struct CB_UserData_EnableHTTP {
    enable_cb_t cb;
    void* ud;
};

static void CB_AccessReply(bool result, void* cbud)
{
    enable_cb_t cb = ((struct CB_UserData_EnableHTTP*)cbud)->cb;
    void* ud = ((struct CB_UserData_EnableHTTP*)cbud)->ud;
    free(cbud);
    
    permission = result;
    cb(HTTP_ENABLE_ASKED | (result ? 0 : HTTP_ENABLE_DENIED), ud);
}

static void CB_SetEnabled(PDNetErr err)
{
    enable_cb_t cb = _cb;
    void* ud = _ud;
    _cb = NULL;
    
    if (err != NET_OK)
    {
        cb(HTTP_ERROR, ud);
    }
    else
    {
        if (permission)
        {
            cb(0, ud);
        }
        else
        {
            struct CB_UserData_EnableHTTP* cbudhttp = malloc(sizeof(struct CB_UserData_EnableHTTP));
            cbudhttp->cb = cb;
            cbudhttp->ud = ud;
            
            if (!cbudhttp)
            {
                cb(HTTP_MEM_ERROR, ud);
                return;
            }
            
            enum accessReply result = playdate->network->http->requestAccess(
                _domain, 0, USE_SSL, _reason, CB_AccessReply, cbudhttp
            );
            
            switch(result) {
            case kAccessAsk:
                printf("Asked for permission\n");
                // callback will be invoked.
                return;
            case kAccessDeny:
                free(cbudhttp);
                cb(HTTP_ENABLE_DENIED, ud);
                return;
            case kAccessAllow:
                permission = true;
                free(cbudhttp);
                cb(0, ud);
                return;
            default:
                free(cbudhttp);
                printf("Unrecognized permission result: %d\n", result);
                cb(HTTP_ERROR, ud);
                break;
            }
        }
    }
    
    free(_domain);
    free(_reason);
}

void enable_http(
    const char* domain,
    const char* reason,
    enable_cb_t cb,
    void* ud
)
{
    if (_cb != NULL)
    {
        cb(HTTP_ENABLE_IN_PROGRESS, ud);
        return;
    }

    _ud = ud;
    _cb = cb;
    _domain = strdup(domain);
    _reason = strdup(reason);

    playdate->network->setEnabled(true, CB_SetEnabled);
}