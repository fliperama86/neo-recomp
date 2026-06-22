# Architecture

`neo-recomp` should keep generated 68000 C as platform-neutral as possible.

Generated functions include only `ngrecomp/generated_abi.h` and should know
only about:

- `NgM68kState`
- `ng68k_read8/read16/read32`
- `ng68k_write8/write16/write32`
- `ng_call_by_address`
- runtime hooks for interrupts, traps, and dispatch misses

Generated dispatch should stay trampoline/tail-call oriented: generated
control-flow handoffs enqueue the next 68k PC and unwind the host C stack before
running the next generated routine. Host integrations that combine multiple
generated modules, such as cart plus BIOS, should preserve that same shape at
the module boundary.

The concrete Neo Geo runtime includes that ABI and supplies one implementation.
Tests may supply a fake implementation, and future host integrations can do the
same without changing generated C or the recompiler.

Neo Geo-specific behavior belongs in the runtime:

- P-ROM and bank mapping
- work RAM and backup RAM
- video registers, VRAM, palette RAM
- controller and DIP inputs
- sound latch and Z80 communication
- BIOS calls or BIOS interpreter fallback
- protection and cartridge-specific behavior

Renderer-facing helpers should stay separate from generated-code execution.
`neo_video` is the current home for deterministic, host-side decode/render
pieces that can be tested without running the CPU: palette conversion, tile
decoders, and snapshot-to-pixel helpers. Live SDL/native hosts should compose
the generated CPU module, the runtime bus state, and these video helpers rather
than baking rendering knowledge into emitted 68000 C.

Audio follows the same boundary. `neo_audio` owns the Z80/M1-side sound bus,
command/reply latches, M-ROM banking, YM2610 synthesis, and rendered sample
generation. Its M-ROM bank view intentionally follows MAME's Neo Geo audio
layout, including the 128 KiB M1 `ROM_RELOAD` effective region used by Metal
Slug. YM2610 V-ROM reads likewise treat the `.neo` `v1`/`v2` chunks as one
contiguous sample address space for Metal Slug's MAME `ymsnd:adpcma` map, with
ADPCM-B falling back to that same region when no explicit B region exists. The
YM wrapper uses MAME's YM2610 stream fidelity/routes and averages native chip
samples when resampling to the host output rate rather than letting the live
host sample a single native tick. The 68000 runtime
exposes only CPU-visible sound latch behavior plus a narrow command/reply event
boundary for hosts; generated 68000 code should not know about Z80 or YM2610
internals.

The first useful milestone is function-level parity against an interpreter for selected 68000 routines, not full game boot.
