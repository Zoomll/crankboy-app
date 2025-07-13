#pragma once

#include "../scene.h"

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
    PGB_Array* games_with_covers;
    uint32_t start_time_ms;
    void* lz4_state;
} PGB_CoverCacheScene;

PGB_CoverCacheScene* PGB_CoverCacheScene_new(void);
