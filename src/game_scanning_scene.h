// game_scanning_scene.h
#pragma once

#include "scene.h"
#include "utility.h"

// States for our non-blocking scanning process
typedef enum
{
    kScanningStateInit,
    kScanningStateScanning,
    kScanningStateDone
} GameScanningState;

// The scene's main structure
typedef struct PGB_GameScanningScene
{
    PGB_Scene* scene;
    PGB_Array* game_filenames;
    int current_index;
    GameScanningState state;
} PGB_GameScanningScene;

// Function to create an instance of the scene
PGB_GameScanningScene* PGB_GameScanningScene_new(void);
