#include "version.h"

#include "pd_api.h"
#include "utility.h"
#include "jparse.h"

#define ERRMEM -255
#define STR_ERRMEM "malloc error"

#define USE_SSL true

struct CB_UserData {
    update_result_cb cb;
    void* ud;
    char* data;
    size_t data_read;
};

static struct CB_UserData static_cbud;
static bool permission = false;

struct VersionInfo
{
    char* name;
    char* domain;
    char* url;
};
static struct VersionInfo* localVersionInfo = NULL;

// returns 0 on success, or
// returns nonzero failure code
static int read_version_info(const char* text, bool ispath, struct VersionInfo* oinfo)
{
    json_value jvinfo;
    
    if (oinfo->name) free(oinfo->name);
    if (oinfo->domain) free(oinfo->domain);
    if (oinfo->url) free(oinfo->url);

    int jparse_result = (ispath)
        ? parse_json("version.json", &jvinfo, kFileRead | kFileReadData)
        : parse_json_string(text, &jvinfo);
    
    if (jparse_result == 0 || jvinfo.type != kJSONTable)
    {
        free_json_data(jvinfo);
        return -1;
    }
    
    json_value jname = json_get_table_value(jvinfo, "name");
    json_value jurl = json_get_table_value(jvinfo, "url");
    json_value jdomain = json_get_table_value(jvinfo, "domain");
    
    if (jname.type != kJSONString || jurl.type != kJSONString || jdomain.type != kJSONString)
    {
        free_json_data(jvinfo);
        return -2;
    }
    
    oinfo->name = strdup(jname.data.stringval);
    oinfo->url = strdup(jurl.data.stringval);
    oinfo->domain = strdup(jdomain.data.stringval);
    
    free_json_data(jvinfo);
    
    return 0;
}

void CB_Header(HTTPConnection* connection, const char* key, const char* value)
{
    printf("Header received: \"%s\": \"%s\"\n", key, value);
}

void CB_HeadersRead(HTTPConnection* connection)
{
    printf("Headers read\n");
    
    playdate->network->http->release(
        connection
    );
    playdate->network->http->close(connection);
}

void CB_Closed(HTTPConnection* connection)
{
    struct CB_UserData* cbud = playdate->network->http->getUserdata(connection);
    
    if (!cbud)
    {
        return;
    }
    
    playdate->network->http->setUserdata(connection, NULL);
    cbud->cb(-150, "Server closed request before version information was received", cbud->ud);
    if (cbud->data) free(cbud->data);
    free(cbud);
}

void CB_Response(HTTPConnection* connection)
{
    struct CB_UserData* cbud = playdate->network->http->getUserdata(connection);
    
    if (!cbud)
    {
        return;
    }
    
    int response = playdate->network->http->getResponseStatus(connection);
    if (response != 0 && response != 200)
    {
        char* s = aprintf("HTTP status code: %d", response);
        cbud->cb(-response - 1000, s, cbud->ud);
        free(s);
        playdate->network->http->setUserdata(connection, NULL);
        if (cbud->data) free(cbud->data);
        free(cbud);
        return;
    }
    
    // status 200, data arrived
    
    size_t available = playdate->network->http->getBytesAvailable(connection);
    if (available > 0)
    {
        cbud->data = realloc(cbud->data, available + cbud->data_read);
        int read = playdate->network->http->read(
            connection, cbud->data + cbud->data_read + 1, available
        );
        printf("read: %d/%u\n", read, (unsigned)available);
        cbud->data_read += read;
        
        // paranoid terminal zero
        cbud->data[cbud->data_read] = 0;
        
        // only try parsing if a '{' and '}' are in the data
        if (strrchr(cbud->data, '}') && strchr(cbud->data, '{'))
        {
            // try parsing json
            json_value jv;
            int result = parse_json_string(strchr(cbud->data, '{'), &jv);
            
            // result of 0 means we couldn't parse; there must be more still on the way.
            // (No need to json_free_data(jv) in this case.)
            if (result == 0) return;
            
            // otherwise, we're done! Validate result:
            json_value jname = json_get_table_value(jv, "name");
            
            if (jname.type == kJSONString && strlen(jname.data.stringval) > 0)
            {
                if (strcmp(jname.data.stringval, localVersionInfo->name))
                {
                    // new version available
                    cbud->cb(1, jname.data.stringval, cbud->ud);
                }
                else
                {
                    cbud->cb(0, "No update available.", cbud->ud);
                }
            }
            else
            {
            invalid:
                cbud->cb(-650, "Invalid version information receieved", cbud->ud);
            }
            
            free_json_data(jv);
            
            playdate->network->http->setUserdata(connection, NULL);
            free(cbud->data);
            free(cbud);
            return;
        }
        
        // TODO: cbud->cb error if 100% of data has arrived.
    }
}

void CB_Permission(bool allowed, void* _cbud)
{
    struct CB_UserData* cbud = _cbud;
    
    int status = -102;
    const char* msg = "HTTP request failed";
    
    if (allowed)
    {
        HTTPConnection* connection = playdate->network->http->newConnection(
            localVersionInfo->domain, 0, USE_SSL
        );
        
        if (!connection) goto fail;
        
        // 10 seconds
        playdate->network->http->setConnectTimeout(connection, 10*1000);
        
        playdate->network->http->setUserdata(connection, cbud);
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
        
        PDNetErr err = playdate->network->http->get(connection, localVersionInfo->url, NULL, 0);
        if (err != NET_OK) goto release_and_fail;
        
        printf("HTTP get, no immediate error\n");
        
        return;
        
    release_and_fail:
        playdate->network->http->release(
            connection
        );
        playdate->network->http->close(connection);
        
        goto fail;
    }
    else
    {
        status = -253;
        msg = "Permission denied";
    fail:
        cbud->cb(status, msg, cbud->ud);
        if (cbud->data) free(cbud->data);
        free(cbud);
    }
}

void CB_SetEnabled(PDNetErr err)
{
    update_result_cb cb = static_cbud.cb;
    void* ud = static_cbud.ud;
    static_cbud.cb = NULL;
    
    if (err != NET_OK)
    {
        cb(err, "Error enabling network", ud);
    }
    else
    {
        struct CB_UserData* cbud = malloc(sizeof(struct CB_UserData));
        if (!cbud)
        {
            cb(ERRMEM, STR_ERRMEM, ud);
        }
        cbud->cb = cb;
        cbud->ud = ud;
        cbud->data = NULL;
        cbud->data_read = 0;
        
        if (permission)
        {
            CB_Permission(true, cbud);
        }
        else
        {
            enum accessReply result = playdate->network->http->requestAccess(
                localVersionInfo->domain, 0, USE_SSL, "to check for a version update",
                &CB_Permission, cbud
            );
            
            switch(result) {
            case kAccessAsk:
                printf("Asked for permission\n");
                // callback will be invoked
                return;
            case kAccessDeny:
                cb(-103, "Network permission denied. Adjust your Playdate's settings.", ud);
                return;
            case kAccessAllow:
                CB_Permission(true, cbud);
                return;
            default:
                printf("Permission result: %d\n", result);
                cb(-104, "Unrecognized network permission request result.", ud);
                break;
            }
        }
    }
}

void check_for_updates(update_result_cb cb, void* ud)
{
    if (static_cbud.cb != NULL)
    {
        cb(-10, "Update check in progress", ud);
        return;
    }
    
    static_cbud.cb = cb;
    static_cbud.ud = ud;

    if (!localVersionInfo)
    {
        localVersionInfo = malloc(sizeof(struct VersionInfo));
        if (!localVersionInfo)
        {
            cb(ERRMEM, STR_ERRMEM, ud);
            static_cbud.cb = NULL;
            return;
        }
        memset(localVersionInfo, 0, sizeof(*localVersionInfo));

        int result;
        if ((result = read_version_info("version.json", true, localVersionInfo)))
        {
            cb(result, "Error getting current version", ud);
            static_cbud.cb = NULL;
            free(localVersionInfo); localVersionInfo = NULL; return;
        }
    }
    
    playdate->network->setEnabled(true, CB_SetEnabled);
}