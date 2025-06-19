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

static const char* pref_filename = "preferences.json";

int preferences_sound_mode = 2;
int preferences_crank_mode = 0;
int preferences_display_fps = 0;
int preferences_frame_skip = true;
int preferences_itcm = false;
int preferences_lua_support = false;
int preferences_dynamic_rate = 0;
int preferences_sample_rate = 1;
int preferences_uncap_fps = 0;
int preferences_save_state_slot = 0;
int preferences_overclock = 1;
int preferences_dynamic_level = 6;

static void cpu_endian_to_big_endian(
    unsigned char* src, unsigned char* buffer, size_t size, size_t len
);

static uint8_t preferences_read_uint8(SDFile* file);
static void preferences_write_uint8(SDFile* file, uint8_t value);
static uint32_t preferences_read_uint32(SDFile* file);
static void preferences_write_uint32(SDFile* file, uint32_t value);

void preferences_init(void)
{
    // default values which depend on device hardware (not available statically)
    preferences_itcm = (pd_rev == PD_REV_A);
    preferences_sample_rate = (pd_rev == PD_REV_A) ? 1 : 0;

    if (playdate->file->stat(pref_filename, NULL) != 0)
    {
        preferences_save_to_disk();
    }
    else
    {
        preferences_read_from_disk();
    }
}

void preferences_read_from_disk(void)
{
    json_value j;
    int success = parse_json(pref_filename, &j, kFileReadData);

    if (!success)
    {
        playdate->system->logToConsole("Failed to load preferences");
        return;
    }

#define KEY(x) if (!strcmp(obj->data[i].key, x))

    if (j.type == kJSONTable)
    {
        JsonObject* obj = j.data.tableval;
        for (size_t i = 0; i < obj->n; ++i)
        {
            json_value pref = obj->data[i].value;
            KEY("sound")
            {
                preferences_sound_mode = pref.data.intval;
            }
            KEY("crank")
            {
                preferences_crank_mode = pref.data.intval;
            }
            KEY("fps")
            {
                preferences_display_fps = pref.data.intval;
            }
            KEY("frameskip")
            {
                preferences_frame_skip = pref.data.intval;
            }
            KEY("itcm")
            {
                preferences_itcm = pref.data.intval;
            }
            KEY("lua")
            {
                preferences_lua_support = pref.data.intval;
            }
            KEY("dynamic_rate")
            {
                preferences_dynamic_rate = pref.data.intval;
            }
            KEY("sample_rate")
            {
                preferences_sample_rate = pref.data.intval;
            }
            KEY("uncap_fps")
            {
                preferences_uncap_fps = pref.data.intval;
            }
            KEY("dither")
            {
                preferences_dither_pattern = pref.data.intval;
            }
            KEY("save-state-slot")
            {
                preferences_save_state_slot = pref.data.intval;
            }
            KEY("overclock")
            {
                preferences_overclock = pref.data.intval;
            }
            KEY("dynamic_level")
            {
                preferences_dynamic_level = pref.data.intval;
            }
        }
    }

#undef KEY

    free_json_data(j);
}

int preferences_save_to_disk(void)
{
    playdate->system->logToConsole("Save preferences...");

// number of prefs to save
#define NUM_PREFS 13

    union
    {
        JsonObject obj;
        volatile char _[sizeof(JsonObject) + sizeof(TableKeyPair) * NUM_PREFS];
    } data;
    json_value j;
    j.type = kJSONTable;
    j.data.tableval = &data.obj;
    data.obj.n = NUM_PREFS;

    data.obj.data[0].key = "sound";
    data.obj.data[0].value.type = kJSONInteger;
    data.obj.data[0].value.data.intval = preferences_sound_mode;

    data.obj.data[1].key = "crank";
    data.obj.data[1].value.type = kJSONInteger;
    data.obj.data[1].value.data.intval = preferences_crank_mode;

    data.obj.data[2].key = "fps";
    data.obj.data[2].value.type = kJSONInteger;
    data.obj.data[2].value.data.intval = preferences_display_fps;

    data.obj.data[3].key = "frameskip";
    data.obj.data[3].value.type = kJSONInteger;
    data.obj.data[3].value.data.intval = preferences_frame_skip;

    data.obj.data[4].key = "itcm";
    data.obj.data[4].value.type = kJSONInteger;
    data.obj.data[4].value.data.intval = preferences_itcm;

    data.obj.data[5].key = "lua";
    data.obj.data[5].value.type = kJSONInteger;
    data.obj.data[5].value.data.intval = preferences_lua_support;

    data.obj.data[6].key = "dynamic_rate";
    data.obj.data[6].value.type = kJSONInteger;
    data.obj.data[6].value.data.intval = preferences_dynamic_rate;

    data.obj.data[7].key = "sample_rate";
    data.obj.data[7].value.type = kJSONInteger;
    data.obj.data[7].value.data.intval = preferences_sample_rate;

    data.obj.data[8].key = "uncap_fps";
    data.obj.data[8].value.type = kJSONInteger;
    data.obj.data[8].value.data.intval = preferences_uncap_fps;

    data.obj.data[9].key = "dither";
    data.obj.data[9].value.type = kJSONInteger;
    data.obj.data[9].value.data.intval = preferences_dither_pattern;

    data.obj.data[10].key = "save-state-slot";
    data.obj.data[10].value.type = kJSONInteger;
    data.obj.data[10].value.data.intval = preferences_save_state_slot;

    data.obj.data[11].key = "overclock";
    data.obj.data[11].value.type = kJSONInteger;
    data.obj.data[11].value.data.intval = preferences_overclock;

    data.obj.data[12].key = "dynamic_level";
    data.obj.data[12].value.type = kJSONInteger;
    data.obj.data[12].value.data.intval = preferences_dynamic_level;

    int error = write_json_to_disk(pref_filename, j);

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
