#include "modal.h"
#include "scene.h"
#include "app.h"
#include "utility.h"

#define MODAL_ANIM_TIME 16

void PGB_Modal_update(PGB_Modal* modal)
{
    if (modal->exit)
    {
        if (--modal->timer == 0)
        {
            PGB_dismiss(modal->scene);
        }
    }   
    else
    {
        if (++modal->timer > MODAL_ANIM_TIME)
            modal->timer = MODAL_ANIM_TIME;
    }
    PDButtons pushed;
    playdate->system->getButtonState(NULL, &pushed, NULL);
    
    uint8_t* lcd = playdate->graphics->getFrame();
    memcpy(lcd, modal->lcd, sizeof(modal->lcd));
    
    // fuzzy grey
    uint32_t lfsr = 0;
    int tap2 = 5 + modal->exit;
    for (size_t y = 0; y < LCD_ROWS; ++y)
    {
        for (size_t x = 0; x < LCD_COLUMNS; ++x)
        {
            if ((x % 2) == (y % 2))
            {
                lfsr <<= 1;
                lfsr |= 1 & ((lfsr >> 1) ^ (lfsr >> tap2) ^ (lfsr >> 8) ^ (lfsr >> 31) ^ 1);
                if (lfsr % MODAL_ANIM_TIME < modal->timer)
                {
                    int c = (y % 2 == 0);
                    lcd[y*LCD_ROWSIZE + (x/8)] &= ~(1 << (x%8));
                    lcd[y*LCD_ROWSIZE + (x/8)] |= (c << (x%8));
                }
            }
        }
    }
    playdate->graphics->markUpdatedRows(0, LCD_ROWS-1);
    
    int w = 250;
    int x = (LCD_COLUMNS - w) / 2;
    int h = 120;
    int y = -h + (((LCD_ROWS - h) / 2 + h)*modal->timer)/MODAL_ANIM_TIME;
    int thick = 3;
    
    playdate->graphics->fillRect(
        x, y, w, h, kColorBlack
    );
    
    playdate->graphics->fillRect(
        x+thick, y+thick, w-2*thick, h-2*thick, kColorWhite
    );
    
    int m = 24; // margin
    playdate->graphics->setFont(PGB_App->bodyFont);
    if (modal->text)
    {
        playdate->graphics->drawTextInRect(
            modal->text, strlen(modal->text), kASCIIEncoding,
            x+m, y+m, w-2*m, h-2*m, kWrapWord, kAlignTextCenter
        );
    }
    
    int spacing = w/(1 + modal->options_count);
    
    for (int i = 0; i < modal->options_count; ++i)
    {
        int ox = x + spacing*(i+1);
        int oy = y + h - m;
        int option_height = 20;
        
        for (int k = 0; k <= 1; ++k)
        {
            playdate->graphics->drawLine(
                ox - spacing/3, oy - k*option_height,
                ox + spacing/3, oy - k*option_height,
                3, kColorBlack
            );
        }
        
        playdate->graphics->drawTextInRect(
            modal->options[i],
            strlen(modal->options[i]),
            kASCIIEncoding,
            ox - spacing/2, oy - option_height,
            spacing, option_height,
            kWrapClip, kAlignTextCenter
        );
    }
    
    if (modal->exit || modal->timer < MODAL_ANIM_TIME) return;
    if ((pushed & kButtonB) || (modal->options_count == 0 && (pushed & kButtonA)))
    {
        modal->timer = MODAL_ANIM_TIME;
        modal->exit = 1;
        modal->result = -1;
    }
    else if (pushed & kButtonA)
    {
        modal->timer = MODAL_ANIM_TIME;
        modal->exit = 1;
        modal->result = modal->option_selected;
    }
    else {
        int d = !!(pushed & kButtonRight) - !!(pushed & kButtonLeft);
        modal->option_selected += d;
        if (modal->option_selected >= modal->options_count) modal->option_selected = modal->options_count - 1;
        if (modal->option_selected < 0) modal->option_selected = 0;
    }
}

void PGB_Modal_free(PGB_Modal* modal)
{
    if (modal->callback) modal->callback(modal->ud, modal->result);
    
    for (size_t i = 0; i < MODAL_MAX_OPTIONS; ++i)
    {
        if (modal->options[i]) free(modal->options[i]);
    }
    if (modal->text) free(modal->text);
    PGB_Scene_free(modal->scene);
    pgb_free(modal);
}

PGB_Modal *PGB_Modal_new(char* text, char const* const* options, PGB_ModalCallback callback, void* ud)
{
    PGB_Modal* modal = malloc(sizeof(PGB_Modal));
    memset(modal, 0, sizeof(*modal));
    
    modal->options_count = 0;
    if (options) for (size_t i = 0; options[i] && i < MODAL_MAX_OPTIONS; ++i)
    {
        modal->options[i] = strdup(options[i]);
        modal->options_count++;
    }
    
    if (text) modal->text = strdup(text);
        
    PGB_Scene *scene = PGB_Scene_new();
    modal->scene = scene;
    scene->managedObject = modal;
    scene->update = (void*)PGB_Modal_update;
    scene->free = (void*)PGB_Modal_free;
    
    modal->callback = callback;
    modal->ud = ud;
    
    // copy in what's currently on the screen
    
    uint8_t* src = playdate->graphics->getFrame();
    memcpy(modal->lcd, src, sizeof(modal->lcd));
    
    return modal;
}