### `cb.rom_poke(addr, val)`
sets the value at the given rom address to the given value

### `cb.rom_peek(addr)`
returns the value at the given rom address

### `cb.get_crank()`
returns crank angle in degrees, or null if docked

### `cb.setCrankSoundsDisabled(bool)`
see `playdate->system->setCrankSoundsDisabled`
    
### `cb.setROMBreakpoint(addr, fn)`

Only call this function at script initialization time.

Inserts a "hardware" execution breakpoint at the given address. Returns the breakpoint index (or null if an error occurred).

`fn(n)` will be invoked every time the instruction at this address runs, where `n` is the breakpoint index.

This is implemented by setting a special opcode, normally considered invalid, to the given address. If this address is modified afterward (e.g. by `cb.rom_poke`), the breakpoint will no longer trigger.

A maximum of 128 breakpoints may be placed.

### `cb.rom_size()`

returns number of bytes in rom