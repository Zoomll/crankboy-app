#pragma once

#include "pd_api.h"
#include "scene.h"
#include "utility.h"

// Just displays some text. Plain and simple.

typedef struct PGB_InfoScene
{
    PGB_Scene* scene;

    char* text;
    float scroll;
    bool canClose;
} PGB_InfoScene;

PGB_InfoScene* PGB_InfoScene_new(char* text);