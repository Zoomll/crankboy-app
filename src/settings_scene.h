//
//  settings_scene.h
//  CrankBoy
//
//  Maintained and developed by the CrankBoy dev team.
//

#ifndef settings_scene_h
#define settings_scene_h

#include <stdio.h>

#include "game_scene.h"
#include "scene.h"

typedef struct PGB_SettingsScene
{
    PGB_Scene *scene;
    PGB_GameScene *gameScene;

    int cursorIndex;
    float crankAccumulator;
    bool shouldDismiss;

} PGB_SettingsScene;

PGB_SettingsScene *PGB_SettingsScene_new(PGB_GameScene *gameScene);

#endif /* settings_scene_h */
