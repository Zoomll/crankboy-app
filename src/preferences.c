//
//  preferences.c
//  CrankBoy
//
//  Created by Matteo D'Ignazio on 18/05/22.
//  Maintained and developed by the CrankBoy dev team.
//

#include "preferences.h"

#include "jparse.h"
#include "revcheck.h"

static const int pref_version = 1;

#define PREF(x, ...) preference_t preferences_##x;
#include "prefs.x"

#define PREF(x, ...) 1 +
const int pref_count =
#include "prefs.x"
    0;

static void cpu_endian_to_big_endian(
    unsigned char* src, unsigned char* buffer, size_t size, size_t len
);

static uint8_t preferences_read_uint8(SDFile* file);
static void preferences_write_uint8(SDFile* file, uint8_t value);
static uint32_t preferences_read_uint32(SDFile* file);
static void preferences_write_uint32(SDFile* file, uint32_t value);

static void preferences_set_defaults(void)
{
#define PREF(x, d) preferences_##x = d;
#include "prefs.x"
}

void preferences_init(void)
{
    // if this fails, increase bitfield to uint64_t
    PGB_ASSERT(pref_count <= 8 * sizeof(preferences_bitfield_t));

    preferences_set_defaults();

    if (playdate->file->stat(PGB_globalPrefsPath, NULL) != 0)
    {
        preferences_save_to_disk(PGB_globalPrefsPath, 0);
    }
    else
    {
        preferences_read_from_disk(PGB_globalPrefsPath);
    }

    // paranoia
    preferences_per_game = 0;
}

void preferences_read_from_disk(const char* filename)
{
    preferences_set_defaults();

    json_value j;
    int success = parse_json(filename, &j, kFileReadData);

    if (!success)
    {
        playdate->system->logToConsole("Failed to load preferences from %s", filename);
        return;
    }

#define KEY(x) if (!strcmp(obj->data[i].key, x))

    if (j.type == kJSONTable)
    {
        JsonObject* obj = j.data.tableval;
        for (size_t i = 0; i < obj->n; ++i)
        {
            json_value pref = obj->data[i].value;

#define PREF(x, ...)                                      \
    if (!strcmp(obj->data[i].key, #x))                    \
    {                                                     \
        preferences_##x = obj->data[i].value.data.intval; \
    };
#include "prefs.x"
        }
    }

#undef KEY

    free_json_data(j);
}

int preferences_save_to_disk(const char* filename, preferences_bitfield_t leave_as_is)
{
    playdate->system->logToConsole("Save preferences to %s...", filename);

    void* preserved_all = preferences_store_subset(-1);
    void* preserved_to_write = preferences_store_subset(~leave_as_is);

    // temporarily load the fields which are to be left as is
    if (leave_as_is != 0 && preserved_to_write)
    {
        preferences_read_from_disk(filename);
        preferences_restore_subset(preserved_to_write);
    }

    if (preserved_to_write)
        free(preserved_to_write);

    union
    {
        JsonObject obj;
        volatile char _[sizeof(JsonObject) + sizeof(TableKeyPair) * pref_count];
    } data;
    json_value j;
    j.type = kJSONTable;
    j.data.tableval = &data.obj;
    data.obj.n = pref_count;

    int i = 0;
#define PREF(x, ...)                                      \
    data.obj.data[i].key = #x;                            \
    data.obj.data[i].value.type = kJSONInteger;           \
    data.obj.data[i].value.data.intval = preferences_##x; \
    ++i;
#include "prefs.x"

    // restore caller's preferences
    if (preserved_all)
    {
        preferences_restore_subset(preserved_all);
        free(preserved_all);
    }

    int error = write_json_to_disk(filename, j);

    playdate->system->logToConsole("Save preferences status code %d", error);

    return !error;
}

static uint8_t preferences_read_uint8(SDFile* file)
{
    uint8_t buffer[1];
    playdate->file->read(file, buffer, sizeof(uint8_t));
    return buffer[0];
}

static void preferences_write_uint8(SDFile* file, uint8_t value)
{
    playdate->file->write(file, &value, sizeof(uint8_t));
}

static uint32_t preferences_read_uint32(SDFile* file)
{
    unsigned char buffer[sizeof(uint32_t)];
    playdate->file->read(file, buffer, sizeof(uint32_t));
    return buffer[0] << 24 | buffer[1] << 16 | buffer[2] << 8 | buffer[3];
}

static void preferences_write_uint32(SDFile* file, uint32_t value)
{
    unsigned char buffer[sizeof(uint32_t)];
    cpu_endian_to_big_endian((unsigned char*)&value, buffer, sizeof(uint32_t), 1);
    playdate->file->write(file, buffer, sizeof(uint32_t));
}

static void cpu_endian_to_big_endian(
    unsigned char* src, unsigned char* buffer, size_t size, size_t len
)
{
    int x = 1;

    if (*((char*)&x) == 1)
    {
        // little endian machine, swap
        for (size_t i = 0; i < len; i++)
        {
            for (size_t ix = 0; ix < size; ix++)
            {
                buffer[size * i + ix] = src[size * i + (size - 1 - ix)];
            }
        }
    }
    else
    {
        memcpy(buffer, src, size * len);
    }
}

void* preferences_store_subset(preferences_bitfield_t subset)
{
    int count = 0;
    int i = 0;
#define PREF(x, ...)       \
    if (subset & (1 << i)) \
    {                      \
        count++;           \
    }                      \
    ++i;
#include "prefs.x"

    void* data = malloc(sizeof(preferences_bitfield_t) + sizeof(preference_t) * count);
    if (!data)
        return NULL;

    preferences_bitfield_t* dbits = data;
    *dbits = subset;
    preference_t* prefs = data + sizeof(preferences_bitfield_t);

    count = 0;
    i = 0;
#define PREF(x, ...)                      \
    if (subset & (1 << i))                \
    {                                     \
        prefs[count++] = preferences_##x; \
    }                                     \
    ++i;
#include "prefs.x"

    return data;
}

void preferences_restore_subset(void* data)
{
    preferences_bitfield_t subset = *(preferences_bitfield_t*)data;
    preference_t* prefs = data + sizeof(preferences_bitfield_t);

    int count = 0;
    int i = 0;
#define PREF(x, ...)                      \
    if (subset & (1 << i))                \
    {                                     \
        preferences_##x = prefs[count++]; \
    }                                     \
    ++i;
#include "prefs.x"
}