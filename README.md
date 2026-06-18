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

Windows with Visual Studio Build Tools:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

macOS or Linux with the default CMake generator:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

The codebase is intended to support Windows, macOS, and Linux. Keep platform
code behind small runtime interfaces, and prefer portable C11/CMake for core
recompiler code.

## Current CLI

```powershell
.\build\Debug\neo-recomp.exe --game .\games\nam1975.toml --p1 path\to\p1.rom
.\build\Debug\neo-recomp.exe --game .\games\mslug.toml --neo G:\Mister\NEOGEO\mslug.neo
```

For now this loads the P-ROM, prints reset vectors, classifies vector target
regions, extracts the standard `NEO-GEO` cartridge header entry jump, and
recognizes the first PC-indexed entry dispatch-table pattern.

Example real `.neo` smoke output:

```text
vector initial_ssp=$0010F300 (work_ram) initial_pc=$00C00402 (bios)
cartridge header: NEO-GEO, entry=$000007CC (p_rom_fixed)
entry preview:
  $0007CC: LEA $0008F4,A0           ; LEA
  $0007D0: MOVE.L A0,$106EA8        ; MOVE
  $0007D6: BCLR #7,$10FD80          ; BCLR
  ...
  $0007F6: MOVEA.L ($0007FC,PC,D0.W),A0 ; MOVEA
  $0007FA: JMP (A0)                 ; JMP
  dispatch table: base=$0007FC index=D0.W target=A0 entry_size=4
    [0] $0000080C (p_rom_fixed)
    [1] $00000832 (p_rom_fixed)
    [2] $00000836 (p_rom_fixed)
    [3] $00000840 (p_rom_fixed)
```

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

Current M1 status:

- Raw P-ROM vector parsing is covered by a synthetic unit test.
- MiSTer `.neo` P-region extraction and word normalization are covered by a
  synthetic unit test.
- Real local smoke has been verified with `G:\Mister\NEOGEO\mslug.neo`.

## Decoder Slice

The first decoder milestone is intentionally tiny: decode enough of the Metal
Slug cartridge entry to prove the normalized P-ROM image is usable.

Covered so far:

- `LEA`
- `MOVE`
- `MOVEQ`
- `ADD`
- `BCLR`
- `ANDI #imm,SR`
- `JSR`
- `JMP`
- `BRA` / `BSR` / `Bcc`
- `RTS`

The first analysis pass recognizes this narrow dispatch shape:
`MOVEA.L (d8,PC,Dn.W),An` followed by `JMP (An)`. It is currently only a
candidate jump-table discovery aid, not proof that a full function boundary has
been recovered.

Unknown opcodes are still printed as `DC.W`; they are not executable support.
