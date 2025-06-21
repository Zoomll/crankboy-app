//
//  settings_scene.h
//  CrankBoy
//
//  Maintained and developed by the CrankBoy dev team.
//

#ifndef settings_scene_h
#define settings_scene_h

#include "game_scene.h"
#include "scene.h"

#include <stdio.h>

struct OptionsMenuEntry;
struct PDSynth;

typedef struct PGB_SettingsScene
{
    PGB_Scene* scene;
    PGB_GameScene* gameScene;

    int cursorIndex;
    int topVisibleIndex;
    int totalMenuItemCount;
    float crankAccumulator;
    bool shouldDismiss : 1;
    bool wasAudioLocked : 1;

    int initial_sound_mode;
    int initial_sample_rate;

    struct OptionsMenuEntry* entries;
    struct PDSynth* clickSynth;

    uint8_t thumbnail[SAVE_STATE_THUMBNAIL_H * ((SAVE_STATE_THUMBNAIL_W + 7) / 8)];
} PGB_SettingsScene;

PGB_SettingsScene* PGB_SettingsScene_new(PGB_GameScene* gameScene);

#endif /* settings_scene_h */
