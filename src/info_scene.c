#include "info_scene.h"

#include "app.h"

// pixels per degree
#define CRANK_RATE 1.1f

// pixels per second
#define SCROLL_RATE 80.3f

static void PGB_InfoScene_update(void* object, uint32_t u32enc_dt)
{
    LCDFont* font = PGB_App->bodyFont;

    int margin = 14;
    int width = LCD_COLUMNS - margin * 2;
    int tracking = 0;
    int extraLeading = 0;

    PGB_InfoScene* infoScene = object;
    float dt = UINT32_AS_FLOAT(u32enc_dt);

    infoScene->scroll += playdate->system->getCrankChange() * CRANK_RATE;

    int buttonsDown = PGB_App->buttons_down;
    int scrollDir = !!(buttonsDown & kButtonDown) - !!(buttonsDown & kButtonUp);
    infoScene->scroll += scrollDir * dt * SCROLL_RATE;

    int textHeight = playdate->graphics->getTextHeightForMaxWidth(
        font, infoScene->text, strlen(infoScene->text), width, kUTF8Encoding, kWrapWord, tracking,
        extraLeading
    );

    if (infoScene->scroll > textHeight - PGB_LCD_HEIGHT)
        infoScene->scroll = textHeight - PGB_LCD_HEIGHT;
    if (infoScene->scroll < 0)
        infoScene->scroll = 0;

    playdate->graphics->clear(kColorWhite);

    playdate->graphics->drawTextInRect(
        infoScene->text, strlen(infoScene->text), kUTF8Encoding, margin,
        -infoScene->scroll + margin, width, textHeight * 2, kWrapWord, kAlignTextLeft
    );

    playdate->graphics->display();

    if (buttonsDown & (kButtonB | kButtonA))
    {
        PGB_dismiss(infoScene->scene);
    }
}

static void PGB_InfoScene_free(void* object)
{
    PGB_InfoScene* infoScene = object;
}

PGB_InfoScene* PGB_InfoScene_new(char* text)
{
    PGB_InfoScene* infoScene = pgb_malloc(sizeof(PGB_InfoScene));
    memset(infoScene, 0, sizeof(*infoScene));
    playdate->system->getCrankChange();

    PGB_Scene* scene = PGB_Scene_new();
    infoScene->scene = scene;
    infoScene->text = strdup(text);
    scene->managedObject = infoScene;
    scene->update = (void*)PGB_InfoScene_update;
    scene->free = (void*)PGB_InfoScene_free;

    return infoScene;
}