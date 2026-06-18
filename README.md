# neo-recomp

Early scaffold for exploring static recompilation of Neo Geo 68000 program ROMs.

The first goal is deliberately small: load Neo Geo P-ROM bytes, discover a few candidate 68000 entry points, emit or call placeholder generated functions, and keep all platform behavior behind a runtime API.

This is not an emulator yet, and it is not a working recompiler yet.

## Shape

- `recompiler/` owns ROM loading, 68000 decoding, function discovery, and C emission.
- `runtime/` owns the Neo Geo bus/runtime boundary used by generated C.
- `include/ngrecomp/` contains shared public interfaces.
- `games/` contains per-game metadata.

## Build

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

## Current CLI

```powershell
.\build\Debug\neo-recomp.exe --game .\games\nam1975.toml --p1 path\to\p1.rom
```

For now this only loads the P-ROM and prints scaffold diagnostics.

## Development Style

This project is intentionally TDD-first. Each layer should get a small failing
test before implementation code is added.

Prefer pure, deterministic tests at the bottom of the stack:

- P-ROM loading, byte order, vector parsing, and address mapping.
- Game config parsing and validation.
- 68000 instruction decoding from raw opcode bytes.
- Function discovery from synthetic byte streams.
- Generated-code behavior against small register/RAM fixtures.
- Runtime bus behavior for RAM, palette, I/O, sound latch, and unmapped access.

Full game boot is not an early test target. The first useful milestone is a
correct 68000-visible program image.

## Next Milestone

M1: load a Neo Geo program ROM image and parse reset vectors correctly.

The first red test should use a synthetic P-ROM fixture:

```text
bytes 0x00..0x03 = 0x0010F300  initial SSP
bytes 0x04..0x07 = 0x00001234  initial PC
```

Expected behavior:

- vector 0 reads `0x0010F300`
- vector 1 reads `0x00001234`
- 16-bit and 32-bit reads are big-endian
- initial PC is checked against the mapped P-ROM range

After that passes, add tests one at a time for MAME-style P-ROM file layout,
word swapping if needed, and bank-window mapping.
