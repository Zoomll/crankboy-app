//
//  library_scene.h
//  CrankBoy
//
//  Created by Matteo D'Ignazio on 15/05/22.
//  Maintained and developed by the CrankBoy dev team.
//

#pragma once

#include "app.h"
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

typedef enum
{
    COVER_DOWNLOAD_IDLE,
    COVER_DOWNLOAD_SEARCHING,
    COVER_DOWNLOAD_DOWNLOADING,
    COVER_DOWNLOAD_FAILED,
    COVER_DOWNLOAD_NO_GAME_IN_DB,
    COVER_DOWNLOAD_COMPLETE
} CoverDownloadState;

typedef enum
{
    kLibraryStateInit,
    kLibraryStateBuildGameList,
    kLibraryStateSort,
    kLibraryStateBuildUIList,
    kLibraryStateDone
} PGB_LibraryState;

typedef struct PGB_Game
{
    char* filename;
    char* fullpath;
    char* coverPath;
    char* name_short;
    char* name_detailed;
    char* name_original_long;
    char* name_filename;
    uint32_t crc32;

    // points to one of the other strings in this struct;
    // should not be free'd directly
    char* displayName;
} PGB_Game;

typedef struct PGB_LibraryScene
{
    PGB_Scene* scene;
    PGB_Array* games;
    PGB_LibrarySceneModel model;
    PGB_ListView* listView;
    PGB_LibrarySceneTab tab;

    PGB_LibraryState state;
    int build_index;

    bool firstLoad;
    bool initialLoadComplete;
    int lastSelectedItem;
    int last_display_name_mode;

    LCDBitmap* missingCoverIcon;

    CoverDownloadState coverDownloadState;
    char* coverDownloadMessage;
    HTTPConnection* activeCoverDownloadConnection;

    bool showCrc;
    bool isReloading;
} PGB_LibraryScene;

PGB_LibraryScene* PGB_LibraryScene_new(void);

PGB_Game* PGB_Game_new(PGB_GameName* cachedName);
void PGB_Game_free(PGB_Game* game);
