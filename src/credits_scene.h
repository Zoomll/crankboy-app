//
//  settings_scene.h
//  CrankBoy
//
//  Maintained and developed by the CrankBoy dev team.
//

#pragma once

#include "game_scene.h"
#include "jparse.h"
#include "scene.h"

#include <stdio.h>

typedef struct PGB_CreditsScene
{
    PGB_Scene* scene;

    json_value jcred;
    int* y_advance_by_item;
    float scroll;
    float time;
    float initial_wait;
    bool shouldDismiss;
    LCDBitmap* logo;
} PGB_CreditsScene;

PGB_CreditsScene* PGB_CreditsScene_new(void);
