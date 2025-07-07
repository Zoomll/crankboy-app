#pragma once

/*

There are two kinds of scripts. Lua scripts and C scripts.
Both are supported via script.c

Lua scripts must be listed in scripts.json

C scripts are .c files which must be included in the makefile
at build time, and they must contain a C_SCRIPT { ... } declaration.

*/

struct PGB_GameScene;
struct gb_s;

// returns user-data
typedef void* (*CS_OnBegin)(struct gb_s* gb);

typedef void (*CS_OnTick)(struct gb_s* gb, void* userdata);

// should free userdata
typedef void (*CS_OnEnd)(struct gb_s* gb, void* userdata);

typedef void (*CS_OnBreakpoint)(
    struct gb_s* gb,
    uint16_t addr,
    int breakpoint_idx,
    void* userdata
);

struct CScriptInfo
{
    const char* rom_name;
    const char* description;
    bool experimental;
    CS_OnBegin on_begin;
    CS_OnTick on_tick;
    CS_OnEnd on_end;
};

#define C_SCRIPT \
    static struct CSCriptInfo _script; \
    static __atribute__((constructor)) void _init_script() \
        { register_script(&_script); } \
    struct CScriptInfo _script

typedef struct ScriptInfo
{
    char rom_name[17];
    bool experimental;
    char* info; // human-readable description
    
    // one of the following will be non-null
    char* lua_script_path;
    const struct CScriptInfo* c_script_info;
} ScriptInfo;

struct lua_State;
typedef struct ScriptState
{
    // one of the following will be non-null
    const struct CScriptInfo* c;
    lua_State* L;
    
    // C script state
    void* ud;
    
    CS_OnBreakpoint* cbp;
} ScriptState;

ScriptState* script_begin(const char* game_name, struct PGB_GameScene* game_scene);
void script_end(ScriptState* state, struct PGB_GameScene* game_scene);
void script_tick(ScriptState* state, struct PGB_GameScene* game_scene);
void script_on_breakpoint(struct PGB_GameScene* game_scene, int index);

void register_c_script(const struct CScriptInfo*);

// for C scripts.
// Returns negative on failure; breakpoint index otherwise.
int c_script_add_hw_breakpoint(
    struct gb_s* gb,
    uint16_t addr,
    CS_OnBreakpoint callback
);

// script info
void script_info_free(ScriptInfo* info);
ScriptInfo* script_get_info_by_rom_path(const char* game_path);
bool script_exists(const char* game_path);