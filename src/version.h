#pragma once

// any negative code is considered an error
// 0: no update
// 1: update exists

typedef void (*update_result_cb)(
    int code, const char* text, void* ud
);

void check_for_updates(update_result_cb cb, void* ud);