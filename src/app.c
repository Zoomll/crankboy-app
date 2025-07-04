//
//  app.c
//  CrankBoy
//
//  Created by Matteo D'Ignazio on 14/05/22.
//  Maintained and developed by the CrankBoy dev team.
//

#include "app.h"

#include "../minigb_apu/minigb_apu.h"
#include "credits_scene.h"
#include "dtcm.h"
#include "game_scene.h"
#include "image_conversion_scene.h"
#include "jparse.h"
#include "library_scene.h"
#include "preferences.h"
#include "userstack.h"

// files that have been copied from PDX to data folder
#define COPIED_FILES "manifest.json"

PGB_Application* PGB_App;

typedef struct
{
    char* short_name;
    char* detailed_name;
} FetchedNames;

static void PGB_precacheGameNames(void);

#if defined(TARGET_SIMULATOR)
pthread_mutex_t audio_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static void checkForPngCallback(const char* filename, void* userdata)
{
    if (filename_has_stbi_extension(filename))
    {
        *(bool*)userdata = true;
    }
}

struct copy_file_callback_ud
{
    json_value* manifest;
    const char* directory;
    bool* modified;
};

static void copy_file_callback(const char* filename, void* userdata)
{
    struct copy_file_callback_ud* ud = userdata;
    json_value* manifest = ud->manifest;
    const char* directory = ud->directory;

    const char* extension = strrchr((char*)filename, '.');
    if (!extension)
        return;

    char* full_path = aprintf("%s/%s", directory, filename);
    if (!full_path)
        return;

    json_value already_copied = json_get_table_value(*manifest, full_path);

    char* dst_path = NULL;
    if (!strcasecmp(extension, ".png") || !strcasecmp(extension, ".jpg") ||
        !strcasecmp(extension, ".jpeg") || !strcasecmp(extension, ".bmp") ||
        !strcasecmp(extension, ".pdi"))
    {
        dst_path = aprintf("%s/%s", PGB_coversPath, filename);
    }
    else if (!strcasecmp(extension, ".gb") || !strcasecmp(extension, ".gbc"))
    {
        dst_path = aprintf("%s/%s", PGB_gamesPath, filename);
    }
    else if (!strcasecmp(extension, ".sav"))
    {
        dst_path = aprintf("%s/%s", PGB_savesPath, filename);
    }
    else if (!strcasecmp(extension, ".state"))
    {
        dst_path = aprintf("%s/%s", PGB_statesPath, filename);
    }

    if (!dst_path)
    {
        free(full_path);
        return;
    }

    if (already_copied.type != kJSONTrue)
    {
        printf("Extracting \"%s\" from PDX...\n", full_path);

        size_t size;
        void* dat = pgb_read_entire_file(full_path, &size, kFileRead);

        if (dat && size > 0)
        {
            bool success = pgb_write_entire_file(dst_path, dat, size);
            free(dat);

            // mark file as transferred
            if (success)
            {
                json_value _true;
                _true.type = kJSONTrue;
                json_set_table_value(manifest, full_path, _true);
                *ud->modified = true;
            }
        }
    }

    free(full_path);
}

void PGB_init(void)
{
    PGB_App = pgb_calloc(1, sizeof(PGB_Application));

    PGB_App->gameNameCache = array_new();
    PGB_App->scene = NULL;

    PGB_App->pendingScene = NULL;

    PGB_App->coverArtCache.rom_path = NULL;
    PGB_App->coverArtCache.art.bitmap = NULL;

    playdate->file->mkdir(PGB_gamesPath);
    playdate->file->mkdir(PGB_coversPath);
    playdate->file->mkdir(PGB_savesPath);
    playdate->file->mkdir(PGB_statesPath);
    playdate->file->mkdir(PGB_settingsPath);

    PGB_App->bodyFont = playdate->graphics->loadFont("fonts/Roobert-11-Medium", NULL);
    PGB_App->titleFont = playdate->graphics->loadFont("fonts/Roobert-20-Medium", NULL);
    PGB_App->subheadFont = playdate->graphics->loadFont("fonts/Asheville-Sans-14-Bold", NULL);
    PGB_App->labelFont = playdate->graphics->loadFont("fonts/Nontendo-Bold", NULL);

    pgb_draw_logo_with_message("Initializing…");
    preferences_init();

    PGB_App->clickSynth = playdate->sound->synth->newSynth();
    playdate->sound->synth->setWaveform(PGB_App->clickSynth, kWaveformSquare);
    playdate->sound->synth->setAttackTime(PGB_App->clickSynth, 0.0001f);
    playdate->sound->synth->setDecayTime(PGB_App->clickSynth, 0.05f);
    playdate->sound->synth->setSustainLevel(PGB_App->clickSynth, 0.0f);
    playdate->sound->synth->setReleaseTime(PGB_App->clickSynth, 0.0f);

    PGB_App->selectorBitmapTable =
        playdate->graphics->loadBitmapTable("images/selector/selector", NULL);
    PGB_App->startSelectBitmap =
        playdate->graphics->loadBitmap("images/selector-start-select", NULL);

    // --- Boot ROM data ---
    const char* bootRomPath = "dmg_boot.bin";
    SDFile* file = playdate->file->open(bootRomPath, kFileRead);
    if (file)
    {
        PGB_App->bootRomData = pgb_malloc(256);
        int bytesRead = playdate->file->read(file, PGB_App->bootRomData, 256);
        playdate->file->close(file);
        if (bytesRead != 256)
        {
            playdate->system->logToConsole(
                "Error: Read %d bytes from dmg_boot.bin, expected 256.", bytesRead
            );
            pgb_free(PGB_App->bootRomData);
            PGB_App->bootRomData = NULL;
        }
        else
        {
            playdate->system->logToConsole("Successfully loaded dmg_boot.bin");
        }
    }
    else
    {
        playdate->system->logToConsole(
            "Warning: Could not find %s. Skipping Boot ROM.", bootRomPath
        );
    }

    // add audio callback later
    PGB_App->soundSource = NULL;

    // custom frame rate delimiter
    playdate->display->setRefreshRate(0);

    // copy in files if not already copied in
    json_value manifest;
    parse_json(COPIED_FILES, &manifest, kFileReadData | kFileRead);

    if (manifest.type != kJSONTable)
    {
        manifest.type = kJSONTable;
        JsonObject* obj = malloc(sizeof(JsonObject));
        obj->n = 0;
        manifest.data.tableval = obj;
    }

    const char* sources[] = {".", PGB_coversPath, PGB_gamesPath, PGB_savesPath, PGB_statesPath};
    bool modified = false;

    for (size_t i = 0; i < sizeof(sources) / sizeof(const char*); ++i)
    {
        struct copy_file_callback_ud ud;
        ud.manifest = &manifest;
        ud.directory = sources[i];
        ud.modified = &modified;
        pgb_listfiles(sources[i], copy_file_callback, &ud, true, kFileRead);
    }

    // TODO: save manifest
    write_json_to_disk(COPIED_FILES, manifest);

    free_json_data(manifest);

    PGB_precacheGameNames();

    // check if any PNGs are in the covers/ folder
    bool png_found = false;
    pgb_listfiles(PGB_coversPath, checkForPngCallback, &png_found, true, kFileReadData);

#if 1
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
#else
    // test credits
    PGB_CreditsScene* credits = PGB_CreditsScene_new();
    PGB_present(credits->scene);
#endif
}

static FetchedNames get_titles_from_db(const char* fullpath)
{
    FetchedNames names = {NULL, NULL};

    uint32_t crc = pgb_calculate_crc32(fullpath);
    if (crc == 0)
    {
        return names;
    }

    char crc_string_upper[9];
    char crc_string_lower[9];

    snprintf(crc_string_upper, sizeof(crc_string_upper), "%08lX", crc);
    snprintf(crc_string_lower, sizeof(crc_string_lower), "%08lx", crc);

    char db_filename[32];
    snprintf(db_filename, sizeof(db_filename), "roms/%.2s.json", crc_string_lower);

    char* json_string = pgb_read_entire_file(db_filename, NULL, kFileRead | kFileReadData);
    if (!json_string)
    {
        return names;
    }

    json_value db_json;
    if (!parse_json_string(json_string, &db_json))
    {
        pgb_free(json_string);
        return names;
    }
    pgb_free(json_string);

    if (db_json.type == kJSONTable)
    {
        json_value game_entry = json_get_table_value(db_json, crc_string_upper);
        if (game_entry.type == kJSONTable)
        {
            json_value short_val = json_get_table_value(game_entry, "short");
            if (short_val.type == kJSONString && short_val.data.stringval)
            {
                names.short_name = string_copy(short_val.data.stringval);
            }

            json_value long_val = json_get_table_value(game_entry, "long");
            if (long_val.type == kJSONString && long_val.data.stringval)
            {
                names.detailed_name = string_copy(long_val.data.stringval);
            }
        }
    }

    free_json_data(db_json);
    return names;
}

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

static void PGB_precacheGameNames(void)
{
    playdate->system->logToConsole("Precaching game names...");

    PGB_Array* game_filenames = array_new();
    pgb_listfiles(PGB_gamesPath, collect_game_filenames_callback, game_filenames, 0, kFileReadData);

    for (int i = 0; i < game_filenames->length; i++)
    {
        const char* filename = game_filenames->items[i];

        char progress_message[100];
        snprintf(
            progress_message, sizeof(progress_message), "Scanning Games… (%d/%d)", i + 1,
            game_filenames->length
        );
        pgb_draw_logo_with_message(progress_message);

        PGB_GameName* newName = pgb_malloc(sizeof(PGB_GameName));

        newName->filename = string_copy(filename);
        newName->name_filename = pgb_basename(filename, true);

        char* fullpath;
        playdate->system->formatString(&fullpath, "%s/%s", PGB_gamesPath, filename);

        FetchedNames fetched = get_titles_from_db(fullpath);

        pgb_free(fullpath);

        newName->name_short =
            fetched.short_name ? fetched.short_name : string_copy(newName->name_filename);
        newName->name_detailed =
            fetched.detailed_name ? fetched.detailed_name : string_copy(newName->name_filename);

        array_push(PGB_App->gameNameCache, newName);
    }

    for (int i = 0; i < game_filenames->length; i++)
    {
        pgb_free(game_filenames->items[i]);
    }
    array_free(game_filenames);

    playdate->system->logToConsole(
        "Finished precaching %d game names.", PGB_App->gameNameCache->length
    );
}

__section__(".rare") static void switchToPendingScene(void)
{
    PGB_Scene* scene = PGB_App->scene;

    PGB_App->scene = PGB_App->pendingScene;
    PGB_App->pendingScene = NULL;

    if (scene)
    {
        void* managedObject = scene->managedObject;
        scene->free(managedObject);
    }
}

__section__(".text.main") void PGB_update(float dt)
{
    PGB_App->dt = dt;
    PGB_App->avg_dt =
        (PGB_App->avg_dt * FPS_AVG_DECAY) + (1 - FPS_AVG_DECAY) * dt * PGB_App->avg_dt_mult;
    PGB_App->avg_dt_mult = 1.0f;

    PGB_App->crankChange = playdate->system->getCrankChange();

    playdate->system->getButtonState(
        &PGB_App->buttons_down, &PGB_App->buttons_pressed, &PGB_App->buttons_released
    );

    PGB_App->buttons_released &= ~PGB_App->buttons_suppress;
    PGB_App->buttons_suppress &= PGB_App->buttons_down;
    PGB_App->buttons_down &= ~PGB_App->buttons_suppress;

    if (PGB_App->scene)
    {
        void* managedObject = PGB_App->scene->managedObject;
        DTCM_VERIFY_DEBUG();
        if (PGB_App->scene->use_user_stack)
        {
            uint32_t udt = FLOAT_AS_UINT32(dt);
            call_with_user_stack_2(PGB_App->scene->update, managedObject, udt);
        }
        else
        {
            PGB_App->scene->update(managedObject, dt);
        }
        DTCM_VERIFY_DEBUG();
    }

    playdate->graphics->display();

    if (PGB_App->pendingScene)
    {
        DTCM_VERIFY();
        call_with_user_stack(switchToPendingScene);
        DTCM_VERIFY();
    }

#if PGB_DEBUG
    playdate->display->setRefreshRate(60);
#else

    float refreshRate = 30;

    if (PGB_App->scene)
    {
        refreshRate = PGB_App->scene->preferredRefreshRate;
    }

#if CAP_FRAME_RATE
    // cap frame rate
    if (refreshRate > 0)
    {
        float refreshInterval = 1.0f / refreshRate;
        while (playdate->system->getElapsedTime() < refreshInterval)
            ;
    }
#endif

#endif
    DTCM_VERIFY_DEBUG();
}

void PGB_present(PGB_Scene* scene)
{
    playdate->system->removeAllMenuItems();
    PGB_App->buttons_suppress |= PGB_App->buttons_down;
    PGB_App->buttons_down = 0;
    PGB_App->buttons_released = 0;
    PGB_App->buttons_pressed = 0;

    PGB_App->pendingScene = scene;
}

void PGB_presentModal(PGB_Scene* scene)
{
    playdate->system->removeAllMenuItems();
    PGB_App->buttons_suppress |= PGB_App->buttons_down;
    PGB_App->buttons_down = 0;
    PGB_App->buttons_released = 0;
    PGB_App->buttons_pressed = 0;

    scene->parentScene = PGB_App->scene;
    PGB_App->scene = scene;
    PGB_Scene_refreshMenu(PGB_App->scene);
}

void PGB_dismiss(PGB_Scene* sceneToDismiss)
{
    printf("Dismiss\n");
    PGB_ASSERT(sceneToDismiss == PGB_App->scene);
    PGB_Scene* parent = sceneToDismiss->parentScene;
    if (parent)
    {
        parent->forceFullRefresh = true;
        PGB_present(parent);
    }
}

void PGB_goToLibrary(void)
{
    pgb_draw_logo_with_message("Returning to Library…");

    PGB_LibraryScene* libraryScene = PGB_LibraryScene_new();
    PGB_present(libraryScene->scene);
}

__section__(".rare") void PGB_event(PDSystemEvent event, uint32_t arg)
{
    PGB_ASSERT(PGB_App);
    if (PGB_App->scene)
    {
        PGB_ASSERT(PGB_App->scene->event != NULL);
        PGB_App->scene->event(PGB_App->scene->managedObject, event, arg);

        if (event == kEventPause)
        {
            // This probably supersedes any need to call PGB_Scene_refreshMenu anywhere else
            PGB_Scene_refreshMenu(PGB_App->scene);
        }
    }
}

void PGB_quit(void)
{
    if (PGB_App->scene)
    {
        void* managedObject = PGB_App->scene->managedObject;
        PGB_App->scene->free(managedObject);
    }

    pgb_clear_global_cover_cache();

    if (PGB_App->clickSynth)
    {
        playdate->sound->synth->freeSynth(PGB_App->clickSynth);
        PGB_App->clickSynth = NULL;
    }

    if (PGB_App->gameNameCache)
    {
        for (int i = 0; i < PGB_App->gameNameCache->length; i++)
        {
            PGB_GameName* gameName = PGB_App->gameNameCache->items[i];
            pgb_free(gameName->filename);
            pgb_free(gameName->name_short);
            pgb_free(gameName->name_detailed);
            pgb_free(gameName->name_filename);
            pgb_free(gameName);
        }
        array_free(PGB_App->gameNameCache);
    }

    pgb_free(PGB_App);
}
