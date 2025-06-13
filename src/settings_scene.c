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

static void PGB_SettingsScene_update(void *object, float dt);
static void PGB_SettingsScene_free(void *object);
static void PGB_SettingsScene_menu(void *object);
static void PGB_SettingsScene_didSelectBack(void *userdata);

bool save_state(PGB_GameScene *gameScene, unsigned slot);
bool load_state(PGB_GameScene *gameScene, unsigned slot);

typedef struct OptionsMenuEntry {
    const char* name;
    const char** values;
    const char* description;
    int* pref_var;
    unsigned max_value;
    void (*on_press)(struct OptionsMenuEntry*);
    void* ud;
} OptionsMenuEntry;

OptionsMenuEntry* getOptionsEntries(PGB_GameScene* gameScene);

PGB_SettingsScene *PGB_SettingsScene_new(PGB_GameScene *gameScene)
{
    PGB_SettingsScene *settingsScene = pgb_malloc(sizeof(PGB_SettingsScene));
    memset(settingsScene, 0, sizeof(*settingsScene));
    settingsScene->gameScene = gameScene;
    settingsScene->cursorIndex = 0;
    settingsScene->crankAccumulator = 0.0f;
    settingsScene->shouldDismiss = false;
    settingsScene->entries = getOptionsEntries(gameScene);

    if (gameScene)
    {
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

static void settings_load_state(PGB_GameScene *gameScene)
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
        PGB_presentModal(
            PGB_Modal_new("Loaded saved state successfully.", NULL, NULL, NULL)
                ->scene);
    }
}

static void settings_confirm_load_state(PGB_GameScene *gameScene, int option)
{
    if (option == 1)
    {
        settings_load_state(gameScene);
    }
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

#define ROTATE(var, dir, max) {var = (var + dir + max) % max;}
#define STRFMT_LAMBDA(...) LAMBDA(char*, (struct OptionsMenuEntry* e), { char* _RET; playdate->system->formatString(&_RET, __VA_ARGS__); return _RET; })

static const char *sound_mode_labels[] = {"Off", "Fast", "Accurate"};
static const char *off_on_labels[] = {"Off", "On"};
const char *crank_mode_labels[] = {"Start/Select", "Turbo A/B", "Turbo B/A"};

static void settings_action_save_state(OptionsMenuEntry* e)
{
    PGB_GameScene* gameScene = e->ud;
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
        playdate->system->logToConsole("Saved state %d successfully",
                                        0);

        // TODO: something less invasive than a modal here.
        PGB_presentModal(
            PGB_Modal_new("State saved successfully.", NULL, NULL, NULL)
                ->scene);
    }
}

static void settings_action_load_state(OptionsMenuEntry* e)
{
    PGB_GameScene* gameScene = e->ud;
    // confirmation needed if more than 2 minutes of progress made
    if (gameScene->playtime >= 60 * 120)
    {
        const char *confirm_options[] = {"No", "Yes", NULL};
        PGB_presentModal(
            PGB_Modal_new("Really load state?", confirm_options,
                            (void *)settings_confirm_load_state,
                            gameScene)
                ->scene);
    }
    else
    {
        settings_load_state(gameScene);
    }
}

OptionsMenuEntry* getOptionsEntries(PGB_GameScene* gameScene)
{
    int max_entries = 15; // we can overshoot, it's ok
    OptionsMenuEntry* entries = malloc(sizeof(OptionsMenuEntry) * max_entries);
    if (!entries) return NULL;
    memset(entries, 0, sizeof(OptionsMenuEntry)*max_entries);
    
    /* clang-format off */
    
    // sound
    int i = -1;
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
    
    // frame skip
    entries[++i] = (OptionsMenuEntry){
        .name = "30 FPS mode",
        .values = off_on_labels,
        .description =
            "Skips displaying every\nsecond frame. Greatly\nimproves performance\n"
            "for most games.\n \nDespite appearing to be\n30 FPS, the game "
            "itself\nstill runs at full speed.",
        .pref_var = &preferences_frame_skip,
        .max_value = 2,
        .on_press = NULL,
    };
    
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
            "B/A:\nCW for B, CCW for A."
        ,
        .pref_var = &preferences_crank_mode,
        .max_value = 3,
        .on_press = NULL
    };
    
    if (!gameScene)
    {
        #if defined(ITCM_CORE) && defined(DTCM_ALLOC)
        // itcm accel
        
        static char* itcm_description = NULL;
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
                "may impact\nperformance."
            ,
            .pref_var = &preferences_lua_support,
            .max_value = 2,
            .on_press = NULL,
        };
        #endif
    }
    
    if (gameScene)
    {
        if (!gameScene->save_states_supported)
        {
            entries[++i] = (OptionsMenuEntry){
                .name = "(save state)",
                .values = NULL,
                .description =
                    "CrankBoy does not\ncurrently support\ncreating save\nstates "
                    "with a\nROM that has\nits own save data.",
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
                    "Create a snapshot of\nthis moment, which\ncan be resumed later."
                ,
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
                    "Restore the previously\n-created snapshot."
                ,
                .pref_var = NULL,
                .max_value = 0,
                .on_press = settings_action_load_state,
                .ud = gameScene,
            };
        }
    }
    
    /* clang-format on */
    
    return entries;
};

static void PGB_SettingsScene_update(void *object, float dt)
{
    PGB_SettingsScene *settingsScene = object;

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

    int menuItemCount;
    for (menuItemCount = 0; settingsScene->entries && settingsScene->entries[menuItemCount].name; ++menuItemCount)
        ;
    
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

    bool cursorMoved = false;
    if (pushed & kButtonDown)
    {
        cursorMoved = true;
        settingsScene->cursorIndex++;
        if (settingsScene->cursorIndex >= menuItemCount)
            settingsScene->cursorIndex = menuItemCount - 1;
    }
    if (pushed & kButtonUp)
    {
        cursorMoved = true;
        settingsScene->cursorIndex--;
        if (settingsScene->cursorIndex < 0)
            settingsScene->cursorIndex = 0;
    }
    if (pushed & kButtonB)
    {
        PGB_SettingsScene_attemptDismiss(settingsScene);
        return;
    }

    bool a_pressed = (pushed & kButtonA);
    int direction = !!(pushed & kButtonRight) - !!(pushed & kButtonLeft);

    if (settingsScene->entries[settingsScene->cursorIndex].pref_var)
    {
        if (direction == 0) direction = a_pressed;
        *settingsScene->entries[settingsScene->cursorIndex].pref_var = (
            *settingsScene->entries[settingsScene->cursorIndex].pref_var
            + direction + settingsScene->entries[settingsScene->cursorIndex].max_value
        ) % settingsScene->entries[settingsScene->cursorIndex].max_value;
    }
    else if (settingsScene->entries[settingsScene->cursorIndex].on_press && a_pressed)
    {
        settingsScene->entries[settingsScene->cursorIndex].on_press(
            &settingsScene->entries[settingsScene->cursorIndex]
        );
    }
        
    playdate->graphics->clear(kColorWhite);

    // Draw the 60/40 vertical divider line
    playdate->graphics->drawLine(kDividerX, 0, kDividerX, kScreenHeight, 1,
                                 kColorBlack);

    playdate->graphics->setFont(PGB_App->bodyFont);
    int fontHeight = playdate->graphics->getFontHeight(PGB_App->bodyFont);
    int rowSpacing = 10;
    int rowHeight = fontHeight + rowSpacing;
    int totalMenuHeight = (menuItemCount * rowHeight) - rowSpacing;

    int initialY = (kScreenHeight - totalMenuHeight) / 2;

    // --- Left Pane (Options - 60%) ---
    
    for (int i = 0; i < menuItemCount; i++)
    {
        int y = initialY + i * rowHeight;
        const char* name = settingsScene->entries[i].name;
        const char* stateText = settingsScene->entries[i].values
            ? settingsScene->entries[i].values[*settingsScene->entries[i].pref_var]
            : "";

        int stateWidth = playdate->graphics->getTextWidth(
            PGB_App->bodyFont, stateText, strlen(stateText), kUTF8Encoding, 0);
        int stateX = kDividerX - stateWidth - kLeftPanePadding;

        if (i == settingsScene->cursorIndex)
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
        playdate->graphics->drawText(name, strlen(name),
                                     kUTF8Encoding, kLeftPanePadding, y);
                                     
        if (stateText[0])
        {
            // Draw the state (right-aligned)
            playdate->graphics->drawText(stateText, strlen(stateText),
                                         kUTF8Encoding, stateX, y);
        }
    }
    playdate->graphics->setDrawMode(kDrawModeFillBlack);

    // --- Right Pane (Description - 40%) ---
    playdate->graphics->setFont(PGB_App->labelFont);


    const char* description = settingsScene->entries[settingsScene->cursorIndex].description;
    if (description)
    {
        // strtok modifies the string, so we need a mutable copy
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

    if (settingsScene->gameScene)
    {
        settingsScene->gameScene->audioLocked = false;
    }

    if (settingsScene->entries) free(settingsScene->entries);

    PGB_Scene_free(settingsScene->scene);
    pgb_free(settingsScene);
    DTCM_VERIFY();
}
