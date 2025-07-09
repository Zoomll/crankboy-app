// game_scanning_scene.c
#include "game_scanning_scene.h"

#include "app.h"
#include "cover_cache_scene.h"
#include "image_conversion_scene.h"
#include "library_scene.h"
#include "pd_api.h"

void PGB_GameScanningScene_update(void* object, uint32_t u32enc_dt);
void PGB_GameScanningScene_free(void* object);

static void collect_game_filenames_callback(const char* filename, void* userdata)
{
    PGB_Array* filenames_array = userdata;
    char* extension;
    char* dot = pgb_strrchr(filename, '.');

    if (!dot || dot == filename)
    {
        extension = "";
    }
    else
    {
        extension = dot + 1;
    }

    if ((pgb_strcmp(extension, "gb") == 0 || pgb_strcmp(extension, "gbc") == 0))
    {
        array_push(filenames_array, string_copy(filename));
    }
}

static void process_one_game(PGB_GameScanningScene* scanScene, const char* filename)
{
    PGB_GameName* newName = pgb_malloc(sizeof(PGB_GameName));
    memset(newName, 0, sizeof(PGB_GameName));

    newName->filename = string_copy(filename);
    newName->name_filename = pgb_basename(filename, true);
    newName->name_filename_leading_article = common_article_form(newName->name_filename);

    char* fullpath;
    playdate->system->formatString(&fullpath, "%s/%s", PGB_gamesPath, filename);

    FileStat stat;
    if (playdate->file->stat(fullpath, &stat) != 0)
    {
        playdate->system->logToConsole("Failed to stat file: %s", fullpath);
        pgb_free(fullpath);
        free_game_names(newName);
        pgb_free(newName);
        return;
    }

    struct PDDateTime dt = {
        .year = stat.m_year,
        .month = stat.m_month,
        .day = stat.m_day,
        .hour = stat.m_hour,
        .minute = stat.m_minute,
        .second = stat.m_second
    };
    uint32_t m_time_epoch = playdate->system->convertDateTimeToEpoch(&dt);

    uint32_t crc = 0;
    bool needs_calculation = true;

    json_value cached_entry = {.type = kJSONNull};
    if (scanScene->crc_cache.type == kJSONTable)
    {
        JsonObject* obj = scanScene->crc_cache.data.tableval;

        TableKeyPair key_to_find;
        key_to_find.key = (char*)filename;

        TableKeyPair* found_pair = (TableKeyPair*)bsearch(
            &key_to_find, obj->data, obj->n, sizeof(TableKeyPair), compare_key_pairs
        );

        if (found_pair)
        {
            cached_entry = found_pair->value;
        }
    }

    if (cached_entry.type == kJSONTable)
    {
        json_value cached_crc_val = json_get_table_value(cached_entry, "crc32");
        json_value cached_size_val = json_get_table_value(cached_entry, "size");
        json_value cached_mtime_val = json_get_table_value(cached_entry, "m_time");

        if (cached_crc_val.type == kJSONInteger && cached_size_val.type == kJSONInteger &&
            cached_mtime_val.type == kJSONInteger)
        {
            if ((uint32_t)cached_size_val.data.intval == stat.size &&
                (uint32_t)cached_mtime_val.data.intval == m_time_epoch)
            {
                crc = (uint32_t)cached_crc_val.data.intval;
                needs_calculation = false;
            }
        }
    }

    PGB_FetchedNames fetched = {NULL, NULL, 0, true};

    if (needs_calculation)
    {
        if (pgb_calculate_crc32(fullpath, kFileReadDataOrBundle, &crc))
        {
            fetched.failedToOpenROM = false;

            json_value new_entry_val;
            new_entry_val.type = kJSONTable;
            JsonObject* obj = pgb_calloc(1, sizeof(JsonObject));
            new_entry_val.data.tableval = obj;

            json_value crc_val = {.type = kJSONInteger, .data.intval = crc};
            json_value size_val = {.type = kJSONInteger, .data.intval = stat.size};
            json_value mtime_val = {.type = kJSONInteger, .data.intval = m_time_epoch};

            json_set_table_value(&new_entry_val, "crc32", crc_val);
            json_set_table_value(&new_entry_val, "size", size_val);
            json_set_table_value(&new_entry_val, "m_time", mtime_val);

            json_set_table_value(&scanScene->crc_cache, filename, new_entry_val);
            scanScene->crc_cache_modified = true;
        }
    }
    else
    {
        fetched.failedToOpenROM = false;
    }

    if (!fetched.failedToOpenROM)
    {
        PGB_FetchedNames names_from_db = pgb_get_titles_from_db_by_crc(crc);
        fetched.short_name = names_from_db.short_name;
        fetched.detailed_name = names_from_db.detailed_name;
    }

    fetched.crc32 = crc;
    newName->crc32 = fetched.crc32;
    pgb_free(fullpath);

    newName->name_database = (fetched.detailed_name) ? string_copy(fetched.detailed_name) : NULL;
    newName->name_short = (fetched.short_name) ? string_copy(fetched.short_name)
                                               : string_copy(newName->name_filename);
    newName->name_detailed = (fetched.detailed_name) ? string_copy(fetched.detailed_name)
                                                     : string_copy(newName->name_filename);

    newName->name_short_leading_article = common_article_form(newName->name_short);
    newName->name_detailed_leading_article = common_article_form(newName->name_detailed);

    if (fetched.short_name)
        pgb_free(fetched.short_name);
    if (fetched.detailed_name)
        pgb_free(fetched.detailed_name);

    if (!fetched.failedToOpenROM)
    {
        array_push(PGB_App->gameNameCache, newName);
    }
    else
    {
        free_game_names(newName);
        pgb_free(newName);
    }
}

static void checkForPngCallback(const char* filename, void* userdata)
{
    if (filename_has_stbi_extension(filename))
    {
        *(bool*)userdata = true;
    }
}

void PGB_GameScanningScene_update(void* object, uint32_t u32enc_dt)
{
    if (PGB_App->pendingScene)
    {
        return;
    }

    PGB_GameScanningScene* scanScene = object;

    switch (scanScene->state)
    {
    case kScanningStateInit:
    {
        pgb_draw_logo_screen_to_buffer("Finding Games…");

        playdate->file->listfiles(
            PGB_gamesPath, collect_game_filenames_callback, scanScene->game_filenames, 0
        );

        array_reserve(PGB_App->gameNameCache, scanScene->game_filenames->length);

        if (scanScene->game_filenames->length == 0)
        {
            scanScene->state = kScanningStateDone;
        }
        else
        {
            scanScene->state = kScanningStateScanning;
        }
        break;
    }

    case kScanningStateScanning:
    {
        if (scanScene->current_index < scanScene->game_filenames->length)
        {
            const char* filename = scanScene->game_filenames->items[scanScene->current_index];

            char progress_message[100];
            snprintf(
                progress_message, sizeof(progress_message), "Scanning Games… (%d/%d)",
                scanScene->current_index + 1, scanScene->game_filenames->length
            );
            pgb_draw_logo_screen_to_buffer(progress_message);

            process_one_game(scanScene, filename);

            scanScene->current_index++;
        }
        else
        {
            scanScene->state = kScanningStateDone;
        }
        break;
    }

    case kScanningStateDone:
    {
        if (scanScene->crc_cache_modified)
        {
            char* path;
            playdate->system->formatString(&path, "%s", CRC_CACHE_FILE);
            if (path)
            {
                write_json_to_disk(path, scanScene->crc_cache);
                pgb_free(path);
            }
        }

        bool png_found = false;
        playdate->file->listfiles(PGB_coversPath, checkForPngCallback, &png_found, false);

        if (png_found)
        {
            PGB_ImageConversionScene* imageConversionScene = PGB_ImageConversionScene_new();
            PGB_present(imageConversionScene->scene);
        }
        else
        {
            PGB_CoverCacheScene* cacheScene = PGB_CoverCacheScene_new();
            PGB_present(cacheScene->scene);
        }
        break;
    }
    }
}

void PGB_GameScanningScene_free(void* object)
{
    PGB_GameScanningScene* scanScene = object;

    if (scanScene->game_filenames)
    {
        for (int i = 0; i < scanScene->game_filenames->length; i++)
        {
            pgb_free(scanScene->game_filenames->items[i]);
        }
        array_free(scanScene->game_filenames);
    }

    free_json_data(scanScene->crc_cache);
    pgb_free(scanScene);
}

PGB_GameScanningScene* PGB_GameScanningScene_new(void)
{
    PGB_GameScanningScene* scanScene = pgb_calloc(1, sizeof(PGB_GameScanningScene));

    scanScene->scene = PGB_Scene_new();
    scanScene->scene->managedObject = scanScene;
    scanScene->scene->update = PGB_GameScanningScene_update;
    scanScene->scene->free = PGB_GameScanningScene_free;
    scanScene->scene->use_user_stack = false;

    scanScene->game_filenames = array_new();
    scanScene->current_index = 0;
    scanScene->state = kScanningStateInit;
    scanScene->crc_cache_modified = false;

    char* path;
    playdate->system->formatString(&path, "%s", CRC_CACHE_FILE);
    if (path)
    {
        if (parse_json(path, &scanScene->crc_cache, kFileReadData))
        {
            if (scanScene->crc_cache.type == kJSONTable)
            {
                JsonObject* obj = scanScene->crc_cache.data.tableval;
                if (obj && obj->n > 1)
                {
                    qsort(obj->data, obj->n, sizeof(TableKeyPair), compare_key_pairs);
                }
            }
        }
        else
        {
            scanScene->crc_cache.type = kJSONTable;
            JsonObject* obj = pgb_malloc(sizeof(JsonObject));
            obj->n = 0;
            scanScene->crc_cache.data.tableval = obj;
        }
        pgb_free(path);
    }

    return scanScene;
}
