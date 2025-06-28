#include "version.h"

#include "pd_api.h"
#include "utility.h"
#include "jparse.h"
#include "http.h"

#define ERRMEM -255
#define STR_ERRMEM "malloc error"

struct CB_UserData {
    update_result_cb cb;
    void* ud;
    
    // optional:
    char* data;
    size_t data_read;
};

struct VersionInfo
{
    char* name;
    char* domain;
    char* path;
};
static struct VersionInfo* localVersionInfo = NULL;

// returns 0 on success, or
// returns nonzero failure code
static int read_version_info(const char* text, bool ispath, struct VersionInfo* oinfo)
{
    json_value jvinfo;
    
    if (oinfo->name) free(oinfo->name);
    if (oinfo->domain) free(oinfo->domain);
    if (oinfo->path) free(oinfo->path);

    int jparse_result = (ispath)
        ? parse_json("version.json", &jvinfo, kFileRead | kFileReadData)
        : parse_json_string(text, &jvinfo);
    
    if (jparse_result == 0 || jvinfo.type != kJSONTable)
    {
        free_json_data(jvinfo);
        return -1;
    }
    
    json_value jname = json_get_table_value(jvinfo, "name");
    json_value jpath = json_get_table_value(jvinfo, "path");
    json_value jdomain = json_get_table_value(jvinfo, "domain");
    
    if (jname.type != kJSONString || jpath.type != kJSONString || jdomain.type != kJSONString)
    {
        free_json_data(jvinfo);
        return -2;
    }
    
    oinfo->name = strdup(jname.data.stringval);
    oinfo->path = strdup(jpath.data.stringval);
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

static int read_local_version(void)
{
    if (!localVersionInfo)
    {
        localVersionInfo = malloc(sizeof(struct VersionInfo));
        if (!localVersionInfo)
        {
            return -1;
        }
        memset(localVersionInfo, 0, sizeof(*localVersionInfo));

        int result;
        if ((result = read_version_info("version.json", true, localVersionInfo)))
        {
            free(localVersionInfo); localVersionInfo = NULL;
            return -2;
        }
    }
    
    return 1;
}

void CB_Permission(unsigned flags, void* _cbud)
{
    struct CB_UserData* cbud = _cbud;
    
    int status = -102;
    const char* msg = "HTTP request failed";
    
    bool allowed = (flags & ~HTTP_ENABLE_ASKED) == 0;
    
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
        
        PDNetErr err = playdate->network->http->get(connection, localVersionInfo->path, NULL, 0);
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
        if (flags & HTTP_ENABLE_ASKED)
        {
            status = ERR_PERMISSION_ASKED_DENIED;
        }
        else
        {
            status = ERR_PERMISSION_DENIED;
        }
        msg = "Permission denied";
    fail:
        cbud->cb(status, msg, cbud->ud);
        if (cbud->data) free(cbud->data);
        free(cbud);
    }
}


const char* get_current_version(void)
{
    if (read_local_version() == 1)
    {
        return localVersionInfo->name;
    }
    else
    {
        return NULL;
    }
}


void check_for_updates(update_result_cb cb, void* ud)
{
    switch(read_local_version())
    {
    case -1:
        cb(ERRMEM, STR_ERRMEM, ud);
        return;
    case -2:
        cb(-956, "Error getting current version", ud);
        return;
    default:
        break;
    }
    
    struct CB_UserData* cbud = malloc(sizeof(struct CB_UserData));
    if (!cbud)
    {
        cb(ERRMEM, STR_ERRMEM, ud);
        return;
    }
    
    memset(cbud, 0, sizeof(*cbud));
    cbud->cb = cb;
    cbud->ud = ud;
    
    enable_http(
        localVersionInfo->domain,
        "to check for a version update",
        CB_Permission,
        cbud
    );
}

#define UPDATE_CHECK_TIMESTAMP_PATH "check_update_timestamp.bin"

typedef uint32_t timestamp_t;

void write_update_timestamp(timestamp_t time)
{
    SDFile* f = playdate->file->open(UPDATE_CHECK_TIMESTAMP_PATH, kFileWrite);
    
    playdate->file->write(f, &time, sizeof(time));
    
    playdate->file->close(f);
}

#define DAYLEN (60*60*24)
#define TIME_BEFORE_CHECK_FIRST_UPDATE (DAYLEN * 4)
#define TIME_BETWEEN_SUBSEQUENT_UPDATE_CHECKS (DAYLEN)

void possibly_check_for_updates(update_result_cb cb, void* ud)
{
    timestamp_t now = playdate->system->getSecondsSinceEpoch(NULL);
    
    SDFile* f = playdate->file->open(UPDATE_CHECK_TIMESTAMP_PATH, kFileReadData);
    
    if (!f)
    {
        write_update_timestamp(now + TIME_BEFORE_CHECK_FIRST_UPDATE);
        cb(-5303, "no update timestamp -- first-time start", ud);
    }
    else
    {
        timestamp_t timestamp;
        
        int read = playdate->file->read(f, &timestamp, sizeof(timestamp));
        playdate->file->close(f);
        if (read != sizeof(timestamp) || timestamp < (365 * DAYLEN * 20))
        {
            write_update_timestamp(now + TIME_BETWEEN_SUBSEQUENT_UPDATE_CHECKS/2);
            cb(-5304, "failed to read timestamp -- replaced", ud);
        }
        else if (now >= timestamp)
        {
            write_update_timestamp(now + TIME_BETWEEN_SUBSEQUENT_UPDATE_CHECKS);
            
            // ready to update!
            check_for_updates(cb, ud);
        }
        else
        {
            cb(-5305, "it's not yet time to check for an update", ud);
        }
    }
}