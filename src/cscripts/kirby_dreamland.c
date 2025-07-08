#include "scriptutil.h"

#define DESCRIPTION "(Experimental)\n\n- Use the crank to flap!\n" \
        "- Start/Select buttons are no longer required anywhere."

#define CRANK_DELTA_SMOOTH_FACTOR 0.9f
#define MIN_RATE_CRANK_BEGIN_FLAP 0.5f
#define MIN_RATE_CRANK_FLAP 0.3f
#define MIN_HYST_CRANK_BEGIN_FLAP 9.0f
#define CRANK_MAX_HYST 10.0f

// custom data for script.
typedef struct ScriptData
{
    float crank_angle;
    float crank_delta;
    float crank_delta_smooth;
    float crank_hyst;
    
    CodeReplacement* patch_no_door;
    CodeReplacement* patch_start_flying;
    CodeReplacement* patch_continue_flying;
    CodeReplacement* patch_fly_accel_down;
    CodeReplacement* patch_fly_accel_up;
} ScriptData;

// this define is used by SCRIPT_BREAKPOINT
#define USERDATA ScriptData* data

#define ROM_US_EU
#define ROM_JP

static float circle_difference(float a, float b)
{
    float diff = b - a;
    diff = fmodf(diff + 180.0f, 360.0f);
    if (diff < 0.0f) {
        diff += 360.0f;
    }
    return diff - 180.0f;
}

// can also start the game with 'start'
SCRIPT_BREAKPOINT(
    BANK_ADDR(6, 0x4096)
) {
    if ($A == 0x8) {
        $A = 1;
    }
}

// force immediate unpause
SCRIPT_BREAKPOINT(
    BANK_ADDR(6, 0x460E)
) {
    $A = 0x8;
}

// Start flying via crank
SCRIPT_BREAKPOINT(
    BANK_ADDR(1, 0x4494)
) {
    if (data->crank_angle >= 0 && data->crank_hyst >= 0)
    {
        if (circle_difference(data->crank_hyst, data->crank_angle) >= MIN_HYST_CRANK_BEGIN_FLAP)
        {
            if (data->crank_delta > MIN_RATE_CRANK_BEGIN_FLAP)
            {
                $A |= 0x40;
            }
        }
    }
}
        
static ScriptData* on_begin(struct gb_s* gb, char* header_name)
{
    printf("Hello from C!\n");
    
    ScriptData* data = allocz(ScriptData);
    
    // enable xram
    ram_poke(IO_PD_FEATURE_SET, 2);
    
    // we're replacing the crank functionality entirely
    force_pref(crank_mode, CRANK_MODE_OFF);
    force_pref(crank_dock_button, PREF_BUTTON_NONE);
    force_pref(crank_undock_button, PREF_BUTTON_NONE);
    
    // no pausing
    poke_verify(0, 0x22C, 0xCB, 0xAF);
    poke_verify(0, 0x22D, 0x5F, 0xAF);

    // Configuration mode with down+'B'
    poke_verify(6, 0x4083, 0x86, 0x82);

    // Extra game mode with up+'A'
    poke_verify(6, 0x4088, 0x45, 0x41);

    // can start game with 'A'
    poke_verify(6, 0x4096, 0xE6, 0xFE);
    poke_verify(6, 0x4097, 0x08, 0x01);
    poke_verify(6, 0x4098, 0x28, 0x20);
    
    unsigned cave_1_addr, cave_1_size;
    find_code_cave(1, &cave_1_addr, &cave_1_size);
    
    if (cave_1_size < 40)
    {
        script_error("Failed to find bank 1 code cave.");
        return NULL;
    }

    // margins
    cave_1_addr += 4;
    cave_1_size -= 8;
    
    #define PLACEHOLDER 0x00
    
    data->patch_no_door = code_replacement(
        0, 0x04C5, (0x28, 0x06), (0x00, 0x00), true
    );
    
    data->patch_start_flying = code_replacement(
        1, 0x4498, (0x2A, 0x45), (0x9A, 0x44), true
    );
    
    data->patch_continue_flying = code_replacement(
        1, 0x467C, (0xF0, 0x8B), (0x3E, K_BUTTON_UP), true
    );
    
    data->patch_fly_accel_down = code_replacement(
        0, 0x3C5, (0xFA, 0x7E, 0xD0), (0x3E, PLACEHOLDER, 0x00), true
    );
    
    data->patch_fly_accel_up = code_replacement(
        0, 0x3F8, (0xFA, 0x7E, 0xD0), (0x3E, PLACEHOLDER, 0x00), true
    );
    
    SET_BREAKPOINTS(
        !!strcmp(header_name, "KIRBY DREAM LAND")
    );
    
    return data;
}

static void on_end(struct gb_s* gb, ScriptData* data)
{
    code_replacement_free(data->patch_no_door);
    code_replacement_free(data->patch_start_flying);
    code_replacement_free(data->patch_continue_flying);
    code_replacement_free(data->patch_fly_accel_down);
    code_replacement_free(data->patch_fly_accel_up);
    
    free(data);
}

static void on_tick(struct gb_s* gb, ScriptData* data)
{
    bool start_flying_via_crank = false;
    bool continue_flying = false;
    
    float new_crank_angle = playdate->system->getCrankAngle();
    if (playdate->system->isCrankDocked()) new_crank_angle = -1;
    
    if (new_crank_angle >= 0 && data->crank_angle >= 0)
    {
        data->crank_delta = circle_difference(data->crank_angle, new_crank_angle);
        if (data->crank_hyst < 0)
            data->crank_hyst = new_crank_angle;
        else {
            float cd = circle_difference(data->crank_hyst, new_crank_angle);
            if (cd > CRANK_MAX_HYST)
                data->crank_hyst = nnfmodf(new_crank_angle - CRANK_MAX_HYST, 360.0f);
            else if (cd < -CRANK_MAX_HYST)
                data->crank_hyst = nnfmodf(new_crank_angle + CRANK_MAX_HYST, 360.0f);
        }
        
        data->crank_delta_smooth = data->crank_delta_smooth * CRANK_DELTA_SMOOTH_FACTOR
            + (1 - CRANK_DELTA_SMOOTH_FACTOR)* data->crank_delta;
    }
    else
    {
        data->crank_delta = 0;
        data->crank_hyst = new_crank_angle;
    }
    
    // crank to flap
    int fly_thrust;
    bool has_fly_thrust = false;
    if (($JOYPAD & (K_BUTTON_UP | K_BUTTON_DOWN)) == 0) {
        if (data->crank_angle >= 0 && data->crank_hyst >= 0) {
            if (circle_difference(data->crank_hyst, data->crank_angle) + data->crank_delta >= MIN_HYST_CRANK_BEGIN_FLAP) {
                if (data->crank_delta > MIN_RATE_CRANK_BEGIN_FLAP) {
                    start_flying_via_crank = true;
                }
            }
        }
        
        int fly_max_speed;
        if (data->crank_delta_smooth > MIN_RATE_CRANK_FLAP) {
            float rate = MAX(0, MIN(data->crank_delta_smooth, 30.0f)) / 30.0f;
            fly_thrust = -0x20 + 0x70 * rate;
            has_fly_thrust = true;
            fly_max_speed = -0x200*rate;
            int current_speed = (ram_peek(0xD078) << 8) | ram_peek(0xD079);
            if (current_speed >= 0x8000) {
                current_speed = current_speed - 0x10000;
            }
            if (current_speed < fly_max_speed) {
                fly_thrust = -0x20;
                has_fly_thrust = true;
            }
            printf("%d %d %d rate=%f\n", current_speed, fly_max_speed, fly_thrust, (double)rate);
            if (has_fly_thrust) {
                // TODO
                fly_thrust = (fly_thrust / 0x50) * (fly_thrust / 0x50) * 0x50;
                continue_flying = true;
            }
        } else {
            has_fly_thrust = false;
        }
        
        if (has_fly_thrust) {
            // TODO
        }
    }
    
    code_replacement_apply(data->patch_start_flying, start_flying_via_crank);
    code_replacement_apply(data->patch_no_door, start_flying_via_crank);
    
    if (continue_flying) {
        u8 buttons = K_BUTTON_UP | $JOYPAD;
        if (buttons != data->patch_continue_flying->tval[2]) {
            data->patch_continue_flying->applied = false;
            data->patch_continue_flying->tval[2] = buttons;
        }
        code_replacement_apply(data->patch_continue_flying, true);
    }
    else
    {
        code_replacement_apply(data->patch_continue_flying, false);
    }
    
    if (has_fly_thrust) {
        data->patch_fly_accel_down->tval[2] = MAX(-fly_thrust, 0);
        data->patch_fly_accel_down->applied = false;
        data->patch_fly_accel_up->tval[2] = MAX(fly_thrust, 0);
        data->patch_fly_accel_up->applied = false;
        code_replacement_apply(data->patch_fly_accel_down, true);
        code_replacement_apply(data->patch_fly_accel_up, true);
    } else {
        code_replacement_apply(data->patch_fly_accel_down, false);
        code_replacement_apply(data->patch_fly_accel_up, false);
    }
    
    data->crank_angle = new_crank_angle;
}
    
C_SCRIPT
{
    .rom_name = "KIRBY DREAM LAND",
    .description = DESCRIPTION,
    .experimental = true,
    .on_begin = (CS_OnBegin)on_begin,
    .on_tick = (CS_OnTick)on_tick,
    .on_end = (CS_OnEnd)on_end,
};

C_SCRIPT
{
    .rom_name = "HOSHI NO KIRBY",
    .description = DESCRIPTION,
    .experimental = true,
    .on_begin = (CS_OnBegin)on_begin,
    .on_tick = (CS_OnTick)on_tick,
    .on_end = (CS_OnEnd)on_end,
};