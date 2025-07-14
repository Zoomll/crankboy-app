#pragma once

#include "../scene.h"
#include "../utility.h"
#include "pd_api.h"

// Just displays some text. Plain and simple.

typedef struct CB_InfoScene
{
    CB_Scene* scene;

    char* text;
    float scroll;
    bool canClose;
} CB_InfoScene;

CB_InfoScene* CB_InfoScene_new(char* text);
