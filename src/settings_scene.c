//
// settings_scene.c
//  CrankBoy
//
//  Maintained and developed by the CrankBoy dev team.
//
#include "settings_scene.h"

#include "revcheck.h"
#include "../minigb_apu/minigb_apu.h"
#include "app.h"
#include "preferences.h"
#include "dtcm.h"
#include "userstack.h"

static void PGB_SettingsScene_update(void *object, float dt);
static void PGB_SettingsScene_free(void *object);
static void PGB_SettingsScene_menu(void *object);
static void PGB_SettingsScene_didSelectBack(void *userdata);

bool save_state(PGB_GameScene *gameScene, unsigned slot);
bool load_state(PGB_GameScene *gameScene, unsigned slot);

PGB_SettingsScene *PGB_SettingsScene_new(PGB_GameScene *gameScene)
{
    PGB_SettingsScene *settingsScene = pgb_malloc(sizeof(PGB_SettingsScene));
    settingsScene->gameScene = gameScene;
    settingsScene->cursorIndex = 0;

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

static void PGB_SettingsScene_update(void *object, float dt)
{
    PGB_SettingsScene *settingsScene = object;
    PGB_GameScene *gameScene = settingsScene->gameScene;

    // In the Game Scene we can show all items.
    // But in the Library Scene only Sound, 30FPS Mode & FPS
    int menuItemCount = gameScene ? 5 : 3;

    PGB_Scene_update(settingsScene->scene, dt);

    PDButtons pushed;
    playdate->system->getButtonState(NULL, &pushed, NULL);
    
    const char *options[] = {"Sound", "30 FPS Mode", "Show FPS", "Save State",
                             "Load State"};
    if (!gameScene || !gameScene->save_states_supported)
    {
        options[3] = "(save state)";
        options[4] = "(load state)";
    }
    bool values[] = {preferences_sound_enabled, preferences_frame_skip,
                     preferences_display_fps, 0, 0};
    bool is_option[] = {1, 1, 1, 0, 0};
    
    const char *descriptions[] = {
        "Toggles all in-game\naudio. Muting may\nimprove performance.",
        "Skips displaying every\nsecond frame. Greatly\nimproves performance\n"
        "for most games.\n \nDespite appearing to be\n30 FPS, the game itself\nstill runs at full speed.",
        "Displays the current\nframes-per-second\non screen.",
        "Create a snapshot of\nthis moment, which\ncan be resumed later.",
        "Load the previously\ncreated snapshot.",
    };

    if (!gameScene || !gameScene->save_states_supported)
    {
        descriptions[3] = descriptions[4] =
            "CrankBoy does not\ncurrently support\ncreating save\nstates with "
            "a\nROM that has\nits own save data.";
    }
    
    #if defined(ITCM_CORE) && defined(DTCM_ALLOC)
    if (!gameScene)
    {
        options[3] = "ITCM acceleration";
        values[3] = preferences_itcm;
        is_option[3] = 1;
        static char* itcm_description = NULL;
        if (itcm_description == NULL)
        {
            playdate->system->formatString(
                &itcm_description,
                "Unstable, but greatly\nimproves performance.\n\nRuns emulator core\ndirectly from the stack.\n \nWorks with Rev A.\n \n(Your device: %s)",
                pd_rev_description
            );
        }
        descriptions[3] = itcm_description ? itcm_description : "";
        ++menuItemCount;
    }
    #endif

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
    if (pushed & kButtonA)
    {
        if (settingsScene->cursorIndex == 0)
        {  // Sound
            preferences_sound_enabled = !preferences_sound_enabled;
            audio_enabled = preferences_sound_enabled ? 1 : 0;

            if (gameScene)
            {
                gameScene->audioEnabled = preferences_sound_enabled;
                gameScene->context->gb->direct.sound =
                    preferences_sound_enabled ? 1 : 0;
                audioGameScene = preferences_sound_enabled ? gameScene : NULL;
            }
        }
        else if (settingsScene->cursorIndex == 1)
        {  // 30FPS Mode
            preferences_frame_skip = !preferences_frame_skip;

            if (gameScene)
            {
                gameScene->context->gb->direct.frame_skip =
                    preferences_frame_skip ? 1 : 0;
            }
        }
        else if (settingsScene->cursorIndex == 2)
        {  // Show FPS
            preferences_display_fps = !preferences_display_fps;
        }
        else if (settingsScene->cursorIndex == 3 && is_option[3])
        {
            preferences_itcm ^= 1;
        }
        else if (settingsScene->cursorIndex == 3 && gameScene &&
                 gameScene->save_states_supported)
        {  // save state
            if (!save_state(gameScene, 0))
            {
                // TODO: pop-up message
                playdate->system->logToConsole("Error saving state %d", 0);
            }
            else
            {
                playdate->system->logToConsole("Saved state %d successfully",
                                               0);
            }
        }
        else if (settingsScene->cursorIndex == 4 && gameScene &&
                 gameScene->save_states_supported)
        {  // load state
            if (!load_state(gameScene, 0))
            {
                // TODO: pop-up message
                playdate->system->logToConsole("Error loading state %d", 0);
            }
            else
            {
                playdate->system->logToConsole("Loaded save state %d", 0);
            }
        }
    }

    playdate->graphics->clear(kColorWhite);

    // Draw the 60/40 vertical divider line
    int dividerX = 240;
    playdate->graphics->drawLine(dividerX, 0, dividerX, 240, 1, kColorBlack);

    // --- Left Pane (Options - 60%) ---
    playdate->graphics->setFont(PGB_App->bodyFont);

    for (int i = 0; i < menuItemCount; i++)
    {
        int y = 50 + i * 30;
        const char *stateText = values[i] ? "On" : "Off";

        // Calculate position for right-aligned state text
        int stateWidth = playdate->graphics->getTextWidth(
            PGB_App->bodyFont, stateText, strlen(stateText), kUTF8Encoding, 0);
        int stateX =
            dividerX - stateWidth - 20;  // 20px padding from the divider

        if (i == settingsScene->cursorIndex)
        {
            // Highlight selection
            playdate->graphics->fillRect(0, y - 2, dividerX, 24, kColorBlack);
            playdate->graphics->setDrawMode(kDrawModeFillWhite);
        }
        else
        {
            playdate->graphics->setDrawMode(kDrawModeFillBlack);
        }

        // Draw the option name (left-aligned)
        playdate->graphics->drawText(options[i], strlen(options[i]),
                                     kUTF8Encoding, 20, y);
        if (is_option[i])
        {
            // Draw the state (right-aligned)
            playdate->graphics->drawText(stateText, strlen(stateText),
                                         kUTF8Encoding, stateX, y);
        }
    }
    playdate->graphics->setDrawMode(kDrawModeFillBlack);

    // --- Right Pane (Description - 40%) ---
    playdate->graphics->setFont(PGB_App->labelFont);

    const char *selectedDescription = descriptions[settingsScene->cursorIndex];

    // strtok modifies the string, so we need a mutable copy
    char descriptionCopy[256];
    strncpy(descriptionCopy, selectedDescription, sizeof(descriptionCopy));

    char *line = strtok(descriptionCopy, "\n");
    int descY = 50;  // Starting Y position for description text
    int lineHeight = playdate->graphics->getFontHeight(PGB_App->labelFont) + 2;

    while (line != NULL)
    {
        // Draw text in the right pane, with 10px padding from divider
        playdate->graphics->drawText(line, strlen(line), kUTF8Encoding,
                                     dividerX + 10, descY);
        descY += lineHeight;
        line = strtok(NULL, "\n");
    }
}

static void PGB_SettingsScene_didSelectBack(void *userdata)
{
    PGB_SettingsScene *settingsScene = userdata;
    PGB_dismiss(settingsScene->scene);
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

    PGB_Scene_free(settingsScene->scene);
    pgb_free(settingsScene);
    call_with_user_stack(preferences_save_to_disk);
    DTCM_VERIFY();
}
