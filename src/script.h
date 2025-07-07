#pragma once

struct PGB_GameScene;

typedef struct LuaScriptInfo
{
    char* script_path;
    char* info;
    char rom_name[17];
    bool experimental;
} LuaScriptInfo;

typedef void (*CS_OnBegin)(struct PGB_GameScene* game_scene);
typedef void (*CS_OnTick)(struct PGB_GameScene* game_scene);

struct CScriptInfo
{
    const char* rom_name;
    bool experimental;
    CS_OnBegin on_begin;
    CS_OnTick on_tick;
};

#define C_SCRIPT \
    static struct CSCriptInfo _script; \
    static __atribute__((constructor)) void _init_script() \
        { register_script(&_script); } \
    struct CScriptInfo _script

struct lua_State;
typedef struct ScriptState
{
    // one of the following will be non-null
    
    struct CScriptInfo* c;
    lua_State* L;
} ScriptState;

lua_State* script_begin(const char* game_name, struct PGB_GameScene* game_scene);
void script_end(lua_State* L);
void script_tick(lua_State* L);
void script_on_breakpoint(lua_State*, int index);

void register_script(const struct CScriptInfo*);

// script info
void script_info_free(LuaScriptInfo* info);
LuaScriptInfo* script_get_info_by_rom_path(const char* game_path);
bool script_exists(const char* game_path);