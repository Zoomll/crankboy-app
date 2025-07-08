require "common"

pgb.setCrankSoundsDisabled()
pgb.force_pref("crank_mode", 5)
pgb.force_pref("crank_undock_button", 0)
pgb.force_pref("crank_dock_button", 0)

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

XRAM_IN_MENU_DOCK_TO_EXIT = 0xFEA0
XRAM_TMP_0 = 0xFEA1
XRAM_TMP_1 = 0xFEA2
XRAM_TMP_2 = 0xFEA3
XRAM_BLOCK_MENU = 0xFEA4
XRAM_TRAINER = 0xFEA5

-- 2: enable xram [0xFEA0-FEFF]
-- 4: enable crank menu mode
pgb.ram_poke(0xFF57, 0x06)

if bint(POKERED) + bint(POKEBLUE) + bint(POKEYELLOW) ~= 1 then
    error("Unrecognized gen1 ROM")
    return 1
else
    print("Gen1 version: ", POKEVER)
end

local cave_rom_start, cave_size = find_code_cave(1)
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
    R = {
        Bankswitch = 0x0035d6,
        CheckPauseMenu = 0x00044F-8,
        PauseMenuInput = 0x002af2,
        hJoyHeld = 0xffb4,
        hJoyPressed = 0xffb3,
        hJoy5 = 0xffb5,
        hJoy7 = 0xffb7,
        hJoyInput = 0xfff8,
        wCurrentMenuItem = 0xcc26,
        wLastMenuItem = 0xcc2a,
        wJoyIgnore = 0xcd6b,
        wStatusFlags5 = 0xd730,
        WrapMenuItemID = 0x2B10,
        CheckIfPastBottom = 0x002B29,
        HandleMenuInput = 0x3abe,
        HandleMenuInput_GetJoypadState = 0x3ae8+1,
        PauseMenuInputCheckDown = 0x002b18,
        JoypadLowSensitivity = 0x003831,
        Joypad = 0x00019A,
        DrawStartMenu = 0x01710B,
        MenuExitDraw = 0x01717A,
        SkipDrawBorder = 0x0170BD,
    },
    B = {
        Bankswitch = 0x0035d6,
        CheckPauseMenu = 0x00044F-8,
        PauseMenuInput = 0x002af2,
        hJoyHeld = 0xffb4,
        hJoyPressed = 0xffb3,
        hJoy5 = 0xffb5,
        hJoy7 = 0xffb7,
        hJoyInput = 0xfff8,
        wStatusFlags5 = 0xd730,
        wCurrentMenuItem = 0xcc26,
        wLastMenuItem = 0xcc2a,
        wJoyIgnore = 0xcd6b,
        WrapMenuItemID = 0x2B10,
        CheckIfPastBottom = 0x002B29,
        HandleMenuInput = 0x3abe,
        HandleMenuInput_GetJoypadState = 0x3ae8+1,
        PauseMenuInputCheckDown = 0x002b18,
        JoypadLowSensitivity = 0x003831,
        Joypad = 0x00019A,
        DrawStartMenu = 0x01710B,
        MenuExitDraw = 0x01717A,
        SkipDrawBorder = 0x0170BD,
    },
    Y = {
        Bankswitch = 0x003e84,
        CheckPauseMenu = 0x000290-8,
        PauseMenuInput = 0x0029f4,
        hJoyHeld = 0xffb4,
        hJoyPressed = 0xffb3,
        hJoy5 = 0xffb5,
        hJoy7 = 0xffb7,
        hJoyInput = 0xfff5,
        wStatusFlags5 = 0xd72f,
        wCurrentMenuItem = 0xcc26,
        wLastMenuItem = 0xcc2a,
        wJoyIgnore = 0xcd6b,
        WrapMenuItemID = 0x2A12,
        CheckIfPastBottom = 0x002A2B,
        HandleMenuInput = 0x3aab,
        HandleMenuInput_GetJoypadState = 0x3ad5+1,
        PauseMenuInputCheckDown = 0x002a1a,
        JoypadLowSensitivity = 0x00381e,
        Joypad = 0x0001B9,
        DrawStartMenu = 0x016F80,
        MenuExitDraw = 0x016FED,
        SkipDrawBorder = 0x016F33,
    }
})[POKEVER]

local function sym_to_rom(sym)
    return (0x4000 * (sym >> 16)) | (sym % 0x4000)
end

local function sym_to_ram(sym)
    return ((0x4000 * (sym >> 16) == 0) and 0 or 0x4000) | (sym % 0x4000)
end

local function farcall(dstbank, dstaddr)
    return {
        OP_LD_B_d8, dstbank,
        OP_LD_HL_d16, word(dstaddr),
        OP_CALL, word(addr.Bankswitch)
    }
end

local trainer = {
"cavecall",
    -- store HL
    OP_LD_A_H,
    OP_LD_a16_A, word(XRAM_TMP_0),
    OP_LD_A_L,
    OP_LD_a16_A, word(XRAM_TMP_1),

    -- HL <- word at return address
    -- return address += 2
    OP_POP_HL,
    OP_LD_A_iHL,
    OP_INC_HL,
    OP_PUSH_HL,
    OP_DEC_HL,
    OP_LD_H_xHL,
    OP_LD_L_A,

    -- B <- dst bank
    OP_LD_B_d8, cave_bank,
    OP_CALL, word(addr.Bankswitch),

    -- A <- H
    OP_LD_A_H,

    OP_PUSH_AF,
        -- restore HL
        OP_LD_a16_A, word(XRAM_TMP_0),
        OP_LD_H_A,
        OP_LD_a16_A, word(XRAM_TMP_1),
        OP_LD_L_A,
    OP_POP_AF,
    OP_RET,
}
trainer_res = apply_patch(trainer, XRAM_TRAINER, XRAM_TRAINER, 0xFF00 - XRAM_TRAINER)

local patch = {
"CrankUndockPauseMenu",
    -- if we're in scripted movement, load [hJoyHeld] instead
    OP_LD_A_a16, word(addr.wStatusFlags5),
    OP_RLCA,
    OP_JR_nc, r8"CrankUndockPauseMenu_notSimulating",

    -- mark disabled menu input, lingers for 1 frame
    OP_XOR_A,
    OP_LD_a16_A, word(XRAM_BLOCK_MENU),

    OP_LD_A_a16, word(addr.hJoyHeld),
    OP_JR, r8"CrankUndockPauseMenu_ret",

"CrankUndockPauseMenu_notSimulating",

    -- if joypad input is ignored, load [hJoyHeld] instead
    OP_AND_d8, 0x40,
    OP_JR_nz, r8"CrankUndockPauseMenu_normal",

    -- if pause button is ignored, load [hJoyHeld] instead
    OP_LD_A_a16, word(addr.wJoyIgnore),
    OP_AND_d8, 0x8,
    OP_JR_nz, r8"CrankUndockPauseMenu_normal",

    OP_LD_A_a16, word(XRAM_BLOCK_MENU),
    OP_PUSH_AF,
        -- enable menu next frame
        OP_LD_A_d8, 1,
        OP_LD_a16_A, word(XRAM_BLOCK_MENU),
    OP_POP_AF,
    OP_AND_A,
    OP_JR_z, r8"CrankUndockPauseMenu_normal_noblock",

    -- check crank docked
    OP_LD_A_a16, word(IO_PD_CRANK_DOCKED),

    OP_RRCA,
    OP_JR_c, r8"CrankUndockPauseMenu_CrankDocked",

    -- undocked; input 'start'
    OP_LD_A_d8, 8,
    -- also, clear crank menu delta register
    OP_LD_a16_A, word(IO_PD_CRANK_hi),
    OP_LD_a16_A, word(IO_PD_CRANK_lo),
    OP_JR, r8"CrankUndockPauseMenu_ret",

"CrankUndockPauseMenu_CrankDocked",
    OP_LD_A_a16, word(addr.hJoyPressed),
    OP_AND_d8, (~8) & 0xFF, -- not start button

"CrankUndockPauseMenu_ret",
    OP_LD_H_A,
    OP_BIT3_A,
    OP_RET,

"CrankUndockPauseMenu_normal",
    -- mark disabled menu input, lingers for 1 frame
    OP_XOR_A,
    OP_LD_a16_A, word(XRAM_BLOCK_MENU),

"CrankUndockPauseMenu_normal_noblock",
    OP_LD_A_a16, word(addr.hJoyHeld),
    OP_JR, r8"CrankUndockPauseMenu_ret",

"CustomPauseMenuInput",
    OP_LD_A_d8, 1,
    OP_LD_a16_A, word(XRAM_IN_MENU_DOCK_TO_EXIT),
    OP_CALL, word(addr.HandleMenuInput),

    OP_PUSH_AF,
        OP_XOR_A,
        OP_LD_a16_A, word(XRAM_IN_MENU_DOCK_TO_EXIT),
    OP_POP_AF,

"CustomPauseMenuInput_Done",
    OP_LD_H_A,
    OP_BIT6_A,
    OP_JR_nz, r8"CustomPauseMenuInput_Done_Up",
    OP_SCF, OP_CCF, -- clear carry flag (indicates not pressing 'up')
    OP_RET,

-- had to steal some of this for detour
"CustomPauseMenuInput_Done_Up",
    OP_LD_A_a16, word(addr.wCurrentMenuItem),
    OP_LD_B_A,
    OP_LD_A_a16, word(addr.wLastMenuItem),
    OP_OR_B,
    OP_SCF,
    OP_RET,

"CustomMenuInput",
    OP_LD_A_a16, word(addr.hJoyInput),

    OP_PUSH_AF,
        OP_LD_A_a16, word(XRAM_IN_MENU_DOCK_TO_EXIT),
        OP_LD_B_A,
        OP_LD_A_a16, word(IO_PD_CRANK_DOCKED),
        OP_LD_C_A,
        OP_LD_A_a16, word(IO_PD_CRANK_lo),
        OP_LD_H_A,
        OP_LD_a16_A, word(IO_PD_CRANK_lo),
    OP_POP_AF,
    OP_RRC_B,
    OP_JR_nc, r8"menuDoctorInput",

"startMenuDoctorInput",
        -- suppress 'start' and 'B'
        OP_AND_d8, 0xFF & (~0xA),

        -- if crank docked, input 'B' to exit menu
        OP_RRC_C,

        OP_JR_nc, r8"menuDoctorInput",
        OP_OR_d8, 2,

"menuDoctorInput",

    OP_BIT0_H,
    OP_CALL_nz, a16"inputVerticalFromCrank",

    -- restore HL
    OP_PUSH_AF,
        OP_LD_A_a16, word(XRAM_TMP_0),
        OP_LD_H_A,
        OP_LD_A_a16, word(XRAM_TMP_1),
        OP_LD_L_A,
    OP_POP_AF,

    OP_LD_a16_A, word(addr.hJoyInput),
    OP_CALL, word(addr.JoypadLowSensitivity),
    OP_LD_A_a16, word(addr.hJoy5),
    OP_RET,

"inputVerticalFromCrank",
    OP_BIT7_H,
    OP_JR_nz, r8"inputUpFromCrank",

"inputDownFromCrank",
    OP_OR_d8, 0x80,
    OP_RET,

"inputUpFromCrank",
    OP_OR_d8, 0x40,
    OP_RET,
}

patch_res = apply_patch(patch, cave_rom_start, cave_start, cave_size, trainer_res.labels)

for label, addr in pairs(patch_res.labels) do
    printf("  %s: %x", label, addr)
end

-- detour to new check for pause menu
apply_patch(
    {
        farcall(cave_bank, patch_res.labels.CrankUndockPauseMenu),
        OP_LD_A_H,
        OP_NOP,
    },
    addr.CheckPauseMenu
)

-- detour to new pause menu input routine
apply_patch(
    {
        farcall(cave_bank, patch_res.labels.CustomPauseMenuInput),
        OP_LD_A_H,
        OP_LD_B_A,
        OP_JR_nc, r8(addr.PauseMenuInputCheckDown),
        OP_NOP, OP_NOP, OP_NOP,
        OP_NOP, OP_NOP, OP_NOP,
    },
    addr.PauseMenuInput
)

-- detour to new pause menu input routine
apply_patch(
    {
        OP_CALL, word(trainer_res.labels.cavecall),
        word(patch_res.labels.CustomMenuInput),
    },
    addr.HandleMenuInput_GetJoypadState
)

-- don't add "Exit" to menu
apply_patch(
    {
        -- number of options minus 1
        0x5
    },
    sym_to_rom(addr.WrapMenuItemID - 4), sym_to_ram(addr.WrapMenuItemID - 4)
)
apply_patch(
    {
        -- number of options
        0x6
    },
    sym_to_rom(addr.CheckIfPastBottom - 4), sym_to_ram(addr.CheckIfPastBottom - 4)
)
apply_patch(
    {
        -- window size
        0xC
    },
    sym_to_rom(addr.SkipDrawBorder - 14), sym_to_ram(addr.MenuExitDraw - 14)
)
apply_patch(
    {
        -- window size
        0xA
    },
    sym_to_rom(addr.SkipDrawBorder - 5), sym_to_ram(addr.MenuExitDraw - 5)
)
apply_patch(
    {
        -- window size
        0xC
    },
    sym_to_rom(addr.DrawStartMenu + 9), sym_to_ram(addr.MenuExitDraw + 9)
)
apply_patch(
    {
        -- window size (no pokédex)
        0xA
    },
    sym_to_rom(addr.DrawStartMenu + 18), sym_to_ram(addr.MenuExitDraw + 18)
)
apply_patch(
    {
        -- add menu item 'Exit'
        OP_NOP, OP_NOP, OP_NOP,
        OP_NOP, OP_NOP, OP_NOP,
    },
    sym_to_rom(addr.MenuExitDraw + 9), sym_to_ram(addr.MenuExitDraw + 9)
)
