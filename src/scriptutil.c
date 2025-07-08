#include "scriptutil.h"

#define GB script_gb

uint8_t __gb_read_full(struct gb_s* gb, const uint_fast16_t addr);
void __gb_write_full(struct gb_s* gb, const uint_fast16_t addr, uint8_t);

u8 rom_peek(romaddr_t addr) { return GB->gb_rom[addr]; }
void rom_poke(romaddr_t addr, u8 v) { GB->gb_rom[addr] = v; }

u8 ram_peek(addr16_t addr)
{
    return __gb_read_full(GB, addr);
}
void ram_poke(addr16_t addr, u8 v)
{
    __gb_write_full(GB, addr, v);
}

romaddr_t rom_size(void)
{
    return GB->gb_rom_size;
}

void poke_verify(unsigned bank, u16 addr, u8 prev, u8 val)
{
    u32 addr32 = bank*0x4000 | (addr % 0x4000);
    u8 actual = rom_peek(addr32);
    if (actual != prev)
    {
        playdate->system->error(
            "SCRIPT ERROR -- is this the right ROM? Poke_verify failed at %04x; expected %02x, but was %02x (should replace with %02x)",
            addr32, prev, actual, val
        );
    }
    
    rom_poke(addr32, val);
}

void find_code_cave(int bank, romaddr_t *max_start, romaddr_t *max_size) {
    uint32_t bank_start = (bank != -1) ? (bank * 0x4000) : 0;
    uint32_t bank_end = (bank != -1) ? (bank_start + 0x4000 - 1) : (rom_size() - 1);

    *max_start = 0;
    *max_size = 0;
    uint32_t current_start = 0;
    uint32_t current_size = 0;
    bool in_cave = false;

    for (uint32_t addr = bank_start; addr <= bank_end; addr++) {
        uint8_t byte = rom_peek(addr);

        if ((byte == 0x00 || byte == 0xFF) && (addr % 0x4000 != 0)) {
            if (!in_cave) {
                current_start = addr;
                current_size = 1;
                in_cave = true;
            } else {
                current_size++;
            }
        } else {
            if (in_cave) {
                if (current_size > *max_size) {
                    *max_size = current_size;
                    *max_start = current_start;
                }
                in_cave = false;
            }
        }
    }

    if (in_cave && current_size > *max_size) {
        *max_size = current_size;
        *max_start = current_start;
    }
}

CodeReplacement *code_replacement_new(
    unsigned bank, 
    uint16_t addr, 
    const uint8_t *tprev, 
    const uint8_t *tval, 
    size_t length, 
    bool unsafe
) {
    if (length == 0) {
        script_error("SCRIPT ERROR -- tprev and tval must have non-zero length");
        return NULL;
    }

    uint32_t base_addr = (bank << 14) | (addr & 0x3FFF);

    // Verify ROM matches tprev
    for (size_t i = 0; i < length; i++) {
        uint32_t current_addr = base_addr + i;
        uint8_t current_byte = rom_peek(current_addr);
        if (current_byte != tprev[i]) {
            script_error(
                "SCRIPT ERROR -- is this the right ROM? Patch verification failed at 0x%04X expected %02X got %02X (would replace with %02x)",
                current_addr, tprev[i], current_byte, tval[i]
            );
            return NULL;
        }
    }

    CodeReplacement *r = allocz(CodeReplacement);
    if (!r) {
        script_error("SCRIPT ERROR -- memory allocation failed");
        return NULL;
    }

    r->tprev = malloc(length);
    r->tval = malloc(length);
    if (!r->tprev || !r->tval) {
        free(r->tprev);
        free(r->tval);
        free(r);
        script_error("SCRIPT ERROR -- memory allocation failed");
        return NULL;
    }

    memcpy(r->tprev, tprev, length);
    memcpy(r->tval, tval, length);

    r->bank = bank;
    r->addr = base_addr;
    r->length = length;
    r->unsafe = unsafe;
    r->applied = false;

    return r;
}

void code_replacement_apply(CodeReplacement *r, bool apply) {
    if (!r) return;
    
    bool target_state = apply;
    
    if (r->applied == target_state) {
        return;
    }

    const uint8_t *target = target_state ? r->tval : r->tprev;

    if (!r->unsafe) {
        // ensure PC out of target range
        while ($PC >= r->addr && $PC < r->addr + r->length) {
            __gb_step_cpu(GB);
            return;
        }
    }
    
    for (size_t i = 0; i < r->length; i++) {
        rom_poke(r->addr + i, target[i]);
    }

    r->applied = target_state;
}

void code_replacement_free(CodeReplacement *r) {
    if (!r) return;
    free(r->tprev);
    free(r->tval);
    free(r);
}