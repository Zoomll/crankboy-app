// game_scanning_scene.c
#include "game_scanning_scene.h"

#include "app.h"
#include "image_conversion_scene.h"
#include "library_scene.h"

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
        char* fullpath = aprintf("%s/%s", PGB_gamesPath, filename);
        if (fullpath)
        {
            // unless we're in bundled mode (where we shouldn't be scanning regardless),
            // only add ROMs if they are present in the data directory.
            if (PGB_App->bundled_rom || pgb_file_exists(fullpath, kFileReadData))
            {
                array_push(filenames_array, string_copy(filename));
            }
            free(fullpath);
        }
    }
}

static void process_one_game(const char* filename)
{
    PGB_GameName* newName = pgb_malloc(sizeof(PGB_GameName));

    memset(newName, 0, sizeof(PGB_GameName));

    newName->filename = string_copy(filename);
    newName->name_filename = pgb_basename(filename, true);

    char* fullpath;
    playdate->system->formatString(&fullpath, "%s/%s", PGB_gamesPath, filename);

    PGB_FetchedNames fetched = pgb_get_titles_from_db(fullpath);
    newName->crc32 = fetched.crc32;

    pgb_free(fullpath);

    newName->name_original_long =
        (fetched.detailed_name) ? string_copy(fetched.detailed_name) : NULL;
    newName->name_short = (fetched.short_name) ? common_article_form(fetched.short_name)
                                               : common_article_form(newName->name_filename);
    newName->name_detailed = (fetched.detailed_name) ? common_article_form(fetched.detailed_name)
                                                     : common_article_form(newName->name_filename);

    if (fetched.short_name)
        pgb_free(fetched.short_name);
    if (fetched.detailed_name)
        pgb_free(fetched.detailed_name);

    // We omit un-openable ROMs, as they are likely not in the Data directory (PDX only) for some reason.
    // Perhaps the user wanted to remove a ROM after earlier installing it with the copy-from-PDX method.
    if (!fetched.failedToOpenROM)
    {
        array_push(PGB_App->gameNameCache, newName);
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
    PGB_GameScanningScene* scanScene = object;

    switch (scanScene->state)
    {
    case kScanningStateInit:
    {
        pgb_draw_logo_with_message("Finding Games…");

        playdate->file->listfiles(
            PGB_gamesPath, collect_game_filenames_callback, scanScene->game_filenames, 0
        );

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
            pgb_draw_logo_with_message(progress_message);

            process_one_game(filename);

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
        playdate->system->logToConsole(
            "Finished precaching %d game names.", PGB_App->gameNameCache->length
        );

        bool png_found = false;
        playdate->file->listfiles(PGB_coversPath, checkForPngCallback, &png_found, true);

        if (png_found)
        {
            PGB_ImageConversionScene* imageConversionScene = PGB_ImageConversionScene_new();
            PGB_present(imageConversionScene->scene);
        }
        else
        {
            pgb_draw_logo_with_message("Loading Library…");
            PGB_LibraryScene* libraryScene = PGB_LibraryScene_new();
            PGB_present(libraryScene->scene);
        }
        break;
    }
    }
}

void PGB_GameScanningScene_free(void* object)
{
    PGB_GameScanningScene* scanScene = object;

    playdate->system->logToConsole("Freeing GameScanningScene");

    if (scanScene->game_filenames)
    {
        for (int i = 0; i < scanScene->game_filenames->length; i++)
        {
            pgb_free(scanScene->game_filenames->items[i]);
        }
        array_free(scanScene->game_filenames);
    }

    free(scanScene);
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

    return scanScene;
}
