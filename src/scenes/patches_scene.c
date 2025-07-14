#include "patches_scene.h"

static void PGB_PatchesScene_update(void* object, uint32_t u32enc_dt)
{
    PGB_PatchesScene* patchesScene = object;
    float dt = UINT32_AS_FLOAT(u32enc_dt);
    
    if (patchesScene->dismiss)
    {
        PGB_dismiss(patchesScene->scene);
        return;
    }
    
    playdate->graphics->clear(kColorWhite);
    
    if (!patchesScene->patches || !patchesScene->patches[0].fullpath)
    {
        int margin = 8;
        LCDFont* font = PGB_App->bodyFont;
        char* msg = aprintf(
            "No patches found for %s.\n \n"
            "1. Place your Playdate in disk mode by holding LEFT+MENU+LOCK for ten seconds.\n"
            "2. From a connected device, add .ips patches to Data/*crankboy/%s\n"
            "3. Finally, enable them from this screen (settings > Patches).\n\n"
            "You may be able to find .ips patches for %s by searching on romhacking.net or romhack.ing",
            
            patchesScene->game->names->name_short_leading_article,
            patchesScene->patches_dir,
            patchesScene->game->names->name_short_leading_article
        );
        playdate->graphics->setFont(font);
        playdate->graphics->drawTextInRect(msg, strlen(msg), kUTF8Encoding, margin, margin, LCD_COLUMNS - margin*2, LCD_ROWS - margin*2, kWrapWord, kAlignTextLeft);
        pgb_free(msg);
    }
    else
    {
        
    }
    
    if (PGB_App->buttons_pressed & kButtonB)
    {
        patchesScene->dismiss = true;
    }
}

static void PGB_PatchesScene_free(void* object)
{
    PGB_PatchesScene* patchesScene = object;
    PGB_Scene_free(patchesScene->scene);
    
    pgb_free(patchesScene->patches_dir);
    free_patches(patchesScene->patches);
    pgb_free(patchesScene);
}

static void PGB_PatchesScene_menu(void* object)
{
    // TODO -- "return" button
}

PGB_PatchesScene* PGB_PatchesScene_new(PGB_Game* game)
{
    PGB_Scene* scene = PGB_Scene_new();
    PGB_PatchesScene* patchesScene = allocz(PGB_PatchesScene);
    patchesScene->scene = scene;
    scene->managedObject = patchesScene;
    
    patchesScene->game = game;
    patchesScene->patches_dir = get_patches_directory(game->fullpath);
    patchesScene->patches = list_patches(game->fullpath, NULL);
    
    scene->update = PGB_PatchesScene_update;
    scene->free = PGB_PatchesScene_free;
    scene->menu = PGB_PatchesScene_menu;
    
    return patchesScene;
}