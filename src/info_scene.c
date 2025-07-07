#include "info_scene.h"

#include "app.h"

#include <stdlib.h>
#include <string.h>

// pixels per degree
#define CRANK_RATE 1.1f

// pixels per second
#define SCROLL_RATE 80.3f

// The height of a blank line in pixels.
#define EMPTY_LINE_HEIGHT 15

static void PGB_InfoScene_update(void* object, uint32_t u32enc_dt)
{
    if (PGB_App->pendingScene)
    {
        return;
    }

    PGB_InfoScene* infoScene = object;
    LCDFont* font = PGB_App->bodyFont;
    float dt = UINT32_AS_FLOAT(u32enc_dt);

    int margin = 14;
    int width = LCD_COLUMNS - margin * 2;

    int tracking = 0;
    int extraLeading = 0;

    infoScene->scroll += playdate->system->getCrankChange() * CRANK_RATE;
    int buttonsDown = PGB_App->buttons_down;
    int scrollDir = !!(buttonsDown & kButtonDown) - !!(buttonsDown & kButtonUp);
    infoScene->scroll += scrollDir * dt * SCROLL_RATE;

    float total_text_height = 0;
    int bullet_indent = playdate->graphics->getTextWidth(font, "- ", 2, kUTF8Encoding, tracking);
    const char* text_ptr = infoScene->text;

    while (*text_ptr)
    {
        const char* next_newline = strchr(text_ptr, '\n');
        int line_len = next_newline ? (next_newline - text_ptr) : strlen(text_ptr);

        if (line_len == 0)
        {  // This is an empty line
            total_text_height += EMPTY_LINE_HEIGHT;
        }
        else
        {
            int current_indent = 0;
            const char* text_to_measure = text_ptr;
            int len_to_measure = line_len;

            if (strncmp(text_ptr, "- ", 2) == 0)
            {
                current_indent = bullet_indent;
                text_to_measure += 2;
                len_to_measure -= 2;
            }
            float line_height = playdate->graphics->getTextHeightForMaxWidth(
                font, text_to_measure, len_to_measure, width - current_indent, kUTF8Encoding,
                kWrapWord, tracking, extraLeading
            );
            total_text_height += line_height;
        }

        if (next_newline)
        {
            text_ptr = next_newline + 1;
        }
        else
        {
            break;
        }
    }

    float visible_height = PGB_LCD_HEIGHT - (margin * 2);
    if (total_text_height > visible_height)
    {
        float max_scroll = total_text_height - visible_height;
        if (infoScene->scroll > max_scroll)
            infoScene->scroll = max_scroll;
        if (infoScene->scroll < 0)
            infoScene->scroll = 0;
    }
    else
    {
        infoScene->scroll = 0;
    }

    playdate->graphics->clear(kColorWhite);
    float current_y = margin - infoScene->scroll;
    text_ptr = infoScene->text;

    while (*text_ptr)
    {
        const char* next_newline = strchr(text_ptr, '\n');
        int line_len = next_newline ? (next_newline - text_ptr) : strlen(text_ptr);

        if (line_len == 0)
        {
            current_y += EMPTY_LINE_HEIGHT;
        }
        else
        {
            const char* text_to_draw = text_ptr;
            int current_indent = 0;

            if (strncmp(text_ptr, "- ", 2) == 0)
            {
                playdate->graphics->drawText("-", 1, kUTF8Encoding, (int)margin, (int)current_y);
                current_indent = bullet_indent;
                text_to_draw += 2;
                line_len -= 2;
            }

            int line_height = playdate->graphics->getTextHeightForMaxWidth(
                font, text_to_draw, line_len, width - current_indent, kUTF8Encoding, kWrapWord,
                tracking, extraLeading
            );

            playdate->graphics->drawTextInRect(
                text_to_draw, line_len, kUTF8Encoding, margin + current_indent, (int)current_y,
                width - current_indent, line_height, kWrapWord, kAlignTextLeft
            );

            current_y += line_height;
        }

        if (next_newline)
        {
            text_ptr = next_newline + 1;
        }
        else
        {
            break;
        }
    }

    playdate->graphics->display();

    if (buttonsDown & (kButtonB | kButtonA))
    {
        if (infoScene->canClose)
        {
            PGB_dismiss(infoScene->scene);
        }
    }
}

static void PGB_InfoScene_free(void* object)
{
    PGB_InfoScene* infoScene = object;
    free(infoScene->text);
}

PGB_InfoScene* PGB_InfoScene_new(char* text)
{
    PGB_InfoScene* infoScene = pgb_malloc(sizeof(PGB_InfoScene));
    if (!infoScene) return NULL;
    memset(infoScene, 0, sizeof(*infoScene));
    playdate->system->getCrankChange();

    PGB_Scene* scene = PGB_Scene_new();
    infoScene->scene = scene;
    infoScene->text = text ? strdup(text) : NULL;
    infoScene->canClose = true;
    scene->managedObject = infoScene;
    scene->update = (void*)PGB_InfoScene_update;
    scene->free = (void*)PGB_InfoScene_free;

    return infoScene;
}
