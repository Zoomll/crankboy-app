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

    // Draw the 60/40 vertical divider line (at 60% of 400px width)
    playdate->graphics->drawLine(240, 0, 240, 240, 1, kColorBlack);

    // --- Left Pane (Options - 60%) ---
    playdate->graphics->setFont(PGB_App->bodyFont);

    const char *options[] = {"Sound", "30FPS Mode", "Show FPS"};
    bool values[] = {preferences_sound_enabled, preferences_frame_skip,
                     preferences_display_fps};

    for (int i = 0; i < 3; i++)
    {
        char fullText[50];
        snprintf(fullText, 50, "%s: %s", options[i], values[i] ? "On" : "Off");

        if (i == settingsScene->cursorIndex)
        {
            // Highlight selection, constrained to the left pane
            playdate->graphics->fillRect(0, 50 + i * 30 - 2, 240, 24,
                                         kColorBlack);
            playdate->graphics->setDrawMode(kDrawModeFillWhite);
        }
        else
        {
            playdate->graphics->setDrawMode(kDrawModeFillBlack);
        }
        playdate->graphics->drawText(fullText, strlen(fullText), kUTF8Encoding,
                                     20, 50 + i * 30);
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
    int y = 50;  // Starting Y position for description text
    int lineHeight = playdate->graphics->getFontHeight(PGB_App->labelFont) + 2;

    while (line != NULL)
    {
        // Draw text in the right pane (x > 240), with 10px padding
        playdate->graphics->drawText(line, strlen(line), kUTF8Encoding, 250, y);
        y += lineHeight;
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
