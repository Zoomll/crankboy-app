#pragma once

struct PGB_GameScene;

typedef struct ScriptInfo
{
    char* script_path;
    char* info;
    char rom_name[17];
    bool experimental;
} ScriptInfo;

lua_State* script_begin(const char* game_name, struct PGB_GameScene* game_scene);
void script_end(lua_State* L);
void script_tick(lua_State* L);
void script_on_breakpoint(lua_State*, int index);
void script_info_free(ScriptInfo* info);
ScriptInfo* script_get_info_by_rom_path(const char* game_path);
bool script_exists(const char* game_path);