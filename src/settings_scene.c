//
// settings_scene.c
//  CrankBoy
//
//  Maintained and developed by the CrankBoy dev team.
//
#include "settings_scene.h"

#include "../minigb_apu/minigb_apu.h"
#include "app.h"
#include "dtcm.h"
#include "modal.h"
#include "preferences.h"
#include "revcheck.h"
#include "userstack.h"
#include "utility.h"

#define MAX_VISIBLE_ITEMS 6
#define SCROLL_INDICATOR_MIN_HEIGHT 10

static void PGB_SettingsScene_update(void *object, float dt);
static void PGB_SettingsScene_free(void *object);
static void PGB_SettingsScene_menu(void *object);
static void PGB_SettingsScene_didSelectBack(void *userdata);
static void PGB_SettingsScene_rebuildEntries(PGB_SettingsScene *settingsScene);
static void PGB_SettingsScene_attemptDismiss(PGB_SettingsScene *settingsScene);
static void settings_load_state(PGB_GameScene *gameScene,
                                PGB_SettingsScene *settingsScene);

bool save_state(PGB_GameScene *gameScene, unsigned slot);
bool load_state(PGB_GameScene *gameScene, unsigned slot);

struct OptionsMenuEntry;

typedef struct OptionsMenuEntry
{
    const char *name;
    const char **values;
    const char *description;
    int *pref_var;
    unsigned max_value;
    void (*on_press)(struct OptionsMenuEntry *,
                     PGB_SettingsScene *settingsScene);
    void *ud;
} OptionsMenuEntry;

OptionsMenuEntry *getOptionsEntries(PGB_GameScene *gameScene);

PGB_SettingsScene *PGB_SettingsScene_new(PGB_GameScene *gameScene)
{
    PGB_SettingsScene *settingsScene = pgb_malloc(sizeof(PGB_SettingsScene));
    memset(settingsScene, 0, sizeof(*settingsScene));
    settingsScene->gameScene = gameScene;
    settingsScene->cursorIndex = 0;
    settingsScene->topVisibleIndex = 0;
    settingsScene->crankAccumulator = 0.0f;
    settingsScene->shouldDismiss = false;
    settingsScene->entries = getOptionsEntries(gameScene);

    settingsScene->clickSynth = playdate->sound->synth->newSynth();
    playdate->sound->synth->setWaveform(settingsScene->clickSynth,
                                        kWaveformSquare);
    playdate->sound->synth->setAttackTime(settingsScene->clickSynth, 0.0f);
    playdate->sound->synth->setDecayTime(settingsScene->clickSynth, 0.05f);
    playdate->sound->synth->setSustainLevel(settingsScene->clickSynth, 0.0f);
    playdate->sound->synth->setReleaseTime(settingsScene->clickSynth, 0.0f);

    settingsScene->totalMenuItemCount = 0;
    if (settingsScene->entries)
    {
        for (int i = 0; settingsScene->entries[i].name; i++)
        {
            settingsScene->totalMenuItemCount++;
        }
    }

    if (gameScene)
    {
        settingsScene->wasAudioLocked = gameScene->audioLocked;
        gameScene->audioLocked = true;
    }

    PGB_Scene *scene = PGB_Scene_new();
    scene->managedObject = settingsScene;
    scene->update = PGB_SettingsScene_update;
    scene->free = PGB_SettingsScene_free;
    scene->menu = PGB_SettingsScene_menu;

    settingsScene->scene = scene;

    PGB_Scene_refreshMenu(scene);

    return settingsScene;
}

static void state_action_modal_callback(void *userdata, int option)
{
    PGB_SettingsScene *settingsScene = userdata;

    if (option == 0)
    {
        settingsScene->shouldDismiss = true;
    }
}

static void settings_load_state(PGB_GameScene *gameScene,
                                PGB_SettingsScene *settingsScene)
{
    if (!load_state(gameScene, 0))
    {
        PGB_presentModal(
            PGB_Modal_new("Failed to load state.", NULL, NULL, NULL)->scene);
        playdate->system->logToConsole("Error loading state %d", 0);
    }
    else
    {
        playdate->system->logToConsole("Loaded save state %d", 0);

        // TODO: something less invasive than a modal here.
        const char *options[] = {"Game", "Settings", NULL};
        PGB_presentModal(PGB_Modal_new("State loaded. Return to:", options,
                                       state_action_modal_callback,
                                       settingsScene)
                             ->scene);
    }
}

typedef struct
{
    PGB_GameScene *gameScene;
    PGB_SettingsScene *settingsScene;
} LoadStateUserdata;

static void settings_confirm_load_state(void *userdata, int option)
{
    LoadStateUserdata *data = userdata;
    if (option == 1)
    {
        settings_load_state(data->gameScene, data->settingsScene);
    }
    free(data);
}

static void PGB_SettingsScene_attemptDismiss(PGB_SettingsScene *settingsScene)
{
    int result = (intptr_t)call_with_user_stack(preferences_save_to_disk);
    if (!result)
    {
        PGB_presentModal(
            PGB_Modal_new("Error saving preferences.", NULL, NULL, NULL)
                ->scene);
    }
    else
    {
        PGB_dismiss(settingsScene->scene);
    }
}

#define ROTATE(var, dir, max)          \
    {                                  \
        var = (var + dir + max) % max; \
    }
#define STRFMT_LAMBDA(...)                                  \
    LAMBDA(char *, (struct OptionsMenuEntry * e), {         \
        char *_RET;                                         \
        playdate->system->formatString(&_RET, __VA_ARGS__); \
        return _RET;                                        \
    })

static const char *sound_mode_labels[] = {"Off", "Fast", "Accurate"};
static const char *off_on_labels[] = {"Off", "On"};
static const char *crank_mode_labels[] = {"Start/Select", "Turbo A/B",
                                          "Turbo B/A"};
static const char *sample_rate_labels[] = {"High", "Medium", "Low"};
static const char *dynamic_rate_labels[] = {"Off", "On", "Auto"};

static void settings_action_save_state(OptionsMenuEntry *e,
                                       PGB_SettingsScene *settingsScene)
{
    PGB_GameScene *gameScene = e->ud;
    // TODO: confirmation if overwriting a state which is >= 10 minutes old
    if (!save_state(gameScene, 0))
    {
        char *msg;
        playdate->system->formatString(&msg, "Error saving state:\n%s",
                                       playdate->file->geterr());
        PGB_presentModal(PGB_Modal_new(msg, NULL, NULL, NULL)->scene);
        free(msg);
    }
    else
    {
        playdate->system->logToConsole("Saved state %d successfully", 0);

        // TODO: something less invasive than a modal here.
        const char *options[] = {"Game", "Settings", NULL};
        PGB_presentModal(PGB_Modal_new("State saved. Return to:", options,
                                       state_action_modal_callback,
                                       settingsScene)
                             ->scene);
    }
}

static void settings_action_load_state(OptionsMenuEntry *e,
                                       PGB_SettingsScene *settingsScene)
{
    PGB_GameScene *gameScene = e->ud;
    // confirmation needed if more than 2 minutes of progress made
    if (gameScene->playtime >= 60 * 120)
    {
        const char *confirm_options[] = {"No", "Yes", NULL};
        LoadStateUserdata *data = malloc(sizeof(LoadStateUserdata));
        data->gameScene = gameScene;
        data->settingsScene = settingsScene;
        PGB_presentModal(PGB_Modal_new("Really load state?", confirm_options,
                                       (void *)settings_confirm_load_state,
                                       data)
                             ->scene);
    }
    else
    {
        settings_load_state(gameScene, settingsScene);
    }
}

OptionsMenuEntry *getOptionsEntries(PGB_GameScene *gameScene)
{
    int max_entries = 16;  // we can overshoot, it's ok
    OptionsMenuEntry *entries = malloc(sizeof(OptionsMenuEntry) * max_entries);
    if (!entries)
        return NULL;
    memset(entries, 0, sizeof(OptionsMenuEntry) * max_entries);

    /* clang-format off */
    int i = -1;

    if (gameScene)
    {
        if (!gameScene->save_states_supported)
        {
            entries[++i] = (OptionsMenuEntry){
                .name = "Save state",
                .values = NULL,
                .description =
                    "CrankBoy does not\ncurrently support\ncreating save states\n"
                    "with a ROM that has its\nown save data.",
                .pref_var = NULL,
                .max_value = 0,
                .on_press = NULL
            };
        }
        else
        {
            // save state
            entries[++i] = (OptionsMenuEntry){
                .name = "Save state",
                .values = NULL,
                .description =
                    "Create a snapshot of\nthis moment, which\ncan be resumed later.",
                .pref_var = NULL,
                .max_value = 0,
                .on_press = settings_action_save_state,
                .ud = gameScene,
            };

            // load state
            entries[++i] = (OptionsMenuEntry){
                .name = "Load state",
                .values = NULL,
                .description =
                    "Restore the previously-\ncreated snapshot."
                ,
                .pref_var = NULL,
                .max_value = 0,
                .on_press = settings_action_load_state,
                .ud = gameScene,
            };
        }
    }

    // sound
    {
        entries[++i] = (OptionsMenuEntry){
            .name = "Sound",
            .values = sound_mode_labels,
            .description =
                "Accurate:\nHighest quality sound.\n \nFast:\nGood balance of\n"
                "quality and speed.\n \nOff:\nNo audio for best\nperformance.",
            .pref_var = &preferences_sound_mode,
            .max_value = 3,
            .on_press = NULL,
        };
    }

    // sample rate
    entries[++i] = (OptionsMenuEntry){
        .name = "Sample Rate",
        .values = sample_rate_labels,
        .description =
            "Adjusts audio quality.\nHigher values may impact\nperformance.\n \n"
            "High:\nBest quality (44.1kHz)\n \n"
            "Medium:\nGood quality (22.0kHz)\n \n"
            "Low:\nReduced quality (14.7kHz)",
        .pref_var = &preferences_sample_rate,
        .max_value = 3,
        .on_press = NULL,
    };

    // frame skip
    entries[++i] = (OptionsMenuEntry){
        .name = "30 FPS mode",
        .values = off_on_labels,
        .description =
            "Skips displaying every\nsecond frame. Greatly\nimproves performance\n"
            "for most games.\n \nDespite appearing to be\n30 FPS, the game "
            "itself\nstill runs at full speed.\n \nEnabling this mode\ndisables "
            "the Interlacing\nsettings.",
        .pref_var = &preferences_frame_skip,
        .max_value = 2,
        .on_press = NULL,
    };

    // dynamic rate adjustment
    if (preferences_frame_skip)
    {
        entries[++i] = (OptionsMenuEntry){
            .name = "Interlacing",
            .values = dynamic_rate_labels,
            .description = "Unavailable in\n30 FPS mode.",
            .pref_var = &preferences_dynamic_rate,
            .max_value = 0,
            .on_press = NULL,
        };
    }
    else
    {
        entries[++i] = (OptionsMenuEntry){
            .name = "Interlacing",
            .values = dynamic_rate_labels,
            .description =
                "Skips lines to keep the\nframerate smooth.\n \n"
                "Off:\nFull quality, no skipping.\n \n"
                "On:\nAlways on for a reliable\nspeed boost.\n \n"
                "Auto:\nRecommended. Skips lines\nonly when needed.",
            .pref_var = &preferences_dynamic_rate,
            .max_value = 3,
            .on_press = NULL,
        };
    }

    // show fps
    entries[++i] = (OptionsMenuEntry){
        .name = "Show FPS",
        .values = off_on_labels,
        .description =
            "Displays the current\nframes-per-second\non screen."
        ,
        .pref_var = &preferences_display_fps,
        .max_value = 2,
        .on_press = NULL
    };

    // crank mode
    entries[++i] = (OptionsMenuEntry){
        .name = "Crank",
        .values = crank_mode_labels,
        .description =
            "Assign a (turbo) function\nto the crank.\n \nStart/Select:\nCW for "
            "Start, CCW for Select.\n \nTurbo A/B:\nCW for A, CCW for B.\n \nTurbo "
            "B/A:\nCW for B, CCW for A.",
        .pref_var = &preferences_crank_mode,
        .max_value = 3,
        .on_press = NULL
    };

    if (!gameScene)
    {
#if defined(ITCM_CORE) && defined(DTCM_ALLOC)
        // itcm accel

        static char *itcm_description = NULL;
        if (itcm_description == NULL) playdate->system->formatString(
                &itcm_description,
            "Unstable, but greatly\nimproves performance.\n\nRuns emulator "
            "core\ndirectly from the stack.\n \nWorks with Rev A.\n "
            "\n(Your device: %s)",
            pd_rev_description
        );
        entries[++i] = (OptionsMenuEntry){
            .name = "ITCM acceleration",
            .values = off_on_labels,
            .description = itcm_description,
            .pref_var = &preferences_itcm,
            .max_value = 2,
            .on_press = NULL
        };
#endif

#ifndef NOLUA
        // lua scripts
        entries[++i] = (OptionsMenuEntry){
            .name = "Game scripts",
            .values = off_on_labels,
            .description =
                "Enable or disable Lua\nscripting support.\n \nEnabling this "
                "may impact\nperformance.",
            .pref_var = &preferences_lua_support,
            .max_value = 2,
            .on_press = NULL,
        };
#endif
    }

    /* clang-format on */

    return entries;
};

static void PGB_SettingsScene_rebuildEntries(PGB_SettingsScene *settingsScene)
{
    if (settingsScene->entries)
    {
        free(settingsScene->entries);
    }

    settingsScene->entries = getOptionsEntries(settingsScene->gameScene);

    settingsScene->totalMenuItemCount = 0;
    if (settingsScene->entries)
    {
        for (int i = 0; settingsScene->entries[i].name; i++)
        {
            settingsScene->totalMenuItemCount++;
        }
    }

    if (settingsScene->cursorIndex >= settingsScene->totalMenuItemCount)
    {
        settingsScene->cursorIndex = settingsScene->totalMenuItemCount - 1;
    }
}

static void PGB_SettingsScene_update(void *object, float dt)
{
    static const uint8_t black_transparent_dither[16] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                            0xFF, 0xFF, 0xAA, 0x55, 0xAA, 0x55,
                                            0xAA, 0x55, 0xAA, 0x55};
    static const uint8_t white_transparent_dither[16] = {0, 0, 0, 0, 0, 0,
                                            0, 0, 0xAA, 0x55, 0xAA, 0x55,
                                            0xAA, 0x55, 0xAA, 0x55};

    PGB_SettingsScene *settingsScene = object;
    int oldCursorIndex = settingsScene->cursorIndex;

    if (settingsScene->shouldDismiss)
    {
        PGB_SettingsScene_attemptDismiss(settingsScene);
        return;
    }

    PGB_GameScene *gameScene = settingsScene->gameScene;

    const int kScreenHeight = 240;
    const int kDividerX = 240;
    const int kLeftPanePadding = 20;
    const int kRightPanePadding = 10;

    int menuItemCount = settingsScene->totalMenuItemCount;

    PGB_Scene_update(settingsScene->scene, dt);

    // Crank
    float crank_change = playdate->system->getCrankChange();
    settingsScene->crankAccumulator += crank_change;
    const float crank_threshold = 45.0f;

    while (settingsScene->crankAccumulator >= crank_threshold)
    {
        settingsScene->cursorIndex++;
        if (settingsScene->cursorIndex >= menuItemCount)
        {
            settingsScene->cursorIndex = menuItemCount - 1;
        }
        settingsScene->crankAccumulator -= crank_threshold;
    }

    while (settingsScene->crankAccumulator <= -crank_threshold)
    {
        settingsScene->cursorIndex--;
        if (settingsScene->cursorIndex < 0)
        {
            settingsScene->cursorIndex = 0;
        }
        settingsScene->crankAccumulator += crank_threshold;
    }

    // Buttons
    PDButtons pushed = PGB_App->buttons_pressed;

    if (pushed & kButtonDown)
    {
        settingsScene->cursorIndex++;
        if (settingsScene->cursorIndex >= menuItemCount)
            settingsScene->cursorIndex = menuItemCount - 1;
    }
    if (pushed & kButtonUp)
    {
        settingsScene->cursorIndex--;
        if (settingsScene->cursorIndex < 0)
            settingsScene->cursorIndex = 0;
    }

    if (oldCursorIndex != settingsScene->cursorIndex &&
        settingsScene->clickSynth)
    {
        playdate->sound->synth->playNote(settingsScene->clickSynth, 1760.0f,
                                         0.7f, 0.1f, 0);
    }

    if (pushed & kButtonB)
    {
        PGB_SettingsScene_attemptDismiss(settingsScene);
        return;
    }

    if (settingsScene->cursorIndex < settingsScene->topVisibleIndex)
    {
        settingsScene->topVisibleIndex = settingsScene->cursorIndex;
    }
    else if (settingsScene->cursorIndex >=
             settingsScene->topVisibleIndex + MAX_VISIBLE_ITEMS)
    {
        settingsScene->topVisibleIndex =
            settingsScene->cursorIndex - (MAX_VISIBLE_ITEMS - 1);
    }

    bool a_pressed = (pushed & kButtonA);
    int direction = !!(pushed & kButtonRight) - !!(pushed & kButtonLeft);

    OptionsMenuEntry *cursor_entry =
        &settingsScene->entries[settingsScene->cursorIndex];

    if (cursor_entry->pref_var && cursor_entry->max_value > 0)
    {
        if (direction == 0)
            direction = a_pressed;

        if (direction != 0)
        {
            int old_value = *cursor_entry->pref_var;

            *cursor_entry->pref_var =
                (old_value + direction + cursor_entry->max_value) %
                cursor_entry->max_value;

            if (old_value != *cursor_entry->pref_var)
            {
                if (settingsScene->clickSynth)
                {
                    playdate->sound->synth->playNote(settingsScene->clickSynth,
                                                     1480.0f, 0.7f, 0.1f, 0);
                }

                if (strcmp(cursor_entry->name, "30 FPS mode") == 0)
                {
                    PGB_SettingsScene_rebuildEntries(settingsScene);
                    cursor_entry =
                        &settingsScene->entries[settingsScene->cursorIndex];
                }
            }
        }
    }
    else if (cursor_entry->on_press && a_pressed)
    {
        cursor_entry->on_press(cursor_entry, settingsScene);
    }

    playdate->graphics->clear(kColorWhite);

    // Draw the 60/40 vertical divider line
    playdate->graphics->drawLine(kDividerX, 0, kDividerX, kScreenHeight, 1,
                                 kColorBlack);

    playdate->graphics->setFont(PGB_App->bodyFont);
    int fontHeight = playdate->graphics->getFontHeight(PGB_App->bodyFont);
    int rowSpacing = 10;
    int rowHeight = fontHeight + rowSpacing;
    int totalMenuHeight = (MAX_VISIBLE_ITEMS * rowHeight) - rowSpacing;
    int initialY = (kScreenHeight - totalMenuHeight) / 2;

    // --- Left Pane (Options - 60%) ---

    for (int i = 0; i < MAX_VISIBLE_ITEMS; i++)
    {
        int itemIndex = settingsScene->topVisibleIndex + i;

        if (itemIndex >= menuItemCount)
        {
            break;
        }

        OptionsMenuEntry *current_entry = &settingsScene->entries[itemIndex];
        bool is_static_text = (current_entry->pref_var == NULL &&
                               current_entry->on_press == NULL);
        bool is_locked_option =
            (current_entry->pref_var != NULL && current_entry->max_value == 0);
        bool is_disabled = is_static_text || is_locked_option;

        int y = initialY + i * rowHeight;
        const char *name = current_entry->name;
        const char *stateText =
            current_entry->values
                ? current_entry->values[*current_entry->pref_var]
                : "";

        int nameWidth = playdate->graphics->getTextWidth(
            PGB_App->bodyFont, name, strlen(name), kUTF8Encoding, 0);
        int stateWidth = playdate->graphics->getTextWidth(
            PGB_App->bodyFont, stateText, strlen(stateText), kUTF8Encoding, 0);
        int stateX = kDividerX - stateWidth - kLeftPanePadding;

        if (itemIndex == settingsScene->cursorIndex)
        {
            playdate->graphics->fillRect(0, y - (rowSpacing / 2), kDividerX,
                                         rowHeight, kColorBlack);
            playdate->graphics->setDrawMode(kDrawModeFillWhite);
        }
        else
        {
            playdate->graphics->setDrawMode(kDrawModeFillBlack);
        }

        // Draw the option name (left-aligned)
        playdate->graphics->drawText(name, strlen(name), kUTF8Encoding,
                                     kLeftPanePadding, y);

        if (stateText[0])
        {
            // Draw the state (right-aligned)
            playdate->graphics->drawText(stateText, strlen(stateText),
                                         kUTF8Encoding, stateX, y);
        }

        if (is_disabled)
        {
            uint8_t* dither = (itemIndex != settingsScene->cursorIndex)
                ? black_transparent_dither
                : white_transparent_dither;
            playdate->graphics->fillRect(kLeftPanePadding, y, nameWidth,
                                         fontHeight, (LCDColor)dither);
            if (stateText[0])
            {
                playdate->graphics->fillRect(stateX, y, stateWidth, fontHeight,
                                             (LCDColor)dither);
            }
        }
    }

    playdate->graphics->setDrawMode(kDrawModeFillBlack);

    if (menuItemCount > MAX_VISIBLE_ITEMS)
    {
        int scrollAreaY = initialY - (rowSpacing / 2);
        int scrollAreaHeight = totalMenuHeight + rowSpacing;

        float calculatedHeight = (float)scrollAreaHeight *
                                 ((float)MAX_VISIBLE_ITEMS / menuItemCount);

        float handleHeight =
            PGB_MAX(calculatedHeight, SCROLL_INDICATOR_MIN_HEIGHT);

        float handleY =
            (float)scrollAreaY +
            ((float)scrollAreaHeight *
             ((float)settingsScene->topVisibleIndex / menuItemCount));

        int indicatorX = kDividerX - 4;
        int indicatorWidth = 2;

        playdate->graphics->fillRect(indicatorX - 1, (int)handleY - 1,
                                     indicatorWidth + 2, (int)handleHeight + 2,
                                     kColorWhite);

        playdate->graphics->fillRect(indicatorX, (int)handleY, indicatorWidth,
                                     (int)handleHeight, kColorBlack);
    }

    // --- Right Pane (Description - 40%) ---
    playdate->graphics->setFont(PGB_App->labelFont);

    const char *description = cursor_entry->description;

    if (description)
    {
        char descriptionCopy[512];
        strncpy(descriptionCopy, description, sizeof(descriptionCopy));
        descriptionCopy[sizeof(descriptionCopy) - 1] = '\0';

        char *line = strtok(descriptionCopy, "\n");

        int descY = initialY;
        int descLineHeight =
            playdate->graphics->getFontHeight(PGB_App->labelFont) + 2;

        while (line != NULL)
        {
            // Draw text in the right pane, with 10px padding from divider
            playdate->graphics->drawText(line, strlen(line), kUTF8Encoding,
                                         kDividerX + kRightPanePadding, descY);
            descY += descLineHeight;
            line = strtok(NULL, "\n");
        }
    }
}

static void PGB_SettingsScene_didSelectBack(void *userdata)
{
    PGB_SettingsScene *settingsScene = userdata;
    settingsScene->shouldDismiss = true;
}

static void PGB_SettingsScene_menu(void *object)
{
    PGB_SettingsScene *settingsScene = object;
    playdate->system->removeAllMenuItems();

    if (settingsScene->gameScene)
    {
        playdate->system->addMenuItem("Resume", PGB_SettingsScene_didSelectBack,
                                      settingsScene);
    }
    else
    {
        playdate->system->addMenuItem(
            "Library", PGB_SettingsScene_didSelectBack, settingsScene);
    }
}

static void PGB_SettingsScene_free(void *object)
{
    DTCM_VERIFY();
    PGB_SettingsScene *settingsScene = object;

    if (settingsScene->clickSynth)
    {
        playdate->sound->synth->freeSynth(settingsScene->clickSynth);
    }

    if (settingsScene->gameScene)
    {
        PGB_GameScene_apply_settings(settingsScene->gameScene);
        settingsScene->gameScene->audioLocked = settingsScene->wasAudioLocked;
    }

    if (settingsScene->entries)
        free(settingsScene->entries);

    PGB_Scene_free(settingsScene->scene);
    pgb_free(settingsScene);
    DTCM_VERIFY();
}
