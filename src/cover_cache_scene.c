#include "cover_cache_scene.h"

#include "app.h"
#include "library_scene.h"
#include "utility.h"

#define MAX_CACHE_SIZE_BYTES (4096 * 1024)  // 4MB

void PGB_CoverCacheScene_update(void* object, uint32_t u32enc_dt);
void PGB_CoverCacheScene_free(void* object);

PGB_CoverCacheScene* PGB_CoverCacheScene_new(void)
{
    PGB_CoverCacheScene* cacheScene = pgb_calloc(1, sizeof(PGB_CoverCacheScene));

    cacheScene->scene = PGB_Scene_new();
    cacheScene->scene->managedObject = cacheScene;
    cacheScene->scene->update = PGB_CoverCacheScene_update;
    cacheScene->scene->free = PGB_CoverCacheScene_free;
    cacheScene->scene->use_user_stack = false;

    cacheScene->state = kCoverCacheStateInit;
    cacheScene->current_index = 0;
    cacheScene->cache_size_bytes = 0;

    if (PGB_App->coverCache == NULL)
    {
        PGB_App->coverCache = array_new();
    }

    return cacheScene;
}

void PGB_CoverCacheScene_update(void* object, uint32_t u32enc_dt)
{
    if (PGB_App->pendingScene)
    {
        return;
    }

    PGB_CoverCacheScene* cacheScene = object;
    float dt = UINT32_AS_FLOAT(u32enc_dt);

    switch (cacheScene->state)
    {
    case kCoverCacheStateInit:
    {
        if (PGB_App->gameNameCache->length > 0)
        {
            cacheScene->state = kCoverCacheStateBuildGameList;
        }
        else
        {
            cacheScene->state = kCoverCacheStateDone;
        }
        break;
    }

    case kCoverCacheStateBuildGameList:
    {
        if (cacheScene->current_index < PGB_App->gameNameCache->length)
        {
            PGB_GameName* cachedName = PGB_App->gameNameCache->items[cacheScene->current_index];
            PGB_Game* game = PGB_Game_new(cachedName);
            array_push(PGB_App->gameListCache, game);

            char progress_message[100];
            int total = PGB_App->gameNameCache->length;
            int percentage = (total > 0) ? ((float)cacheScene->current_index / total) * 100 : 100;
            snprintf(
                progress_message, sizeof(progress_message), "Building Games List… %d%%", percentage
            );
            pgb_draw_logo_screen_to_buffer(progress_message);

            cacheScene->current_index++;
        }
        else
        {
            PGB_App->gameListCacheIsSorted = false;
            cacheScene->state = kCoverCacheStateSort;
        }
        break;
    }

    case kCoverCacheStateSort:
    {
        pgb_sort_games_array(PGB_App->gameListCache);
        PGB_App->gameListCacheIsSorted = true;
        cacheScene->current_index = 0;
        cacheScene->state = kCoverCacheStateCaching;
        break;
    }

    case kCoverCacheStateCaching:
    {
        if (cacheScene->current_index < PGB_App->gameListCache->length &&
            cacheScene->cache_size_bytes < MAX_CACHE_SIZE_BYTES)
        {
            PGB_Game* game = PGB_App->gameListCache->items[cacheScene->current_index];

            char progress_message[100];
            int total = PGB_App->gameListCache->length;
            int percentage = (total > 0) ? ((float)cacheScene->current_index / total) * 100 : 100;
            snprintf(
                progress_message, sizeof(progress_message), "Caching Covers… %d%%", percentage
            );
            pgb_draw_logo_screen_to_buffer(progress_message);

            if (game->coverPath)
            {
                const char* error = NULL;
                LCDBitmap* coverBitmap = playdate->graphics->loadBitmap(game->coverPath, &error);

                if (coverBitmap)
                {
                    int width, height, rowbytes;
                    uint8_t* mask;
                    playdate->graphics->getBitmapData(
                        coverBitmap, &width, &height, &rowbytes, &mask, NULL
                    );

                    size_t bitmap_size = rowbytes * height;
                    if (mask)
                    {
                        bitmap_size += rowbytes * height;
                    }

                    if (cacheScene->cache_size_bytes + bitmap_size <= MAX_CACHE_SIZE_BYTES)
                    {
                        PGB_CoverCacheEntry* entry = pgb_malloc(sizeof(PGB_CoverCacheEntry));
                        entry->rom_path = string_copy(game->fullpath);
                        entry->bitmap = coverBitmap;
                        array_push(PGB_App->coverCache, entry);
                        cacheScene->cache_size_bytes += bitmap_size;
                    }
                    else
                    {
                        playdate->graphics->freeBitmap(coverBitmap);
                        cacheScene->state = kCoverCacheStateDone;
                    }
                }
            }
            cacheScene->current_index++;
        }
        else
        {
            cacheScene->state = kCoverCacheStateDone;
        }
        break;
    }

    case kCoverCacheStateDone:
    {
        PGB_LibraryScene* libraryScene = PGB_LibraryScene_new();
        PGB_present(libraryScene->scene);
        break;
    }
    }
}

void PGB_CoverCacheScene_free(void* object)
{
    PGB_CoverCacheScene* cacheScene = object;
    PGB_Scene_free(cacheScene->scene);
    pgb_free(cacheScene);
}
