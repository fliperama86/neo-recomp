# neo-recomp

Early scaffold for exploring static recompilation of Neo Geo 68000 program ROMs.

The first goal is deliberately small: load Neo Geo P-ROM bytes, discover a few candidate 68000 entry points, emit or call placeholder generated functions, and keep all platform behavior behind a runtime API.

This is not an emulator yet, and it is not a working recompiler yet.

For the current implementation status, latest real-ROM frontier, and next
TDD slices, see [`docs/progress.md`](docs/progress.md). For the concrete
68000 done/missing checklist, see [`docs/68k_correctness_tracker.md`](docs/68k_correctness_tracker.md).

## Shape

- `recompiler/` owns ROM loading, 68000 decoding, function discovery, and C emission.
- `runtime/` owns the Neo Geo bus/runtime boundary used by generated C plus
  small host-side video helpers.
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
.\build\Debug\neo-recomp.exe --game .\games\mslug.toml --neo G:\Mister\NEOGEO\mslug.neo --emit-c .\build\mslug_recomp.c
```

For now this loads the P-ROM, prints reset vectors, classifies vector target
regions, extracts the standard `NEO-GEO` cartridge header entry jump, and
recognizes the first PC-indexed entry dispatch-table pattern. Those dispatch
targets seed a bounded first-pass call graph that follows direct `JSR`, `BSR`,
and tail `JMP` targets when they point into the loaded P-ROM.
With `--emit-c`, the same discovered candidate list is written as a generated C
file with stable address-derived function symbols, a dispatch switch, and
statements for supported decoded instructions.

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
function candidates: 31
  [00] $000007CC (p_rom_fixed) entry
  [01] $0000080C (p_rom_fixed)
  [02] $00000832 (p_rom_fixed)
  [03] $00000836 (p_rom_fixed)
  [04] $00000840 (p_rom_fixed)
  [05] $00024E38 (p_rom_fixed)
  [06] $0000085E (p_rom_fixed)
  [07] $0000097A (p_rom_fixed)
  [08] $00000656 (p_rom_fixed)
  ...
function preview $00080C:
  $00080C: JSR $024E38              ; JSR
  $000812: MOVE.L $10FE80,D0        ; MOVE
  $000818: Bcc.6 $000826            ; BCC
  $00081C: CLR.W $100000            ; CLR
  $000822: BRA $00082E              ; BRA
  $000826: MOVE.W #$0,$100000       ; MOVE
generated C: .\build\mslug_recomp.c
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
- MiSTer `.neo` extraction is covered by a synthetic unit test: the P-region is
  normalized for 68000 execution, and the S/M/V1/V2/C regions are available for
  future video/audio host work.
- Real local smoke has been verified with `G:\Mister\NEOGEO\mslug.neo`.

Current headless Metal Slug smoke:

```sh
scripts/run_mslug_headless.sh \
  ~/Documents/Games/Mister/NEOGEO/mslug.neo \
  ~/Documents/Games/Mister/NEOGEO/bios/sp-s2.sp1
```

The BIOS is user-provided. The smoke is not playable; success currently means
reaching deterministic dispatch budgets without a dispatch/bus miss, with
growing runtime counters (including scanline-driven frame/VBlank progress) and
a non-looping final tail plus late VRAM writes. By default it checks
`10000 50000 100000 500000`; override with
`NG_MSLUG_PROGRESS_BUDGETS="..."` or run one budget with
`NG_MSLUG_DISPATCH_BUDGET=...`.

To save inspectable CPU-visible state from the final budget, set
`NG_MSLUG_SNAPSHOT_DIR=build/mslug_snapshot`. The script writes
`work_ram.bin` (64 KiB), `palette_ram.bin` (16 KiB), `vram_be.bin` (64K
big-endian VRAM words), and `summary.txt`.

Those raw dumps can be turned into dependency-free debug images:

```sh
tools/render_snapshot_debug.py build/mslug_snapshot
```

This writes PPMs under `build/mslug_snapshot/debug_images/`. They are visual
diagnostics, not accurate Neo Geo rendering yet.

The first real-rendering seam is a deterministic snapshot renderer:

```sh
build/neo-render-snapshot build/mslug_snapshot \
  ~/Documents/Games/Mister/NEOGEO/mslug.neo \
  build/mslug_snapshot/fix_layer.ppm
```

It loads the user-provided `.neo` image plus snapshot VRAM/palette RAM and
renders the CPU-visible 40x32 fix tile map by default.

The same tool can render a diagnostic sprite-map atlas from slow VRAM entries
and the cartridge C region:

```sh
build/neo-render-snapshot --mode sprite-atlas \
  --palette-bank auto --sprite-base-word 0x1000 --sprite-cols 32 --sprite-rows 16 \
  build/mslug_snapshot ~/Documents/Games/Mister/NEOGEO/mslug.neo \
  build/mslug_snapshot/sprite_atlas.ppm
```

That atlas proves C-region sprite tile decode and sprite-map/palette plumbing,
but it is not a positioned/layered game frame.
If a snapshot palette is all white/black, add `--debug-palette` to false-color
the sprite pixel indices.

For a first-pass real game-frame image, render positioned sprites plus the
visible fix layer:

```sh
build/neo-render-snapshot --mode frame --palette-bank auto \
  build/mslug_snapshot ~/Documents/Games/Mister/NEOGEO/mslug.neo \
  build/mslug_snapshot/frame.ppm
```

This is still an offline snapshot renderer, not a live emulator loop. Sprite
shrink and line-buffer priority are not exact yet, but the C-region swizzle,
sprite positioning, palette lookup, and fix overlay are isolated and covered by
tests.

If SDL2 is available through `pkg-config`, CMake also builds an optional
interactive snapshot host:

```sh
cmake --build build --target neo-snapshot-viewer
build/neo-snapshot-viewer build/mslug_snapshot
```

Keys `1`-`4` switch between raw VRAM, nonzero VRAM, work RAM, and palette
views; `r` reloads the snapshot. This is a host/viewer scaffold, not the final
live emulator loop.

## Decoder Slice

The first decoder milestone is intentionally tiny: decode enough of the Metal
Slug cartridge entry to prove the normalized P-ROM image is usable.

Covered so far:

- `LEA`
- `MOVE`
- `MOVEQ`
- `ADD`
- `CLR`
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

## Function Discovery Slice

The first discovery pass is deliberately conservative:

- seed the queue from the cartridge header entry
- scan a bounded number of instructions per candidate
- recognize the tested PC-indexed jump-table shape
- enqueue the first four mapped, unique table targets
- follow mapped direct `JSR`, `BSR`, and tail `JMP` targets
- ignore unmapped and duplicate targets

This is enough to turn the Metal Slug entry dispatch into 31 candidate
addresses. It is still not proof of exact function boundaries because unknown
opcodes are scanned as words until better decode coverage exists.

## C Emission Slice

The current emitter produces C from the discovered candidates. It emits a
dispatch switch plus decoded statements for the supported instruction subset:

```c
void ng_generated_call(uint32_t addr) {
    switch (addr & 0x00FFFFFFu) {
    case 0x000007CCu: ng_func_0007CC(); return;
    case 0x0000080Cu: ng_func_00080C(); return;
    default: ng_log_dispatch_miss(addr); return;
    }
}
```

It also emits local C labels for decoded `BRA` and Z-flag `BNE` / `BEQ` paths.
That is enough for the Metal Slug `$00080C` candidate to compile through the
first conditional branch instead of stopping at `$000818`:

```c
static void ng_func_00080C(void) {
    /* $00080C: JSR $024E38 */
    ng_generated_call(0x00024E38u);
    /* $000812: MOVE.L $10FE80,D0 */
    g_ng_m68k.d[0] = ng68k_read32(0x0010FE80u);
    ng_set_nz32(g_ng_m68k.d[0]);
    /* $000818: Bcc.6 $000826 */
    if ((g_ng_m68k.sr & NG_CCR_Z) == 0) goto ng_label_000826;
    /* $00081C: CLR.W $100000 */
    ng68k_write16(0x00100000u, 0x0000u);
    ng_set_nz16(0);
    /* $000822: BRA $00082E */
    goto ng_label_00082E;
ng_label_000826:
    /* $000826: MOVE.W #$0,$100000 */
    ng68k_write16(0x00100000u, 0x0000u);
    ng_set_nz16(0x0000u);
ng_label_00082E:
    /* $00082E: JMP $00085E */
    ng_generated_call(0x0000085Eu);
    return;
}
```

For the Metal Slug entry, the generated body now starts like this:

```c
static void ng_func_0007CC(void) {
    /* $0007CC: LEA $0008F4,A0 */
    g_ng_m68k.a[0] = 0x000008F4u;
    /* $0007D0: MOVE.L A0,$106EA8 */
    ng68k_write32(0x00106EA8u, g_ng_m68k.a[0]);
    /* $0007D6: BCLR #7,$10FD80 */
    ng68k_write8(0x0010FD80u, (uint8_t)(ng68k_read8(0x0010FD80u) & (uint8_t)~0x80u));
}
```

Unsupported instructions stop the generated function with
`ng_log_dispatch_miss`. This keeps unsupported behavior visible while allowing
instruction-level C emission to grow one decoded operation at a time with tests
against small register and memory fixtures.

Condition-code support is still intentionally partial. The emitter updates
`N`/`Z` for the operations covered by tests, and it updates `C` for tested
`CMPI.B` paths. Branch emission currently trusts `BNE`/`BEQ` and carry-based
`BCC`/`BCS` only in those covered cases. `V` and the rest of the 68k branch
condition table still need explicit tests before they are trusted.

Unknown opcodes are still printed as `DC.W`; they are not executable support.

## Validation Slice

The first generated-code sanity check is now executable:

- a build-time helper creates a synthetic 68k ROM fixture
- `ng_emit_c` generates C from that fixture into the build directory
- CMake compiles the generated C into `test_generated_exec`
- the test provides a fake Neo Geo runtime bus and `g_ng_m68k`
- a tiny interpreter runs the same original 68k bytes as an oracle
- the test runs `ng_generated_call(0)` and compares registers and bus contents
  against the interpreter result

This is a stronger check than compiling generated C back to 68k and comparing
machine code. A C compiler will not reproduce the original binary instruction
stream; the useful invariant is behavior. The current behavior check covers
`MOVEQ`, `MOVE.B/W #imm,Dn`, `ADD.W`, direct `JSR`, tail `JMP`,
`MOVE.W #imm,abs`, `MOVE.B #imm,abs`, `MOVE.B Dn,abs`,
`MOVE.B (d16,An),Dn`, `CMPI.B #imm,Dn`, `ANDI.B #imm,(d16,An)`,
`SUBQ.B #imm,Dn`, `CLR.B/W/L abs`, `CLR.B Dn`, `TST.B abs`,
`TST.B (d16,An)`, `LEA`, and `MOVE.L An,abs` through the same generated
dispatch shape used by real ROMs. It also covers not-taken and taken
`BNE`/`BEQ`/`BCC` paths, comparing generated C behavior against the tiny
interpreter oracle.
