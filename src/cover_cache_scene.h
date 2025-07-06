#pragma once

#include "scene.h"

#include <stdbool.h>

typedef enum
{
    kCoverCacheStateInit,
    kCoverCacheStateBuildGameList,
    kCoverCacheStateSort,
    kCoverCacheStateCaching,
    kCoverCacheStateDone
} CoverCachingState;

typedef struct PGB_CoverCacheScene
{
    PGB_Scene* scene;
    int current_index;
    size_t cache_size_bytes;
    CoverCachingState state;
    PGB_Array* available_covers;
    uint32_t start_time_ms;
} PGB_CoverCacheScene;

PGB_CoverCacheScene* PGB_CoverCacheScene_new(void);
