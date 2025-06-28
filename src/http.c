#include "http.h"

#include "pd_api.h"
#include "utility.h"

static enable_cb_t _cb;
static void* _ud;
static bool permission = false;
char* _domain = NULL;
char* _reason = NULL;

struct CB_UserData_HTTP {
    enable_cb_t cb;
    void* ud;
};

static void CB_AccessReply(bool result, void* cbud)
{
    enable_cb_t cb = ((struct CB_UserData_HTTP*)cbud)->cb;
    void* ud = ((struct CB_UserData_HTTP*)cbud)->ud;
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
            struct CB_UserData_HTTP* cbudhttp = malloc(sizeof(struct CB_UserData_HTTP));
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