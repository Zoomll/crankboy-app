#include "version.h"

#include "app.h"
#include "http.h"
#include "jparse.h"
#include "pd_api.h"
#include "utility.h"

#define ERRMEM -255
#define STR_ERRMEM "malloc error"
#define UPDATE_CHECK_TIMESTAMP_PATH "check_update_timestamp.bin"
#define UPDATE_LAST_KNOWN_VERSION "check_update_version.txt"

struct CB_UserData
{
    update_result_cb cb;
    void* ud;
};

struct VersionInfo
{
    char* name;
    char* domain;
    char* path;
    char* download;
};
static struct VersionInfo* localVersionInfo = NULL;

// previously been alerted to this version update, so dismiss it
char* ignore_version = NULL;

// returns 0 on success, or
// returns nonzero failure code
static int read_version_info(const char* text, bool ispath, struct VersionInfo* oinfo)
{
    json_value jvinfo;

    if (oinfo->name)
        pgb_free(oinfo->name);
    if (oinfo->domain)
        pgb_free(oinfo->domain);
    if (oinfo->path)
        pgb_free(oinfo->path);

    int jparse_result = (ispath) ? parse_json(VERSION_INFO_FILE, &jvinfo, kFileRead | kFileReadData)
                                 : parse_json_string(text, &jvinfo);

    if (jparse_result == 0 || jvinfo.type != kJSONTable)
    {
        free_json_data(jvinfo);
        return -1;
    }

    json_value jname = json_get_table_value(jvinfo, "name");
    json_value jpath = json_get_table_value(jvinfo, "path");
    json_value jdomain = json_get_table_value(jvinfo, "domain");
    json_value jdownload = json_get_table_value(jvinfo, "download");

    if (jname.type != kJSONString || jpath.type != kJSONString || jdomain.type != kJSONString ||
        jdownload.type != kJSONString)
    {
        free_json_data(jvinfo);
        return -2;
    }

    oinfo->name = string_copy(jname.data.stringval);
    oinfo->path = string_copy(jpath.data.stringval);
    oinfo->domain = string_copy(jdomain.data.stringval);
    oinfo->download = string_copy(jdownload.data.stringval);

    free_json_data(jvinfo);

    return 0;
}

static int read_local_version(void)
{
    if (!localVersionInfo)
    {
        localVersionInfo = pgb_malloc(sizeof(struct VersionInfo));
        if (!localVersionInfo)
        {
            return -1;
        }
        memset(localVersionInfo, 0, sizeof(*localVersionInfo));

        int result;
        if ((result = read_version_info("version.json", true, localVersionInfo)))
        {
            pgb_free(localVersionInfo);
            localVersionInfo = NULL;
            return -2;
        }
    }

    if (!ignore_version)
    {
        ignore_version = pgb_read_entire_file(UPDATE_LAST_KNOWN_VERSION, NULL, kFileReadData);
    }

    return 1;
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

const char* get_download_url(void)
{
    if (read_local_version() == 1)
    {
        return localVersionInfo->download;
    }
    else
    {
        return NULL;
    }
}

static void CB_Get(unsigned flags, char* data, size_t data_len, void* ud)
{
    struct CB_UserData* cbud = ud;

    if ((flags & HTTP_ENABLE_DENIED))
    {
        cbud->cb(
            (flags & HTTP_ENABLE_ASKED) ? ERR_PERMISSION_ASKED_DENIED : ERR_PERMISSION_DENIED,
            "Permission denied", cbud->ud
        );
    }
    else if (flags & ~HTTP_ENABLE_ASKED)
    {
        cbud->cb(-9000 - flags, "Update failed", cbud->ud);
    }
    else
    {
        // try parsing json. We have to skip to the first `{` because
        // the playdate HTTP API seems to put some garbage data (the http status code?) at the
        // beginning.
        json_value jv;
        int result = parse_json_string(strchr(data, '{'), &jv);

        if (result != 0)
        {
            // otherwise, we're done! Validate result:
            json_value jname = json_get_table_value(jv, "name");

            if (jname.type == kJSONString && strlen(jname.data.stringval) > 0)
            {
                // if this matches the last-seen version, we mention that in the callback
                if (ignore_version && strcmp(jname.data.stringval, ignore_version) == 0)
                {
                    // new version available, but we already knew about it
                    cbud->cb(1, jname.data.stringval, cbud->ud);
                }
                else
                {
                    // update last-seen version
                    pgb_write_entire_file(
                        UPDATE_LAST_KNOWN_VERSION, jname.data.stringval,
                        strlen(jname.data.stringval)
                    );
                    if (ignore_version)
                        pgb_free(ignore_version);
                    ignore_version = string_copy(jname.data.stringval);

                    if (strcmp(jname.data.stringval, localVersionInfo->name))
                    {
                        // new version available
                        cbud->cb(2, jname.data.stringval, cbud->ud);
                    }
                    else
                    {
                        cbud->cb(0, "No update available.", cbud->ud);
                    }
                }
            }
            else
            {
                cbud->cb(-650, "Invalid version information receieved", cbud->ud);
            }
        }
        else
        {
            cbud->cb(-651, "Invalid JSON response", cbud->ud);
        }
        free_json_data(jv);
    }
    pgb_free(ud);
}

void check_for_updates(update_result_cb cb, void* ud)
{
    switch (read_local_version())
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

    struct CB_UserData* cbud = pgb_malloc(sizeof(struct CB_UserData));
    if (!cbud)
    {
        cb(ERRMEM, STR_ERRMEM, ud);
        return;
    }

    memset(cbud, 0, sizeof(*cbud));
    cbud->cb = cb;
    cbud->ud = ud;

#define TIMEOUT_MS (10 * 1000)

    http_get(
        localVersionInfo->domain, localVersionInfo->path, "to check for a version update", CB_Get,
        TIMEOUT_MS, cbud, NULL
    );
}

typedef uint32_t timestamp_t;

void write_update_timestamp(timestamp_t time)
{
    SDFile* f = playdate->file->open(UPDATE_CHECK_TIMESTAMP_PATH, kFileWrite);

    playdate->file->write(f, &time, sizeof(time));

    playdate->file->close(f);
}

#define DAYLEN (60 * 60 * 24)
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
            write_update_timestamp(now + TIME_BETWEEN_SUBSEQUENT_UPDATE_CHECKS / 2);
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

void version_quit(void)
{
    if (localVersionInfo)
    {
        pgb_free(localVersionInfo->name);
        pgb_free(localVersionInfo->domain);
        pgb_free(localVersionInfo->path);
        pgb_free(localVersionInfo);
        localVersionInfo = NULL;
    }
    if (ignore_version)
    {
        pgb_free(ignore_version);
        ignore_version = NULL;
    }
}
