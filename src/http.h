#pragma once

#define USE_SSL true

#define HTTP_ENABLE_DENIED 1
#define HTTP_ENABLE_ASKED 2
#define HTTP_ENABLE_IN_PROGRESS 4
#define HTTP_ERROR 8
#define HTTP_MEM_ERROR 16

typedef void (*enable_cb_t)(unsigned flags, void* ud);

// attempts to enable HTTP, then 
void enable_http(
    const char* domain,
    const char* reason,
    enable_cb_t cb,
    void* ud
);