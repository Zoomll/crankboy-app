//
//  preferences.c
//  CrankBoy
//
//  Created by Matteo D'Ignazio on 18/05/22.
//  Maintained and developed by the CrankBoy dev team.
//

#include "preferences.h"

#include "revcheck.h"

static const int pref_version = 1;

static const char *pref_filename = "preferences.bin";

int preferences_sound_mode = 0;
int preferences_crank_mode = 0;
bool preferences_display_fps = false;
bool preferences_frame_skip = false;
bool preferences_itcm = false;

static void cpu_endian_to_big_endian(unsigned char *src, unsigned char *buffer,
                                     size_t size, size_t len);

static uint8_t preferences_read_uint8(SDFile *file);
static void preferences_write_uint8(SDFile *file, uint8_t value);
static uint32_t preferences_read_uint32(SDFile *file);
static void preferences_write_uint32(SDFile *file, uint32_t value);

void preferences_init(void)
{
    preferences_sound_mode = 2;
    preferences_crank_mode = 0;
    preferences_display_fps = false;
    preferences_frame_skip = false;
    preferences_itcm = (pd_rev == PD_REV_A);

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
    SDFile *file = playdate->file->open(pref_filename, kFileReadData);

    if (file)
    {
        uint32_t version = preferences_read_uint32(file);

        preferences_sound_mode = preferences_read_uint8(file);
        preferences_crank_mode = preferences_read_uint8(file);
        preferences_display_fps = preferences_read_uint8(file);
        preferences_frame_skip = preferences_read_uint8(file);
        preferences_itcm = preferences_read_uint8(file);

        playdate->file->close(file);
    }
    else
    {
        playdate->system->logToConsole(
            "Error: Could not open preferences file for reading.");
    }
}

void preferences_save_to_disk(void)
{
    playdate->system->logToConsole("Saving preferences.");
    SDFile *file = playdate->file->open(pref_filename, kFileWrite);

    if (file)
    {
        preferences_write_uint32(file, pref_version);

        preferences_write_uint8(file, preferences_sound_mode);
        preferences_write_uint8(file, preferences_crank_mode);
        preferences_write_uint8(file, preferences_display_fps ? 1 : 0);
        preferences_write_uint8(file, preferences_frame_skip ? 1 : 0);
        preferences_write_uint8(file, preferences_itcm ? 1 : 0);

        playdate->file->close(file);
    }
    else
    {
        playdate->system->logToConsole(
            "Error: Could not open preferences file for writing.");
    }
}

static uint8_t preferences_read_uint8(SDFile *file)
{
    uint8_t buffer[1];
    playdate->file->read(file, buffer, sizeof(uint8_t));
    return buffer[0];
}

static void preferences_write_uint8(SDFile *file, uint8_t value)
{
    playdate->file->write(file, &value, sizeof(uint8_t));
}

static uint32_t preferences_read_uint32(SDFile *file)
{
    unsigned char buffer[sizeof(uint32_t)];
    playdate->file->read(file, buffer, sizeof(uint32_t));
    return buffer[0] << 24 | buffer[1] << 16 | buffer[2] << 8 | buffer[3];
}

static void preferences_write_uint32(SDFile *file, uint32_t value)
{
    unsigned char buffer[sizeof(uint32_t)];
    cpu_endian_to_big_endian((unsigned char *)&value, buffer, sizeof(uint32_t),
                             1);
    playdate->file->write(file, buffer, sizeof(uint32_t));
}

static void cpu_endian_to_big_endian(unsigned char *src, unsigned char *buffer,
                                     size_t size, size_t len)
{
    int x = 1;

    if (*((char *)&x) == 1)
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
