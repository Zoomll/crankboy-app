# Gameboy IO Extensions for Playdate

To allow gameboy games access to Playdate features such as the crank and accelerometer,
CrankBoy includes some custom I/O registers for this purpose. They must first be enabled before use (`FF57`)

## `FF57` [w] - Features Enable

Different bits enable/disable different features:

```
bit 0 - accelerometer
bit 1 - xram
bits 2-7 - reserved (assume 0 as safe default)
```

If bit 0 is set, accelerometer is enabled; otherwise it is disabled.

If bit 1 is set, RAM range 0xFEA0-0xFEFF becomes a read-writeable 0x60 byte region. (Normally this region is unused.)

## `FF57` [r] - Crank Docked

When reading from `FF57`, some flags are returned:

```
bit 0: crank docked [1 if docked]
bits 1-7: reserved
```

## `FF58` and `FF59` - Crank

16-bit word, contains a value in the range 0-FFFF, indicates crank rotation. FFFF is one pip less than 360°.

## `FF5A` and `FF5B` - Accelerometer (X axis)

16-bit word. 8000 = zero acceleration; A000 = accelerating to the left at *g*; 6000 = accelerating to the right at *g*. Limited to the range [-4g, 4g).

## `FF5C` and `FF5D` - Accelerometer (Y axis)

16-bit word. 8000 = zero acceleration; A000 = accelerating upward at *g* (as would be the case if the user is holding the playdate vertically); 6000 = accelerating to the downward at *g* (as if the user is holding the playdate upside-down). Limited to the range [-4g, 4g).

## `FF5E` and `FF5F` - Accelerometer (Z axis)

16-bit word. 8000 = zero acceleration; A000 = accelerating outward from the screen at *g* (as would be the case if the playdate is lying on its back); 6000 = accelerating to the toward the screen at *g* (as if the playdate is lying on its front). Limited to the range [-4g, 4g).