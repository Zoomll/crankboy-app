//
//  scene.h
//  CrankBoy
//
//  Created by Matteo D'Ignazio on 14/05/22.
//  Maintained and developed by the CrankBoy dev team.
//

#ifndef scene_h
#define scene_h

#include "pd_api.h"
#include "utility.h"

#include <stdio.h>

typedef struct PGB_Scene
{
    void* managedObject;
    struct PGB_Scene* parentScene;

    float preferredRefreshRate;

    bool forceFullRefresh;
    bool use_user_stack;

    void (*update)(void* object, uint32_t u32float_dt);
    void (*menu)(void* object);
    void (*free)(void* object);
    void (*event)(void* object, PDSystemEvent event, uint32_t arg);
} PGB_Scene;

PGB_Scene* PGB_Scene_new(void);

void PGB_Scene_refreshMenu(PGB_Scene* scene);

void PGB_Scene_update(void* scene, uint32_t u32enc_dt);
void PGB_Scene_free(void* scene);

#endif /* scene_h */
