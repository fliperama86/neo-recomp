# Architecture

`neo-recomp` should keep generated 68000 C as platform-neutral as possible.

Generated functions should only know about:

- `NgM68kState`
- `ng68k_read8/read16/read32`
- `ng68k_write8/write16/write32`
- `ng_call_by_address`
- runtime hooks for interrupts, traps, and dispatch misses

Neo Geo-specific behavior belongs in the runtime:

- P-ROM and bank mapping
- work RAM and backup RAM
- video registers, VRAM, palette RAM
- controller and DIP inputs
- sound latch and Z80 communication
- BIOS calls or BIOS interpreter fallback
- protection and cartridge-specific behavior

The first useful milestone is function-level parity against an interpreter for selected 68000 routines, not full game boot.

