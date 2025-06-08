//
// settings_scene.c
//  CrankBoy
//
//  Maintained and developed by the CrankBoy dev team.
//
#include "settings_scene.h"

#include "../minigb_apu/minigb_apu.h"
#include "app.h"
#include "preferences.h"

static void PGB_SettingsScene_update(void *object, float dt);
static void PGB_SettingsScene_free(void *object);
static void PGB_SettingsScene_menu(void *object);
static void PGB_SettingsScene_didSelectBack(void *userdata);

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

    PGB_Scene_update(settingsScene->scene, dt);

    PDButtons pushed;
    playdate->system->getButtonState(NULL, &pushed, NULL);

    if (pushed & kButtonDown)
    {
        settingsScene->cursorIndex++;
        if (settingsScene->cursorIndex > 2)
            settingsScene->cursorIndex = 2;
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
    }

    playdate->graphics->clear(kColorWhite);

    // Draw the 60/40 vertical divider line
    int dividerX = 240;
    playdate->graphics->drawLine(dividerX, 0, dividerX, 240, 1, kColorBlack);

    // --- Left Pane (Options - 60%) ---
    playdate->graphics->setFont(PGB_App->bodyFont);

    const char *options[] = {"Sound", "30FPS Mode", "Show FPS"};
    bool values[] = {preferences_sound_enabled, preferences_frame_skip,
                     preferences_display_fps};

    for (int i = 0; i < 3; i++)
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
        // Draw the state (right-aligned)
        playdate->graphics->drawText(stateText, strlen(stateText),
                                     kUTF8Encoding, stateX, y);
    }
    playdate->graphics->setDrawMode(kDrawModeFillBlack);

    // --- Right Pane (Description - 40%) ---
    playdate->graphics->setFont(PGB_App->labelFont);

    const char *descriptions[] = {
        "Toggles all in-game\naudio. Muting may\nimprove performance.",
        "Limits framerate to\n30 FPS. Improves\nperformance in\ndemanding "
        "games.",
        "Displays the current\nframes-per-second\non screen."};

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
    PGB_SettingsScene *settingsScene = object;

    if (settingsScene->gameScene)
    {
        settingsScene->gameScene->audioLocked = false;
    }

    PGB_Scene_free(settingsScene->scene);
    pgb_free(settingsScene);
}
