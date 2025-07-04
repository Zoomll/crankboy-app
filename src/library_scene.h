//
//  library_scene.h
//  CrankBoy
//
//  Created by Matteo D'Ignazio on 15/05/22.
//  Maintained and developed by the CrankBoy dev team.
//

#pragma once

#include "array.h"
#include "listview.h"
#include "scene.h"

#include <stdio.h>

typedef enum
{
    PGB_LibrarySceneTabList,
    PGB_LibrarySceneTabEmpty
} PGB_LibrarySceneTab;

typedef struct
{
    bool empty;
    PGB_LibrarySceneTab tab;
} PGB_LibrarySceneModel;

typedef struct PGB_Game
{
    char* filename;
    char* fullpath;
    char* coverPath;
    char* displayName;
} PGB_Game;

typedef struct PGB_LibraryScene
{
    PGB_Scene* scene;
    PGB_Array* games;
    PGB_LibrarySceneModel model;
    PGB_ListView* listView;
    PGB_LibrarySceneTab tab;

    bool firstLoad;
    bool initialLoadComplete;
    int lastSelectedItem;
    int last_display_name_mode;

    LCDBitmap* missingCoverIcon;
} PGB_LibraryScene;

PGB_LibraryScene* PGB_LibraryScene_new(void);

PGB_Game* PGB_Game_new(const char* filename);
void PGB_Game_free(PGB_Game* game);
