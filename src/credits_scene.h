//
//  settings_scene.h
//  CrankBoy
//
//  Maintained and developed by the CrankBoy dev team.
//

#pragma once

#include <stdio.h>

#include "game_scene.h"
#include "scene.h"
#include "jparse.h"

typedef struct PGB_CreditsScene
{
    PGB_Scene *scene;
    
    json_value jcred;
    int* y_advance_by_item;
    float scroll;
    float time;
    bool shouldDismiss;
} PGB_CreditsScene;

PGB_CreditsScene *PGB_CreditsScene_new(void);
