if true then
    return
end

-- enable xram [0xFEA0-FEFF]
pgb.ram_poke(0xFF57, 2)

printf = function(s,...)
    return print(s:format(...))
end

local function poke_verify(bank, addr, prev, val)
    addr = bank*0x4000 | (addr % 0x4000)
    if pgb.rom_peek(addr) ~= prev then
        error("SCRIPT ERROR -- is this the right ROM? Poke_verify failed at " .. string.format("0x%04X", addr) .. " expected " .. string.format("0x%02X", prev) .. " got " .. string.format("0x%02X", pgb.rom_peek(addr)))
    end
    
    pgb.rom_poke(addr, val)
end

local function code_replacement(bank, addr, tprev, tval, unsafe)
    -- verify that the lengths match
    if #tprev ~= #tval then
        error("SCRIPT ERROR -- tprev and tval must have the same length")
    end
    
    -- verify tprev matches what's in ROM
    local base_addr = bank * 0x4000 | (addr % 0x4000)
    for i = 1, #tprev do
        local current_addr = base_addr + i - 1
        local current_byte = pgb.rom_peek(current_addr)
        if current_byte ~= tprev[i] then
            error(string.format("SCRIPT ERROR -- is this the right ROM? Poke_verify failed at 0x%04X expected 0x%02X got 0x%02X", 
                current_addr, tprev[i], current_byte))
        end
    end
    
    local replacement = {
        bank = bank,
        addr = base_addr,
        unsafe = unsafe,
        tprev = tprev,
        tval = tval,
        length = #tprev,
        applied = false,
    }
    
    function replacement:apply(yes)
        if apply == null then
            apply = true
        end
        
        if self.applied == apply then
            return
        end
    
        local target = (yes == false)
            and self.tprev
            or self.tval
        
        if not self.unsafe then
            -- wait until PC is outside the replacement area
            while pgb.regs.pc >= self.addr and pgb.regs.pc < self.addr + self.length do
                pgb.step_cpu()
            end
        end
        
        -- Apply the changes
        for i = 1, self.length do
            pgb.rom_poke(self.addr + i - 1, target[i])
        end
    end
    
    return replacement
end

function find_code_cave(bank)
    local bank_start = bank * 0x4000
    local bank_end = bank_start + 0x4000 - 1
    local max_start = 0
    local max_size = 0
    local current_start = nil
    local current_size = 0
    
    for addr = bank_start, bank_end do
        local byte = pgb.rom_peek(addr)
        
        -- Check if byte is part of a code cave (0x00 or 0xFF)
        if byte == 0x00 or byte == 0xFF then
            if current_start == nil then
                -- Start new code cave
                current_start = addr
                current_size = 1
            else
                -- Continue existing code cave
                current_size = current_size + 1
            end
        else
            if current_start ~= nil then
                -- Code cave ended, check if it's the largest found
                if current_size > max_size then
                    max_size = current_size
                    max_start = current_start
                end
                current_start = nil
                current_size = 0
            end
        end
    end
    
    -- Check if the last bytes in the bank were part of a code cave
    if current_start ~= nil and current_size > max_size then
        max_size = current_size
        max_start = current_start
    end
    
    return max_start, max_size
end

PAD_A = 1
PAD_B = 2
PAD_SELECT = 4
PAD_START = 8
PAD_RIGHT = 0x10
PAD_LEFT = 0x20
PAD_UP = 0x40
PAD_DOWN = 0x80

OP_NOP = 0x00
OP_ADD_HL_BC = 0x09
OP_RRCA = 0x0F
OP_JR = 0x18
OP_JR_nz = 0x20
OP_LD_HL_d16 = 0x21
OP_INC_HL = 0x23
OP_JR_z = 0x28
OP_JR_nc = 0x30
OP_JR_ge = 0x30
OP_JR_c = 0x38
OP_JR_lt = 0x38
OP_LD_A_d8 = 0x3E
OP_LD_B_A = 0x47
OP_LD_C_A = 0x4F
OP_LD_L_A = 0x6F
OP_LD_H_A = 0x67
OP_LD_A_H = 0x7C
OP_LD_A_L = 0x7D
OP_AND_xHL = 0xA6
OP_XOR_A = 0xAF
OP_POP_BC = 0xC1
OP_PUSH_BC = 0xC5
OP_OR_A = 0xB7
OP_CP_xHL = 0xBE
OP_RET = 0xC9
OP_CALL = 0xCD
OP_POP_HL = 0xE1
OP_PUSH_HL = 0xE5
OP_LD_a16_A = 0xEA
OP_XOR_d8 = 0xEE
OP_POP_AF = 0xF1
OP_PUSH_AF = 0xF5
OP_OR_d8 = 0xF6
OP_LD_A_a16 = 0xFA
OP_CP_d8 = 0xFE

OP_BIT0_A = {0xCB, 0x47}
OP_BIT6_A = {0xCB, 0x77}
OP_BIT7_H = {0xCB, 0x7C}

IO_PD_FEATURE_SET = 0xFF57
IO_PD_CRANK_DOCKED = 0xFF57
IO_PD_CRANK_lo = 0xFF58
IO_PD_CRANK_hi = 0xFF59

function word(x)
    return {x & 0xFF, x >> 8}
end

function r8(x)
    return {r8=x}
end

function a16(x)
    return {a16=x}
end

function apply_patch(patch, rom_addr, ram_addr, max_size, _labels)
    if not ram_addr then
        if rom_addr < 0x4000 then
            ram_addr = rom_addr
        else
            ram_addr = 0x4000 | (rom_addr % 0x4000)
        end
    end
    
    local labels = _labels or {}
    local data = {}
    
    local r8 = {}
    local a16 = {}
    
    -- TODO: ensure no holes in table
    
    for _, op in ipairs(patch) do
        if type(op) == "number" then
            data[#data + 1] = op
        elseif type(op) == "table" then
            if op.r8 then
                r8[#data] = op.r8
                data[#data + 1] = 0 -- placeholder
            elseif op.a16 then
                a16[#data] = op.a16
                data[#data + 1] = 0 -- placeholder
                data[#data + 1] = 0 -- placeholder
            else
                for _2, op2 in ipairs(op) do
                    data[#data + 1] = op2
                end 
            end
        elseif type(op) == "string" then
            labels[op] = ram_addr + #data
        end
    end
    
    for offset, label in pairs(r8) do
        dst = labels[label]
        if not dst then
            error("SCRIPT ERROR -- label \"" .. label .. "\" not found")
            return nil
        end
        local val = dst - (offset + ram_addr + 1)
        if val < -0x80 or val >= 0x80 then
            error("SCRIPT ERROR -- label \"" .. label .. "\" out of range for relative jump")
            return nil
        end
        if val < 0 then
            val = val + 0x100
        end
        data[offset + 1] = val
    end
    
    for offset, label in pairs(a16) do
        dst = labels[label]
        if not dst then
            error("SCRIPT ERROR -- label \"" .. label .. "\" not found")
            return nil
        end
        data[offset + 1] = dst & 0xFF
        data[offset + 2] = dst >> 8
    end
    
    -- write to rom
    for i, b in ipairs(data) do
        if b < 0 or b >= 0x100 then
            error("SCRIPT ERROR -- value doesn't fit in byte: " .. tostring(b))
        end
        pgb.rom_poke(rom_addr + i - 1, b)
    end
    
    return {
        labels=labels
    }
end

-- ram addr
-- y speed - d078
-- input - ff8b

local crank_angle = null;
local crank_delta = 0
local crank_delta_smooth = 0
local crank_hyst = null

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

local fly_thrust = null;
local fly_max_speed = null;

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
            
            OP_LD_A_L,
            OP_LD_a16_A, word(XRAM_crank_delta_lo),
            OP_LD_a16_A, word(XRAM_crank_delta_smooth_lo),
            
            -- TODO: smoothed crank delta
            OP_LD_A_H,
            OP_LD_a16_A, word(XRAM_crank_delta_hi),
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
    OP_CP_d8, math.floor(MIN_RATE_CRANK_FLAP / 360.0 * 0x10000),
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
    --OP_RET,

"door_up_check.org",
    -- original
    OP_LD_B_A,
    0x2A, 0xB8,
    OP_RET,
}

patch_res = apply_patch(patch, cave_1_addr, 0x4000 | (cave_1_addr % 0x4000), cave_1_size)

-- only go through door if holding 'up'
--[[
poke_verify(0, 0x49A, OP_LD_B_A, OP_CALL)
poke_verify(0, 0x49B, 0x2A, patch_res.labels.door_up_check & 0xFF)
poke_verify(0, 0x49C, 0xB8, patch_res.labels.door_up_check >> 8)
--]]

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
            fly_thrust = null;
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