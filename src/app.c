//
//  app.c
//  CrankBoy
//
//  Created by Matteo D'Ignazio on 14/05/22.
//  Maintained and developed by the CrankBoy dev team.
//

#include "app.h"

#include "../minigb_apu/minigb_apu.h"
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

    PGB_App->scene = NULL;

    PGB_App->pendingScene = NULL;

    PGB_App->coverArtCache.rom_path = NULL;
    PGB_App->coverArtCache.art.bitmap = NULL;

    playdate->file->mkdir(PGB_gamesPath);
    playdate->file->mkdir(PGB_coversPath);
    playdate->file->mkdir(PGB_savesPath);
    playdate->file->mkdir(PGB_statesPath);
    playdate->file->mkdir(PGB_settingsPath);

    preferences_init();

    PGB_App->bodyFont = playdate->graphics->loadFont("fonts/Roobert-11-Medium", NULL);
    PGB_App->titleFont = playdate->graphics->loadFont("fonts/Roobert-20-Medium", NULL);
    PGB_App->subheadFont = playdate->graphics->loadFont("fonts/Asheville-Sans-14-Bold", NULL);
    PGB_App->labelFont = playdate->graphics->loadFont("fonts/Nontendo-Bold", NULL);

    PGB_App->clickSynth = playdate->sound->synth->newSynth();
    playdate->sound->synth->setWaveform(PGB_App->clickSynth, kWaveformSquare);
    playdate->sound->synth->setAttackTime(PGB_App->clickSynth, 0.0f);
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

    const char* logoPath = "images/logo.pdi";
    LCDBitmap* logoBitmap = playdate->graphics->loadBitmap(logoPath, NULL);

    playdate->graphics->clear(kColorWhite);

    if (logoBitmap)
    {
        const char* init_msg = "Initializing...";

        int screenWidth = LCD_COLUMNS;
        int screenHeight = LCD_ROWS;
        LCDFont* font = PGB_App->bodyFont;

        int logoWidth, logoHeight;
        playdate->graphics->getBitmapData(logoBitmap, &logoWidth, &logoHeight, NULL, NULL, NULL);

        int textWidth =
            playdate->graphics->getTextWidth(font, init_msg, strlen(init_msg), kUTF8Encoding, 0);
        int textHeight = playdate->graphics->getFontHeight(font);

        int lineSpacing = textHeight;

        int totalBlockHeight = logoHeight + lineSpacing + textHeight;

        int blockY_start = (screenHeight - totalBlockHeight) / 2;

        int logoX = (screenWidth - logoWidth) / 2;
        int logoY = blockY_start;

        int textX = (screenWidth - textWidth) / 2;
        int textY = logoY + logoHeight + lineSpacing;

        playdate->graphics->drawBitmap(logoBitmap, logoX, logoY, kBitmapUnflipped);
        playdate->graphics->drawText(init_msg, strlen(init_msg), kUTF8Encoding, textX, textY);

        playdate->graphics->freeBitmap(logoBitmap);
    }
    else
    {
        const char* setup_msg = "Performing first-time setup...";
        int textWidth = playdate->graphics->getTextWidth(
            PGB_App->bodyFont, setup_msg, strlen(setup_msg), kUTF8Encoding, 0
        );
        playdate->graphics->drawText(
            setup_msg, strlen(setup_msg), kUTF8Encoding, LCD_COLUMNS / 2 - textWidth / 2,
            LCD_ROWS / 2
        );
    }

    playdate->graphics->display();

    const char* sources[] = {".", PGB_coversPath, PGB_gamesPath, PGB_savesPath, PGB_statesPath};
    bool modified = false;

    for (size_t i = 0; i < sizeof(sources) / sizeof(const char*); ++i)
    {
        struct copy_file_callback_ud ud;
        ud.manifest = &manifest;
        ud.directory = sources[i];
        ud.modified = &modified;
        playdate->file->listfiles(sources[i], copy_file_callback, &ud, true);
    }

    // TODO: save manifest
    write_json_to_disk(COPIED_FILES, manifest);

    free_json_data(manifest);

    // check if any PNGs are in the covers/ folder
    bool png_found = false;
    playdate->file->listfiles(PGB_coversPath, checkForPngCallback, &png_found, true);

    if (png_found)
    {
        PGB_ImageConversionScene* imageConversionScene = PGB_ImageConversionScene_new();
        PGB_present(imageConversionScene->scene);
    }
    else
    {
        PGB_LibraryScene* libraryScene = PGB_LibraryScene_new();
        PGB_present(libraryScene->scene);
    }
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

    playdate->system->getButtonState(&PGB_App->buttons_down, &PGB_App->buttons_pressed, NULL);

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
    PGB_App->buttons_pressed = 0;

    PGB_App->pendingScene = scene;
}

void PGB_presentModal(PGB_Scene* scene)
{
    playdate->system->removeAllMenuItems();
    PGB_App->buttons_suppress |= PGB_App->buttons_down;
    PGB_App->buttons_down = 0;
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

    pgb_free(PGB_App);
}
