#pragma once

#include "library_scene.h"
#include "../softpatch.h"

typedef struct PGB_PatchesScene
{
    PGB_Scene* scene;
    PGB_Game* game;
    SoftPatch* patches;
    char* patches_dir;
    bool dismiss : 1;
    bool didDrag : 1;
    unsigned selected;
} PGB_PatchesScene;

PGB_PatchesScene* PGB_PatchesScene_new(PGB_Game* game);