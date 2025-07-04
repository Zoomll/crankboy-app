#pragma once

#include "pd_api.h"

#include <stdio.h>

#define USE_SSL true

#define HTTP_ENABLE_DENIED 1
#define HTTP_ENABLE_ASKED 2
#define HTTP_ENABLE_IN_PROGRESS 4
#define HTTP_ERROR 8
#define HTTP_MEM_ERROR 16
#define HTTP_TIMEOUT 32
#define HTTP_NON_SUCCESS_STATUS 64
#define HTTP_UNEXPECTED_CONTENT_TYPE 128

typedef void (*enable_cb_t)(unsigned flags, void* ud);

// attempts to enable HTTP, then invokes the given callback
void enable_http(const char* domain, const char* reason, enable_cb_t cb, void* ud);

typedef void (*http_result_cb)(unsigned flags, char* data, size_t data_len, void* ud);

// performs an HTTP request, then invokes the callback.
// automatically does enable_http as part of this.
// returns the connection handle, or NULL on immediate failure.
HTTPConnection* http_get(
    const char* domain, const char* path, const char* reason, http_result_cb cb, int timeout_ms,
    void* ud
);
