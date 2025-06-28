#pragma once

#include "http.h"

#define ERR_PERMISSION_ASKED_DENIED (-253)
#define ERR_PERMISSION_DENIED (-254)

// any negative code is considered an error
// 0: success (but no result)
// 1: success (and result)
typedef void (*update_result_cb)(
    int code, const char* text, void* ud
);

void check_for_updates(update_result_cb cb, void* ud);

// check for updates if it's been more than a certain amount of time since we last checked
void possibly_check_for_updates(update_result_cb cb, void* ud);

const char* get_current_version(void);