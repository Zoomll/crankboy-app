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

typedef struct CB_CoverCacheScene
{
    CB_Scene* scene;
    int current_index;
    size_t cache_size_bytes;
    CoverCachingState state;
    CB_Array* available_covers;
    CB_Array* games_with_covers;
    uint32_t start_time_ms;
    void* lz4_state;
} CB_CoverCacheScene;

CB_CoverCacheScene* CB_CoverCacheScene_new(void);
