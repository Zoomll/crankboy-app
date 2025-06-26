require "common"

-- enable xram [0xFEA0-FEFF]
pgb.ram_poke(0xFF57, 2)


-- ram addr
-- y speed - d078
-- input - ff8b

local crank_angle = nil;
local crank_delta = 0
local crank_delta_smooth = 0
local crank_hyst = nil

local CRANK_DELTA_SMOOTH_FACTOR = 0.9
local MIN_RATE_CRANK_BEGIN_FLAP = 0.5
local MIN_RATE_CRANK_FLAP = 0.3
local MIN_HYST_CRANK_BEGIN_FLAP = 9
local CRANK_MAX_HYST = 10

-- no pausing
poke_verify(0, 0x22C, 0xCB, 0xAF)
poke_verify(0, 0x22D, 0x5F, 0xAF)

-- Configuration mode with down+'B'
poke_verify(6, 0x4083, 0x86, 0x82)

-- Extra game mode with up+'A'
poke_verify(6, 0x4088, 0x45, 0x41)

-- can start game with 'A'
poke_verify(6, 0x4096, 0xE6, 0xFE)
poke_verify(6, 0x4097, 0x08, 0x01)
poke_verify(6, 0x4098, 0x28, 0x20)

local fly_thrust = nil;
local fly_max_speed = nil;

local cave_1_addr, cave_1_size = find_code_cave(1);

-- margins
cave_1_addr = cave_1_addr + 4
cave_1_size = cave_1_size - 8

XRAM_crank_dock_prev = 0xFEA0
XRAM_crank_prev_lo = 0xFEA1
XRAM_crank_prev_hi = 0xFEA2
XRAM_crank_delta_lo = 0xFEA3
XRAM_crank_delta_hi = 0xFEA4
XRAM_crank_delta_smooth_lo = 0xFEA5
XRAM_crank_delta_smooth_hi = 0xFEA6
XRAM_thrust = 0xFEA7

local patch = {
"read_crank",
    -- compare and store docked
    OP_PUSH_HL, OP_PUSH_BC,
        OP_LD_HL_d16, word(XRAM_crank_dock_prev),
        OP_PUSH_AF,
            OP_LD_A_a16, word(IO_PD_CRANK_DOCKED),
            OP_AND_xHL,
            OP_RRCA,
            OP_JR_c, r8"docked",

            -- circle difference between crank angle and prev
            -- HL <- IO_PD_CRANK
            OP_LD_A_a16, word(IO_PD_CRANK_lo),
            OP_LD_L_A,
            OP_LD_A_a16, word(IO_PD_CRANK_hi),
            OP_LD_H_A,

            -- BC <- FFFF - XRAM_crank_prev
            OP_LD_A_a16, word(XRAM_crank_prev_lo),
            OP_XOR_d8, 0xFF,
            OP_LD_C_A,
            OP_LD_A_a16, word(XRAM_crank_prev_hi),
            OP_XOR_d8, 0xFF,
            OP_LD_B_A,

            -- store previous crank
            OP_LD_A_H,
            OP_LD_a16_A, word(XRAM_crank_prev_hi),
            OP_LD_A_L,
            OP_LD_a16_A, word(XRAM_crank_prev_lo),

            -- HL <- IO_PD_CRANK - XRAM_crank_prev
            OP_ADD_HL_BC,
            OP_INC_HL,

            -- TODO: hysteresis

            -- write delta and smoothed delta
            OP_LD_A_L,
            OP_LD_a16_A, word(XRAM_crank_delta_lo),

            OP_LD_A_H,
            OP_LD_a16_A, word(XRAM_crank_delta_hi),

            -- HL <- XRAM_crank_delta_smooth
            OP_LD_HL_d16, word(XRAM_crank_delta_smooth_lo),
            OP_LD_A_iHL,
            OP_LD_H_xHL,
            OP_LD_L_A,

            -- BC <- HL
            OP_LD_B_H,
            OP_LD_C_L,

            -- hl <- hl/8
            OP_SRA_H,
            OP_RR_L,
            OP_SRA_H,
            OP_RR_L,
            OP_SRA_H,
            OP_RR_L,
        "bp5",

            -- hl <- bc - hl
            OP_LD_A_C,
            OP_SUB_L,
            OP_LD_L_A,
            OP_LD_A_B,
            OP_SBC_H,
            OP_LD_H_A,
        "bp6",

            -- BC <- XRAM_crank_delta
            OP_LD_A_a16, word(XRAM_crank_delta_lo),
            OP_LD_B_A,
            OP_LD_A_a16, word(XRAM_crank_delta_hi),
            OP_LD_C_A,

            -- BC /= 8
            OP_SRA_B,
            OP_RR_C,
            OP_SRA_B,
            OP_RR_C,
            OP_SRA_B,
            OP_RR_C,

            -- HL += BC
            OP_ADD_HL_BC,

        "bp7",
            OP_LD_A_L,
            OP_LD_a16_A, word(XRAM_crank_delta_smooth_lo),
            OP_LD_A_H,
            OP_LD_a16_A, word(XRAM_crank_delta_smooth_hi),
            OP_JR, r8"write_prev",

        "docked",
            -- crank delta <- 0
            -- crank delta smooth <- 0
            OP_XOR_A,
            OP_LD_a16_A, word(XRAM_crank_delta_lo),
            OP_LD_a16_A, word(XRAM_crank_delta_smooth_lo),
            OP_LD_a16_A, word(XRAM_crank_delta_hi),
            OP_LD_a16_A, word(XRAM_crank_delta_smooth_hi),

    "write_prev",
        OP_POP_AF,
        OP_LD_a16_A, word(XRAM_crank_dock_prev),

    OP_POP_BC, OP_POP_HL,

    -- input 'up' if cranking
    -- TODO: min. hysteresis
    OP_LD_A_a16, word(XRAM_crank_delta_smooth_hi),
"bp3",
    OP_OR_A,
    OP_JR_z, r8("check_lo"),
    OP_CP_d8, 0x80,
"bp4",
    OP_JR_ge, r8("org"),
"check_lo",
    OP_LD_A_a16, word(XRAM_crank_delta_smooth_lo),
    OP_CP_d8, math.floor(MIN_RATE_CRANK_BEGIN_FLAP / 360.0 * 0x10000),
"bp",
    OP_JR_ge, r8"input_up",

    -- original code
"org",
    0xF0, 0x8B,
    OP_LD_B_A,
    OP_RET,

"input_up",
    0xF0, 0x8B,
    OP_OR_d8, PAD_UP,
    OP_LD_B_A,
    OP_RET,

"door_up_check",
    -- A <- input
    0xF0, 0x8B,
    OP_BIT6_A,
    OP_JR_nz, r8"door_up_check.org",
    OP_RET, -- ignore door

"door_up_check.org",
    -- original
    OP_LD_B_A,
    0x2A, 0xB8,
    OP_RET,

"continue_flying",
    OP_LD_A_a16, word(XRAM_crank_delta_smooth_hi),
    OP_OR_A,
    OP_JR_z, r8"_check_lo",
    OP_CP_d8, 0x80,
    OP_JR_ge, r8"continue_flying.org",

"cf_math",
    OP_PUSH_HL,
        OP_LD_HL_d16, word(XRAM_crank_delta_smooth_lo),

        OP_LD_A_iHL,
        OP_LD_H_xHL,
        OP_LD_L_A,

        -- skip if HL < 0
        OP_BIT7_H,
        OP_JR_nz, r8("pophl_continue_flying.org"),

        -- hl <- hl/32
        OP_SRA_H,
        OP_RR_L,
        OP_SRA_H,
        OP_RR_L,
        OP_SRA_H,
        OP_RR_L,
        OP_SRA_H,
        OP_RR_L,
        OP_SRA_H,
        OP_RR_L,

        OP_LD_A_H,
        OP_OR_A,
        OP_JR_z, r8("ld_a_l"),

        OP_LD_A_d8, 0xFF,
        OP_LD_L_A,
    "ld_a_l",
        OP_LD_A_L,

        -- A in range [0, 0x80)
        OP_SRL_A,
        OP_SUB_d8, 0x20,
        OP_JR_lt, r8"flap_negative",
        OP_JR, r8"set_thrust",

    "flap_negative",
        OP_LD_A_d8, 0x20,
        OP_LD_a16_A, word(XRAM_thrust),
        OP_JR, r8"pophl_continue_flying.org",

    "set_thrust",
        OP_LD_a16_A, word(XRAM_thrust),

    OP_POP_HL,
    -- fallthrough

"input_up_swap",
    0xF0, 0x8B,
    OP_OR_d8, PAD_UP,
    OP_SWAP_A,
    OP_RET,

"_check_lo",
    OP_LD_A_a16, word(XRAM_crank_delta_smooth_lo),
    OP_CP_d8, math.floor(MIN_RATE_CRANK_FLAP / 360.0 * 0x10000),
    OP_JR_ge, r8"cf_math",
    OP_LD_A_d8, -- skip next instr

"pophl_continue_flying.org",
    OP_POP_HL,

"continue_flying.org",
    0xF0, 0x8B,
    OP_SWAP_A,
    OP_RET,
}

patch_res = apply_patch(patch, cave_1_addr, 0x4000 | (cave_1_addr % 0x4000), cave_1_size)

-- only go through door if holding 'up'
poke_verify(0, 0x49A, OP_LD_B_A, OP_CALL)
poke_verify(0, 0x49B, 0x2A, patch_res.labels.door_up_check & 0xFF)
poke_verify(0, 0x49C, 0xB8, patch_res.labels.door_up_check >> 8)

-- continue flying if cranking
poke_verify(1, 0x467C, 0xF0, OP_CALL)
poke_verify(1, 0x467D, 0x8B, patch_res.labels.continue_flying & 0xFF)
poke_verify(1, 0x467E, 0xCB, patch_res.labels.continue_flying >> 8)
poke_verify(1, 0x467F, 0x37, OP_NOP)

-- custom crank reading
poke_verify(1, 0x4492, 0xF0, OP_CALL)
poke_verify(1, 0x4493, 0x8B, patch_res.labels.read_crank & 0xFF)
poke_verify(1, 0x4494, 0x47, patch_res.labels.read_crank >> 8)

-- can also start the game with 'start'
pgb.rom_set_breakpoint(
    (6*0x4000) | (0x4096 % 0x4000),
    function(n)
        if pgb.regs.a == 0x08 then
            pgb.regs.a = 1
        end
    end
)

-- force immediate unpause
pgb.rom_set_breakpoint(
    (6*0x4000) | (0x460E % 0x4000),
    function(n)
        pgb.regs.a = 0x8
    end
)

if true then
    return
end

-- set accel directly (tval[2] is accel value)
patch_fly_accel_down = code_replacement(
    0, 0x3C5, {0xFA, 0x7E, 0xD0}, {0x3E, 0, 0x00}
)

-- set accel directly (tval[2] is accel value)
patch_fly_accel_up = code_replacement(
    0, 0x3F8, {0xFA, 0x7E, 0xD0}, {0x3E, 0, 0x00}
)

-- continue flying via crank
pgb.rom_set_breakpoint(
    (1*0x4000) | (0x467E % 0x4000),
    function(n)
        -- TODO: if holding up/down, ignore crank

        if crank_delta_smooth > MIN_RATE_CRANK_FLAP then
            local rate = math.max(0, math.min(crank_delta_smooth, 30)) / 30
            fly_thrust = math.floor(-0x20 + 0x70 * rate);
            fly_max_speed = -0x200*rate
            local current_speed = (pgb.ram_peek(0xD078) << 8) | pgb.ram_peek(0xD079)
            if current_speed >= 0x8000 then
                current_speed = current_speed - 0x10000
            end
            if current_speed < fly_max_speed then
                fly_thrust = -0x20
            end
            --print(current_speed, fly_max_speed, fly_thrust, rate)
            if fly_thrust == 0 then
                patch_fly_accel_up.tval[2] = 0
                patch_fly_accel_up.applied = false
                patch_fly_accel_up:apply()
                patch_fly_accel_down.tval[2] = 0
                patch_fly_accel_down.applied = false
                patch_fly_accel_down:apply()
            elseif fly_thrust >= 0 then
                fly_thrust = math.floor((fly_thrust / 0x50) * (fly_thrust / 0x50) * 0x50)
                pgb.regs.a = pgb.regs.a | 0x40

                patch_fly_accel_up.tval[2] = math.max(fly_thrust, 0)
                patch_fly_accel_up.applied = false
                patch_fly_accel_up:apply()
            else
                patch_fly_accel_down.tval[2] = math.max(-fly_thrust, 0)
                patch_fly_accel_down.applied = false
                patch_fly_accel_down:apply()
            end
        else
            fly_thrust = nil;
        end
    end
)

-- start flying via crank
pgb.rom_set_breakpoint(
    (1*0x4000) | (0x4494 % 0x4000),
    function(n)
        if crank_angle and crank_hyst then
            if circle_difference(crank_hyst, crank_angle) >= MIN_HYST_CRANK_BEGIN_FLAP then
                if crank_delta > MIN_RATE_CRANK_BEGIN_FLAP then
                    pgb.regs.a = pgb.regs.a | 0x40
                end
            end
        end
    end
)

function circle_difference(a, b)
    return ((b - a + 180) % 360) - 180
end

function pgb.update()
    local new_crank_angle = pgb.get_crank()
    if new_crank_angle and crank_angle then
        crank_delta = circle_difference(crank_angle, new_crank_angle)
        if not crank_hyst then
            crank_hyst = new_crank_angle
        else
            local cd = circle_difference(crank_hyst, new_crank_angle)
            if cd > CRANK_MAX_HYST then
                crank_hyst = (new_crank_angle - CRANK_MAX_HYST) % 360
            elseif cd < -CRANK_MAX_HYST then
                crank_hyst = (new_crank_angle + CRANK_MAX_HYST) % 360
            end
        end

        crank_delta_smooth = crank_delta_smooth * CRANK_DELTA_SMOOTH_FACTOR
            + (1 - CRANK_DELTA_SMOOTH_FACTOR)* crank_delta
    else
        crank_delta = 0
        crank_hyst = new_crank_angle
    end

    crank_angle = new_crank_angle

    patch_fly_accel_down:apply(false)
    patch_fly_accel_up:apply(false)
end

print("Hello from Lua!")
