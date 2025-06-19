#pragma once

#include "pd_api.h"
#include "scene.h"
#include "utility.h"

#define MODAL_MAX_OPTIONS 3

// pop-up boxes and such

struct PGB_Modal;

// option is -1 if cancelled;
// otherwise, option is index in options[]
typedef void (*PGB_ModalCallback)(void* ud, int option);

typedef struct PGB_Modal
{
    PGB_Scene* scene;
    void* ud;

    int cursorIndex;
    uint8_t lcd[LCD_ROWS * LCD_ROWSIZE];

    char* text;
    int options_count;
    int option_selected;
    char* options[MODAL_MAX_OPTIONS];
    PGB_ModalCallback callback;
    int timer;
    int droptimer;
    bool exit : 1;
    bool setup : 1;
    int result;

    LCDBitmap* dissolveMask;
} PGB_Modal;

PGB_Modal* PGB_Modal_new(
    char* text, char const* const* options, PGB_ModalCallback callback, void* ud
);
