require "common"

-- decide version based on name in header
if pgb.rom_peek(0x13C) == 0x52 then
    POKERED = true
    POKEVER = "R"
end
if pgb.rom_peek(0x13C) == 0x42 then
    POKEBLUE = true
    POKEVER = "B"
end
if pgb.rom_peek(0x13C) == 0x59 then
    POKEYELLOW = true
    POKEVER = "Y"
end

-- 1: enable xram [0xFEA0-FEFF]
-- 4: enable crank menu mode
pgb.ram_poke(0xFF57, 0x05)

if bint(POKERED) + bint(POKEBLUE) + bint(POKEYELLOW) ~= 1 then
    error("Unrecognized gen1 ROM")
    return 1
else
    print("Gen1 version: ", POKEVER)
end

local cave_rom_start, cave_size = find_code_cave()
local cave_bank = math.floor(cave_rom_start / 0x4000)
cave_start = ((cave_bank == 0) and 0 or 0x4000) | (cave_rom_start % 0x4000)

if (cave_size < 0x180) then
    error("unable to find sufficiently large code cave")
end

-- margins
cave_start = cave_start + 8
cave_rom_start = cave_rom_start + 8
cave_size = cave_size - 16

print("Special thanks to pret for disasm reference")
printf("Code cave: %x:%x +%x", cave_bank, cave_start, cave_size)
local addr = ({
    R= {
        Bankswitch = 0x0035d6,
        CheckPauseMenu = 0x00044F-8,
        hJoyHeld = 0xffb4,
        hJoyPressed = 0xffb3,
    },
    B= {
        Bankswitch = 0x0035d6,
        CheckPauseMenu = 0x00044F-8,
        hJoyHeld = 0xffb4,
        hJoyPressed = 0xffb3,
    },
    Y= {
        Bankswitch = 0x003e84,
        CheckPauseMenu = 0x000290-8,
        hJoyHeld = 0xffb4,
        hJoyPressed = 0xffb3,
    }
})[POKEVER]

local function farcall(dstbank, dstaddr)
    return {
        OP_LD_B_d8, dstbank,
        OP_LD_HL_d16, word(dstaddr),
        OP_CALL, word(addr.Bankswitch)
    }
end

local patch = {
"CrankUndockPauseMenu",
    -- if we're in scripted movement, load [hJoyHeld] instead
    OP_RLCA,
    OP_JR_nc, r8"CrankUndockPauseMenu_notSimulating",
    
    OP_LD_A_a16, word(addr.hJoyHeld),
    OP_JR, r8"CrankUndockPauseMenu_ret",
    
"CrankUndockPauseMenu_notSimulating",
    -- check crank docked
    OP_LD_A_a16, word(IO_PD_CRANK_DOCKED),
    
    OP_RRCA,
    OP_JR_c, r8"CrankUndockPauseMenu_CrankDocked",
    
    -- undocked
    OP_LD_A_d8, 8,
    OP_JR, r8"CrankUndockPauseMenu_ret",
    
"CrankUndockPauseMenu_CrankDocked",
    OP_LD_A_a16, word(addr.hJoyPressed),
    OP_AND_d8, (~8) & 0xFF, -- not start button
    
"CrankUndockPauseMenu_ret",
    OP_LD_H_A,
    OP_BIT3_A,
    OP_RET,
}

patch_res = apply_patch(patch, cave_rom_start, cave_start, cave_size)

-- detour to new check for pause menu
apply_patch(
    {
        farcall(cave_bank, patch_res.labels.CrankUndockPauseMenu),
        OP_LD_A_H,
        OP_NOP,
    },
    addr.CheckPauseMenu
)