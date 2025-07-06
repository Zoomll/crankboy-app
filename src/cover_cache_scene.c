#include "cover_cache_scene.h"

#include "../lz4/lz4hc.h"
#include "app.h"
#include "library_scene.h"
#include "utility.h"

#define MAX_CACHE_SIZE_BYTES (3072 * 1024)  // 3MB

void PGB_CoverCacheScene_update(void* object, uint32_t u32enc_dt);
void PGB_CoverCacheScene_free(void* object);

static void collect_cover_filenames_callback(const char* filename, void* userdata)
{
    if (endswithi(filename, ".pdi"))
    {
        PGB_Array* covers_array = userdata;
        char* basename_no_ext = pgb_basename(filename, true);
        if (basename_no_ext)
        {
            array_push(covers_array, basename_no_ext);
        }
    }
}

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

    cacheScene->available_covers = array_new();

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
        playdate->file->listfiles(
            PGB_coversPath, collect_cover_filenames_callback, cacheScene->available_covers, 0
        );

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
            PGB_Game* game = PGB_Game_new(cachedName, cacheScene->available_covers);
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

        cacheScene->start_time_ms = playdate->system->getCurrentTimeMilliseconds();
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
                    uint8_t *mask_data, *pixel_data;
                    playdate->graphics->getBitmapData(
                        coverBitmap, &width, &height, &rowbytes, &mask_data, &pixel_data
                    );

                    bool has_mask = (mask_data != NULL);
                    size_t original_size = rowbytes * height;
                    if (has_mask)
                    {
                        original_size *= 2;
                    }

                    int max_dst_size = LZ4_compressBound(original_size);
                    char* compressed_buffer = pgb_malloc(max_dst_size);

                    if (compressed_buffer)
                    {
                        uint8_t* uncompressed_buffer = pgb_malloc(original_size);
                        if (uncompressed_buffer)
                        {
                            memcpy(uncompressed_buffer, pixel_data, rowbytes * height);
                            if (has_mask)
                            {
                                memcpy(
                                    uncompressed_buffer + (rowbytes * height), mask_data,
                                    rowbytes * height
                                );
                            }

                            int compressed_size = LZ4_compress_HC(
                                (const char*)uncompressed_buffer, compressed_buffer, original_size,
                                max_dst_size, LZ4HC_CLEVEL_MIN
                            );

                            pgb_free(uncompressed_buffer);

                            if (compressed_size > 0 &&
                                (cacheScene->cache_size_bytes + compressed_size <=
                                 MAX_CACHE_SIZE_BYTES))
                            {
                                PGB_CoverCacheEntry* entry =
                                    pgb_malloc(sizeof(PGB_CoverCacheEntry));

                                entry->rom_path = string_copy(game->fullpath);
                                entry->compressed_data = compressed_buffer;
                                entry->compressed_size = compressed_size;
                                entry->original_size = original_size;
                                entry->width = width;
                                entry->height = height;
                                entry->rowbytes = rowbytes;
                                entry->has_mask = has_mask;

                                array_push(PGB_App->coverCache, entry);
                                cacheScene->cache_size_bytes += compressed_size;
                            }
                            else
                            {
                                pgb_free(compressed_buffer);
                                if (compressed_size > 0)
                                {  // Cache is full
                                    cacheScene->state = kCoverCacheStateDone;
                                }
                            }
                        }
                        else
                        {
                            pgb_free(compressed_buffer);
                        }
                    }

                    playdate->graphics->freeBitmap(coverBitmap);
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
        uint32_t end_time_ms = playdate->system->getCurrentTimeMilliseconds();
        float duration = (end_time_ms - cacheScene->start_time_ms) / 1000.0f;

        playdate->system->logToConsole(
            "Cover Caching Complete: %d covers cached, size: %lu bytes, took %.2f seconds.",
            PGB_App->coverCache->length, (unsigned long)cacheScene->cache_size_bytes, duration
        );

        PGB_LibraryScene* libraryScene = PGB_LibraryScene_new();
        PGB_present(libraryScene->scene);
        break;
    }
    }
}

void PGB_CoverCacheScene_free(void* object)
{
    PGB_CoverCacheScene* cacheScene = object;

    if (cacheScene->available_covers)
    {
        for (int i = 0; i < cacheScene->available_covers->length; i++)
        {
            pgb_free(cacheScene->available_covers->items[i]);
        }
        array_free(cacheScene->available_covers);
    }

    PGB_Scene_free(cacheScene->scene);
    pgb_free(cacheScene);
}
