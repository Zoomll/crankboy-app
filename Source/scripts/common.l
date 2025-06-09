printf = function(s,...)
    return print(s:format(...))
end

function poke_verify(bank, addr, prev, val)
    addr = bank*0x4000 | (addr % 0x4000)
    if pgb.rom_peek(addr) ~= prev then
        error("SCRIPT ERROR -- is this the right ROM? Poke_verify failed at " .. string.format("0x%04X", addr) .. " expected " .. string.format("0x%02X", prev) .. " got " .. string.format("0x%02X", pgb.rom_peek(addr)))
    end
    
    pgb.rom_poke(addr, val)
end

function code_replacement(bank, addr, tprev, tval, unsafe)
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