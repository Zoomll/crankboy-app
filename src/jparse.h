#pragma once

#include <stdint.h>

#include "pd_api.h"

typedef struct TableKeyPair
{
    char *key;
    json_value value;
} TableKeyPair;

typedef struct JsonObject
{
    size_t n;
    TableKeyPair data[];
} JsonObject;

typedef struct JsonArray
{
    size_t n;
    json_value data[];
} JsonArray;

void free_json_data(json_value);

// returns 0 on failure
// opts should be kFileRead, kFileReadData, or kFileRead | kFileReadData
int parse_json(const char *file, json_value *out, FileOptions opts);

// returns 0 on success
int write_json_to_disk(const char* path, json_value out);

json_value json_get_table_value(json_value table, const char* key);