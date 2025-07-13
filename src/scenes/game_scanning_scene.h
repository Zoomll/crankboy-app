// game_scanning_scene.h
#pragma once

#include "jparse.h"
#include "scene.h"
#include "utility.h"

// States for the scanning process
typedef enum
{
    kScanningStateInit,
    kScanningStateScanning,
    kScanningStateDone
} GameScanningState;

typedef struct PGB_GameScanningScene
{
    PGB_Scene* scene;
    PGB_Array* game_filenames;
    int current_index;
    GameScanningState state;
    json_value crc_cache;
    bool crc_cache_modified;
} PGB_GameScanningScene;

PGB_GameScanningScene* PGB_GameScanningScene_new(void);
