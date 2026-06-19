# Progress Tracker

Last updated: 2026-06-19

This is the live working document for `neo-recomp`. Keep the README focused on
project orientation; update this file after each meaningful green slice.

## Current Snapshot

- Repository: `https://github.com/fliperama86/neo-recomp.git`
- Branch: `main`
- Latest pushed commit: see `git log --oneline -1` after each push
- Local validation: `ctest --test-dir build --build-config Debug --output-on-failure`
- Current test status: 13/13 passing
- Detailed CPU correctness tracker: [`docs/68k_correctness_tracker.md`](68k_correctness_tracker.md)
- Reference contrast: [`docs/segagenesisrecomp_contrast.md`](segagenesisrecomp_contrast.md)
- Static opcode sweep: all decoder-recognized non-`UNKNOWN` opcodes emit without
  an unsupported-dispatch stub in the current synthetic sweep
- Generated-code/runtime boundary: emitted C includes only
  `ngrecomp/generated_abi.h`; the Neo Geo runtime is one implementation of that
  ABI, while the recompiler CLI no longer links the runtime library
- Real smoke input: `G:\Mister\NEOGEO\mslug.neo`
- Real smoke command:

```powershell
.\build\Debug\neo-recomp.exe --game .\games\mslug.toml --neo G:\Mister\NEOGEO\mslug.neo --emit-c .\build\mslug_recomp.c
```

## Goal

The near-term goal is not full Neo Geo emulation. The near-term goal is a
small, behavior-checked static recompilation loop:

- load a known Neo Geo program image
- discover candidate 68000 routines
- emit C for a growing 68000 subset
- compile the generated C
- compare generated behavior against an interpreter oracle on small fixtures
- use real Metal Slug generation as a smoke test and opcode frontier finder

## Validation Model

The strongest current check is `test_generated_exec`:

- a build-time helper creates a synthetic 68k ROM fixture
- `ng_emit_c` emits C for that fixture into the build directory
- CMake compiles that generated C into the test binary
- a small interpreter runs the same fixture bytes as an oracle
- the test compares registers, SR, dispatch misses, and fake bus contents

This is intentionally behavior-based. Recompiling generated C back to 68k and
expecting byte-identical machine code is not a useful invariant.

## Real ROM Frontier

Latest local Metal Slug smoke used:

```sh
./build/neo-recomp --game games/mslug.toml \
  --neo ~/Documents/Games/Mister/NEOGEO/mslug.neo \
  --emit-c build/mslug_recomp.c \
  --emit-dispatch-audit build/mslug_dispatch_audit.txt \
  --fail-on-dispatch-gaps
cc -std=c99 -Wall -Wextra -Iinclude -Irecompiler/src \
  -c build/mslug_recomp.c -o build/mslug_recomp.o
```

The generated C now compiles to an object, and the current Metal Slug smoke
passes `--fail-on-dispatch-gaps`. The current dispatch audit is:

```text
function candidates: 1155
dispatch audit: sites=73 direct=69 missing_direct=0 external_direct=5 computed=0 runtime_computed=3 jump_tables=1 table_resolved=4 table_missing=0
$000900 DIRECT JMP target=$C00438 discovered=no external=yes
$00092E DIRECT JSR target=$C004CE discovered=no external=yes
$000940 DIRECT JSR target=$C0044A discovered=no external=yes
$000862 DIRECT JMP target=$C00444 discovered=no external=yes
$000984 DIRECT JSR target=$C004C2 discovered=no external=yes
$0006C8 COMPUTED JSR target=<runtime> allowed=yes
$001F3C COMPUTED JSR target=<runtime> allowed=yes
$05BF8A COMPUTED JMP target=<runtime> allowed=yes
```

The discovery candidate cap is now 8192; Metal Slug currently discovers 1155
candidate entry/instruction-boundary addresses without truncation after seeding
the cart VBlank vector handler (`$0008F6`), the cartridge header start
trampoline (`$000122`), and the BIOS-reached init callback (`$001F84`). The previous
missing direct P-ROM targets (`$05DC1C`, `$05DC34`, `$024FB8`) are now
discovered. Direct BIOS/system-ROM targets are classified as external runtime
fallbacks instead of missing P-ROM discovery; the current external BIOS targets
visible from cart code are `$C00438`, `$C00444`, `$C0044A`, `$C004C2`, and
`$C004CE`. The RAM-loaded callback at
`$0006C8` (`MOVEA.L (A6),A0; JSR (A0)`) and the additional VBlank-reached
computed sites (`$001F3C`, `$05BF8A`) are explicitly classified in
`games/mslug.toml` as runtime-computed dispatch sites, so they remain visible in
the audit without failing the static P-ROM dispatch-gap check.

The generated executable checkpoint uses the reusable smoke harness:

```sh
cc -std=c99 -Wall -Wextra -Iinclude -Irecompiler/src \
  tools/generated_smoke_harness.c build/mslug_recomp.c \
  runtime/src/neogeo_runtime.c recompiler/src/p_rom.c \
  -o build/mslug_smoke_harness
./build/mslug_smoke_harness ~/Documents/Games/Mister/NEOGEO/mslug.neo
```

Current result:

```text
starting cart entry $0007CC ssp=$0010F300
dispatch miss at $C00444
returned pc=$C00444 sr=$2004 sp=$0010F300
```

So static CPU recompilation reaches executable cartridge code without BIOS and
the non-BIOS frontier is the expected runtime/BIOS fallback at `$C00444`, not a
generated-C or static P-ROM dispatch gap.

The BIOS-backed smoke path now has a small BIOS recompilation tool plus a
runtime external-dispatch hook. Users must provide their own BIOS; local testing
used a MiSTer `sp-s2.sp1` dumped in word-swapped form, so the helper defaults to
word-swapping it into 68000 byte order:

```sh
./build/generate-bios-recomp \
  ~/Documents/Games/Mister/NEOGEO/bios/sp-s2.sp1 \
  0xC00444,0xC004C2 \
  build/bios_recomp.c

cc -std=c99 -Wall -Wextra -Iinclude -Irecompiler/src \
  -DNG_GENERATED_CALL=ng_cart_generated_call \
  -DNG_GENERATED_DISPATCH=ng_generated_call \
  -c build/mslug_recomp.c -o build/mslug_recomp.o
cc -std=c99 -Wall -Wextra -Iinclude -Irecompiler/src \
  -DNG_GENERATED_CALL=ng_bios_generated_call \
  -DNG_GENERATED_DISPATCH=ng_generated_call \
  -c build/bios_recomp.c -o build/bios_recomp.o
cc -std=c99 -Wall -Wextra -Iinclude -Irecompiler/src \
  -DNG_GENERATED_SMOKE_HAS_BIOS \
  -DNG_GENERATED_SMOKE_COMBINED_DISPATCH \
  -c tools/generated_smoke_harness.c \
  -o build/generated_smoke_harness_bios.o
cc -std=c99 -Wall -Wextra -Iinclude -Irecompiler/src \
  -c runtime/src/neogeo_runtime.c -o build/neogeo_runtime.o
cc -std=c99 -Wall -Wextra -Iinclude -Irecompiler/src \
  -c recompiler/src/p_rom.c -o build/p_rom.o
cc build/generated_smoke_harness_bios.o build/mslug_recomp.o \
  build/bios_recomp.o build/neogeo_runtime.o build/p_rom.o \
  -o build/mslug_bios_smoke_harness
./build/mslug_bios_smoke_harness --bios \
  ~/Documents/Games/Mister/NEOGEO/bios/sp-s2.sp1 \
  ~/Documents/Games/Mister/NEOGEO/mslug.neo
```

The first BIOS-backed frontier was the DIP switch/watchdog register:

```text
starting cart entry $0007CC ssp=$0010F300
ng68k_write8 miss at $300001 value=$FF
```

The runtime now handles `$300001` as the active-low DIP switch read and watchdog
kick write. The smoke harness also supports a trampoline-safe combined cart/BIOS
dispatcher, now uses scanline-driven VBlank for BIOS-backed smoke, and the
runtime covers default active-low P1/P2/status-B input reads plus the port-output
latch. At that point in the BIOS seed expansion, the checkpoint was:

```text
starting cart entry $0007CC ssp=$0010F300
dispatch miss at $C00438
returned pc=$C00438 sr=$2104 sp=$0010F2F6
```

That meant generated BIOS code could wait for VBlank, enter the generated cart
VBlank handler, and return with the next missing BIOS entry at `$C00438`. With
the larger discovery budget at that stage, the BIOS seed list reported
`BIOS candidates: 2139` without truncation; `$C00438` remains the default
checkpoint for that historical step because it is a separate BIOS handoff that
was not yet reached by the static seed list.

Generated dispatch now splits the per-address switch from the public dispatch
entry and uses a pending-address loop, so nested `JMP`/`JSR`/`RTS`/exception
dispatches no longer grow the host C stack. The combined cart/BIOS smoke wrapper
uses the same trampoline shape for cross-module handoffs. As an integration
probe, manually adding `$C00438` to the local BIOS seed list no longer stack
overflows. After adding narrow system-latch coverage and a headless VRAM port
backing store for `$3A0003` and `$3C0000-$3C0005`, that extended probe runs
without those bus-write misses and currently returns at:

```text
dispatch miss at $C11142
returned pc=$C11142 sr=$2004 sp=$0010F300
```

That extended probe became the next frontier finder for small runtime bus
slices.

Continuing that local-only seed expansion through `$C11142`, `$C18694`, and
`$C11FFA` exposed the first sound-port access. The runtime now treats `$320000`
as a headless sound command/reply latch, preserving the previous default
`0xFF` read value and recording 68000->Z80 commands without emulating the Z80 or
YM2610. The next BIOS step exposed a wider port-output mirror at `$380021`, so
the port-output latch now accepts odd addresses under the documented
`$380000/$390000` mirror. With the 8192-candidate budget, cart seeds for
`$000122`/`$001F84`, all currently visible cart->BIOS direct targets
(`$C00438`, `$C00444`, `$C0044A`, `$C004C2`, `$C004CE`), and further local-only
BIOS seeds through the `$C187xx`/`$C180xx` helper chain, the latest local probe
now reaches the deterministic 500k dispatch budget instead of relying on a
wall-clock timeout, and it gets far enough to write headless VRAM:

```text
starting cart entry $0007CC ssp=$0010F300
returned pc=$C11646 sr=$2100 sp=$0010F2EE
smoke summary: dispatches=500000 cart=13547 bios=486453 unique=288 hot_overflow=0 last=$C00438 pc=$C11646 sr=$2100 sp=$0010F2EE polls=2655844 watchdog=13587 vblank=10060 frame=10060 timer_irq=0 irqack=9290 irq_pending=$0004 scanline=4 sound=$03 port=$00 wram_nonzero=606 wram_sum=$0000FB79 palette_nonzero=0 palette_sum=$00000000 vram_nonzero=2304 vram_sum=$019C9E00 recent_loop=0
dispatch hot: unique=288 overflow=0 top0=$C18500:74304 top1=$C184FC:18578 top2=$C184F8:18577 top3=$C184F4:18573 top4=$C1866C:18570
smoke budget reached at $C11646 after 500000 dispatches
progress oracle: ok budgets=10000,50000,100000,500000 final_pc=$C11646 cart=13547 bios=486453 unique=288 polls=2655844 vblank=10060 frame=10060 scanline=4 irqack=9290 watchdog=13587 wram_nonzero=606 wram_sum=$0000FB79 palette_nonzero=0 palette_sum=$00000000 vram_nonzero=2304 vram_sum=$019C9E00 final_recent_loop=0 max_recent_loop=50
```

That is the first controlled "keeps running headless" checkpoint: no dispatch
or bus miss before the budget. It is not yet a boot/attract correctness proof:
the expanded BIOS probe still reports `BIOS candidates: 8192 (truncated)`, and
the harness only has a first-pass state oracle (multi-budget dispatch counts,
cart/BIOS dispatch growth, unique dispatch coverage, hot dispatch rankings,
RAM/VRAM checksums,
watchdog/poll/VBlank/frame/IRQACK growth, pending-IRQ state, current scanline,
and a short recent-dispatch loop detector), not a boot/attract-mode success
oracle. The 50k checkpoint can stop in a short BIOS polling loop, but the 500k
final checkpoint leaves that loop (`final_recent_loop=0`) and now requires
nonzero VRAM writes.

The smoke can also save final-budget CPU-visible state for offline inspection:

```sh
NG_MSLUG_SNAPSHOT_DIR=build/mslug_snapshot scripts/run_mslug_headless.sh
```

That produces `work_ram.bin` (64 KiB), `palette_ram.bin` (16 KiB),
`vram_be.bin` (64K big-endian VRAM words), and `summary.txt`. The current
500k snapshot summary matches the smoke output above and gives us concrete
artifacts to feed a minimal visualizer/debug viewer:

```sh
tools/render_snapshot_debug.py build/mslug_snapshot
```

The visualizer writes dependency-free PPM diagnostics under
`build/mslug_snapshot/debug_images/`: grayscale work RAM bytes, raw VRAM-word
color hashes, a nonzero-VRAM mask/tint, approximate palette swatches, and a
short text report. These are intentionally not accurate Neo Geo rendering yet;
they are a bridge from headless numeric counters to inspectable artifacts.
Both the 500k and 5M snapshots still have `palette_nonzero=0`, so the next
display-facing runtime question is why the current path reaches VRAM writes
before any palette writes.

This snapshot/debug-render path is also the cleanest seam for a real SDL host.
When SDL2 is available through `pkg-config`, CMake builds the optional
`neo-snapshot-viewer` target, which interactively displays the same snapshot
files with raw VRAM, nonzero-VRAM, work-RAM, and palette modes. It remains an
offline diagnostic scaffold, not a live emulator loop, so there is still no
mandatory SDL dependency or added CI surface.

The next real-frame prerequisite is loading the cartridge asset regions that a
renderer will actually need. The `.neo` loader now has a tested full-container
path for P/S/M/V1/V2/C regions: P is still normalized for 68000 recompilation,
while S fix-layer and interleaved C sprite data are preserved for future video
decode/rendering work.

A manual single-budget deep probe also reaches its guard without a dispatch or
bus miss:

```text
smoke summary: dispatches=5000000 cart=943807 bios=4056193 unique=369 hot_overflow=0 last=$C116A4 pc=$C116C8 sr=$2108 sp=$0010F2B2 polls=23700433 watchdog=19149 vblank=89774 frame=89774 timer_irq=0 irqack=77854 irq_pending=$0004 scanline=97 sound=$04 port=$00 wram_nonzero=20380 wram_sum=$004BE705 palette_nonzero=0 palette_sum=$00000000 vram_nonzero=2304 vram_sum=$01A0C140 recent_loop=0
dispatch hot: unique=369 overflow=0 top0=$C18500:622816 top1=$C184FC:155706 top2=$C184F8:155705 top3=$C184F4:155701 top4=$C1866C:155698
progress oracle: ok budgets=5000000 final_pc=$C116C8 cart=943807 bios=4056193 unique=369 polls=23700433 vblank=89774 frame=89774 scanline=97 irqack=77854 watchdog=19149 wram_nonzero=20380 wram_sum=$004BE705 palette_nonzero=0 palette_sum=$00000000 vram_nonzero=2304 vram_sum=$01A0C140 final_recent_loop=0 max_recent_loop=0
```

The previous `$00067E: DC.W $D101` frontier has since been confirmed as
`ADDX.B D1,D0` and is decoded/emitted locally with generated-exec coverage.

Previously visible real-output frontiers, now covered locally by the generic EA
decode/emission path and/or generated-exec tests, include:

- `$097748: DC.W $12D8`
- `$00089C: DC.W $10C1`
- `$00215A: DC.W $B039`
- `$0137A6: DC.W $20C1`
- `$05A836: DC.W $207C`
- `$000420: DC.W $2248`
- `$0526C6: DC.W $32C0`
- `$05DC20: DC.W $21BC`
- `$05DC34: DC.W $4268`
- `$024FF2: DC.W $4298`

The real `.neo` smoke needs to be rerun to replace this now-stale frontier list
with the next set of generated-code misses.

## Supported Generated Behavior

For a concrete done/partial/missing checklist, see [`68k_correctness_tracker.md`](68k_correctness_tracker.md).

Covered by executable generated-C validation:

- control flow: direct and control-EA `JSR`, `BSR`, `RTS` stack-return
  dispatch, a reference-inspired stack-manipulating return regression, tail
  `JMP`, local `BRA`, and selected `Bcc`
- CFG discovery: direct call/jump targets, `JSR`/`BSR`/`STOP` continuations,
  instruction-boundary continuations for interrupt returns, taken `Bcc`
  targets, `DBcc` branch targets except never-branching `DBT`, and tail
  `BRA` targets are now seeded; `BRA` no longer causes discovery to scan
  unreachable fall-through bytes as code. `RTE`/`RTR` also terminate static
  candidate scanning because they resume from a stacked PC rather than the
  following ROM word. Discovery now checks the post-decode validator before
  seeding fall-through instruction starts, so `UNKNOWN`/`INVALID`/unvalidated
  frontier words stop speculative scanning instead of becoming extra function
  candidates. Emission now also materializes out-of-block branch-target
  trampolines before returning from terminal blocks, so branches to discovered
  labels outside the current linear body do not leave undeclared C labels
- system/exception-control paths: `TRAP`, `TRAPV`, `ILLEGAL`, A-line,
  F-line, `RESET`, `STOP`, `RTE`, and `RTR` are recognized. `TRAP`,
  `TRAPV`, `ILLEGAL`, A-line, F-line, failed `CHK`, and divide-by-zero now
  push 68000-style SR/PC exception frames and dispatch through the vector
  table; `RTE`/`RTR` pop stack frames back into SR/CCR and
  PC. Generated-exec now oracle-checks canonical `ILLEGAL` vector-4 entry,
  A-line/F-line emulator vectors 10/11, taken `TRAPV` vector-7 entry with the
  saved SR and next PC preserved, failed `CHK.W` vector-6 entry for
  negative values (`N` set in the saved SR) and above-bound values (`N`
  cleared in the saved SR), and oracle-checks `DIVU.W #0,Dn` vector-5 entry
  preserving the destination register while saving the pre-trap SR and next PC.
  Generated-exec now specifically covers unprivileged `RTR` restoring only
  the CCR bits from the active user stack, preserving the supervisor/system
  byte, popping the return PC, and dispatching through that target. Full-SR
  writes, `STOP`, exception entry, and `RTE` use the
  generated SR helper to switch active `A7` between `USP` and `SSP` when the
  S bit changes. User-mode privileged operations now vector through
  privilege-violation vector 8 with the faulting instruction PC. `STOP`
  installs the immediate SR, yields through the runtime stop hook when
  executed in supervisor mode, and can wake through a runtime-approved
  interrupt that returns via `RTE`. Supervisor-mode `RESET` now calls an
  explicit runtime external-device reset hook without changing 68000 state;
  the default hook is a no-op until NeoGeo device reset behavior is modeled.
  Full device/interrupt timing remains pending.
- branches: tested `BNE`, `BEQ`, `BCC`; `BCS` emission exists for carry cases
- conditional branches: all 68000 `Bcc` condition predicates are emitted;
  generated-exec covers the 14 real `Bcc` condition encodings (2-15; 0/1 are
  `BRA`/`BSR`) across every `N/Z/V/C` combination, plus older `BNE`, `BEQ`,
  `BCC`, and `BMI` spot checks
- condition operations: `Scc <ea>` and `DBcc Dn,disp` share the same
  condition predicate table; generated-exec covers all 16 `Scc` predicates
  across every `N/Z/V/C` combination, all 16 `DBcc` predicates across every
  `N/Z/V/C` combination on both counter-exhaustion fall-through and
  taken-branch paths, plus the older taken-branch `DBF` spot check
- flags: `N`/`Z` for covered operations; `C`/`X` for tested byte
  subtract/extend-add paths, register-count-zero shift/rotate cases, and
  selected nonzero shift/rotate edge cases
- data movement:
  - `MOVEQ`
  - `MOVE.B/W/L #imm,Dn`
  - `MOVE.W #imm,abs`
  - `MOVE.B #imm,abs`
  - `MOVE.B Dn,abs`
  - `MOVE.B (d16,An),Dn`
  - generic `MOVE.B/W/L <ea>,<ea>` paths covered so far by data-register,
    immediate, postincrement, address-indirect, indexed destination, and
    predecrement stack-pointer self-alias forms; `MOVE.L A7,-(A7)` now proves
    the source address register is captured before the destination
    predecrement side effect
  - generic `MOVEA.W/L <ea>,An` paths covered so far by immediate and
    address-register sources, including `MOVEA.W` sign-extension with CCR
    preserved
  - `MOVE.L An,abs`
  - generic `LEA <ea>,An` control-address loads covered so far by PC-relative,
    PC-index, and displacement sources, including extension-word PC base and
    word-index sign-extension behavior
  - `MOVEM.W/L` register-list transfers covered so far by long Dn masks to/from
    address-indirect memory, word memory-to-register sign extension into both
    data and address registers, postincrement memory-to-register self-load
    behavior where the addressing register receives the final EA, and
    predecrement register-to-memory masks, including MC68000 predecrement mask
    reversal and initial-`A7` storage when `A7` is also in the saved register
    list; control-mode coverage now includes address-displacement and
    address-indexed register-to-memory destinations, absolute-word
    register-to-memory destinations, plus address-displacement,
    address-indexed, absolute-long, and PC-relative memory-to-register sources
    with sparse D/A masks, extension-word PC bases, and word-index sign
    extension covered for indexed transfers
  - `MOVEP.W/L` staggered peripheral transfers covered by decode/emitter tests and
    generated-exec long Dn→alternate-byte-memory→Dn round-trip with flags preserved
  - `MOVE <ea>,CCR/SR` and `MOVE SR/CCR,<ea>` covered so far by immediate CCR
    source and absolute SR destination forms
  - `MOVE An,USP` and `MOVE USP,An` user-stack-pointer transfers
  - `LINK An,#disp` and `UNLK An`
- arithmetic/logical:
  - `ADD.W Dn,Dn`
  - generic `ADD.B/W/L <ea>,Dn` paths covered so far by Dn reads, including
    byte overflow-plus-carry flag behavior
  - generic `ADDI.B/W/L #imm,<ea>` paths covered so far by Dn destinations
  - generic `ADD.B/W/L Dn,<ea>` paths covered so far by postincrement memory
    destinations
  - generic `ADDQ.B/W/L #imm,<ea>` paths covered so far by Dn destinations
    and address-register destinations, including word-sized address-register
    full-32-bit arithmetic with CCR preservation
  - generic `SUB.B/W/L <ea>,Dn` paths covered so far by Dn reads, including
    byte borrow-to-`X/C` flag behavior
  - generic `SUB.B/W/L Dn,<ea>` paths covered so far by postincrement memory
    destinations
  - generic `SUBI.B/W/L #imm,<ea>` paths covered so far by postincrement
    memory destinations
  - generic `SUBQ.B/W/L #imm,<ea>` paths covered so far by Dn,
    address-register, and postincrement memory destinations, including
    word-sized address-register full-32-bit arithmetic with CCR preservation
  - generic `CMP.B/W/L <ea>,Dn` paths covered so far by Dn and abs reads,
    including byte overflow with `X` preserved
  - generic `ADDA.W/L <ea>,An`, `SUBA.W/L <ea>,An`, and
    `CMPA.W/L <ea>,An` paths covered so far by immediate sources, including
    word-source sign-extension, `ADDA`/`SUBA` CCR preservation, and `CMPA`
    preserving `X` while updating `N/Z/V/C`
  - `CMPM.B/W/L (Ay)+,(Ax)+` covered so far by word postincrement memory
    comparisons plus byte compares where source `A7` postincrements by two
    for stack-pointer alignment while preserving `X`
  - `CHK.W <ea>,Dn` covered so far by immediate in-range checks and
    oracle-backed failed checks that vector through exception vector 6 while
    saving `N` set for negative operands and cleared for above-bound operands
  - `ADDX/SUBX.B/W/L` register and predecrement-memory forms, covered so far
    by generated-exec byte add-extend and word subtract-extend paths
  - `ABCD/SBCD` register and predecrement-memory BCD extend forms, covered so
    far by generated-exec register BCD arithmetic plus predecrement-memory
    sticky-`Z`/`X`/`C` edge behavior
  - generic `OR.B/W/L <ea>,Dn` and `AND.B/W/L <ea>,Dn` paths covered so far
    by data-register sources
  - generic `OR.B/W/L Dn,<ea>`, `AND.B/W/L Dn,<ea>`, and
    `EOR.B/W/L Dn,<ea>` paths covered so far by data-register and
    postincrement memory destinations
  - `MULU.W <ea>,Dn` and `MULS.W <ea>,Dn` covered so far by immediate sources,
    including unsigned high-bit products, signed negative products, signed
    zero products, full 32-bit result storage, and required `N/Z/X/V/C`
    behavior
  - `DIVU.W <ea>,Dn` and `DIVS.W <ea>,Dn` covered so far by immediate sources,
    oracle-backed `DIVU.W #0,Dn` divide-by-zero vector-5 entry with destination
    preservation and pre-trap SR/next-PC frame saving, signed and unsigned
    quotient-overflow cases that leave the destination operand unchanged,
    successful quotient/remainder packing for unsigned zero/bit-15 quotients
    and signed negative/zero quotients, and required `N/Z/X/V/C` behavior on
    those covered non-zero-divisor divide paths; divide-by-zero flags remain
    architecturally undefined, so tests assert exception-frame parity instead
  - `EXG` register exchanges covered for data-register, address-register, and
    data-register/address-register pairs, with CCR preserved
  - `ANDI.B #imm,(d16,An)`
  - generic `ANDI.B/W/L #imm,<ea>` paths covered so far by Dn,
    displacement, and postincrement destinations
  - generic `ORI.B/W/L #imm,<ea>` paths covered so far by Dn destinations
  - generic `EORI.B/W/L #imm,<ea>` paths covered so far by postincrement
    memory destinations
  - `CMPI.B #imm,Dn`
  - generic `CMPI.B/W/L #imm,<ea>` paths covered so far by Dn,
    postincrement, and indexed memory destinations
  - `TST.B abs`
  - `TST.B (d16,An)`
  - generic `TST.B/W/L <ea>` paths covered so far by data-register and
    postincrement sources
  - `TAS <ea>` covered so far by absolute byte memory and data-register
    destinations, including test-before-set flag behavior with `X` preserved
  - `CLR.B/W/L abs`
  - `CLR.B/W/L Dn`
  - generic `CLR.B/W/L <ea>` paths covered so far by `(d16,An)` and `(An)+`
  - generic `BTST/BCHG/BCLR/BSET #imm,<ea>` and `BTST/BCHG/BCLR/BSET Dn,<ea>`
    paths covered so far by abs, Dn, and postincrement memory destinations
  - generic `NEG.B/W/L <ea>` and `NOT.B/W/L <ea>` paths covered so far by Dn,
    abs, and postincrement memory destinations
  - generic `NEGX.B/W/L <ea>` paths covered so far by absolute byte memory
    destinations
  - `NBCD <ea>` covered so far by absolute byte memory destinations and
    data-register sticky-`Z`/borrow-to-`X/C` edge cases, plus predecrement
    `A7` byte-stack destinations with decimal borrow and byte stack-pointer
    predecrement-by-two
  - `EXT.W Dn`, `EXT.L Dn`, and `SWAP Dn`
  - data-register `ASL/ASR/LSL/LSR/ROXL/ROXR/ROL/ROR` decode/emission,
    covered so far by generated-exec logical shift pairs, zero-count
    register-count flag cases, and nonzero `ASL.B`, `ASR.W`, `LSR.L`,
    `ROXR.B`, and `ROR.W` flag/result edge cases
  - memory `ASL/ASR/LSL/LSR/ROXL/ROXR/ROL/ROR` word forms, covered so far by
    absolute-memory logical shift, `ROXL.W`, and `ASL.W` flag/result cases
  - `PEA <ea>` control-address pushes covered so far by displacement,
    address-indirect `A7`, and PC-relative sources, including extension-word PC
    base calculation, CCR preservation, and source-EA capture before the stack
    push decrements `A7`
  - `ORI/ANDI/EORI #imm,CCR/SR` immediate status-register operations

Important caveat: this is not a complete 68000 condition-code model. `C`/`X`
and `V` are only trusted where generated-exec tests cover them.

## Recent Green Slices

- local: Fixed generic `MOVE` destination-predecrement source capture. The
  generated-exec fixture covered `MOVE.L A7,-(A7)` red first and now checks
  that the pushed longword is the old stack pointer value, not the
  already-decremented destination `A7`.
- local: Fixed `PEA` stack-push source-address capture. The generated-exec
  fixture covered `PEA (A7)` red first and now checks that the pushed longword
  is the old `A7` effective address while condition codes remain unchanged
  until the following recording instruction.
- local: Fixed `UNLK A7` stack-pointer alias semantics. `test_generated_exec`
  now covers opcode `$4E5F` red first and checks that `A7` becomes the
  longword read from the old stack pointer, without the normal post-pop `+4`
  applied for non-`A7` frame registers.
- local: Fixed `LINK A7,#disp` stack-frame semantics so the value pushed on
  the stack is the old predecrement `A7`, not the already-decremented stack
  pointer. The generated-exec fixture now covers `LINK A7,#-4` red first and
  checks both final `A7` and the saved longword at the frame slot.
- local: Added generated-exec/oracle coverage proving exception entry clears
  the live trace bit while preserving the saved SR. The new `ILLEGAL` fixture
  starts with `T` set, has the handler store live `SR`, and checks the stacked
  frame still contains the pre-exception `T`; the red step exposed that the
  interpreter oracle set supervisor mode without clearing `T`.
- local: Added standalone generated-exec/oracle coverage for `NOP` as an
  architectural no-op. The fixture now executes `NOP` between status/register
  setup and `MOVE SR,<abs>`/`STOP`, proving registers, CCR/SR, memory, and
  fall-through PC behavior stay aligned; the red step failed until the
  interpreter oracle modeled opcode `$4E71`.
- local: Tightened `JMP`/`JSR` validation so the implicit destination side must have an empty EA payload. `test_m68k_validate` covered stray destination payloads on `JMP (A0)` and `JSR (d16,PC)` red first.
- local: Tightened `TST` validation so its implicit destination side must have an empty EA payload. `test_m68k_validate` covered a stray destination payload on `TST.W D1` red first.
- local: Tightened `MOVEM` validation so the implicit register-list side must have an empty EA payload in both register-to-memory and memory-to-register directions. `test_m68k_validate` covered stray source payload on `MOVEM.L <list>,-(A7)` and stray destination payload on `MOVEM.W (d16,PC),<list>` red first.
- local: Tightened `PEA` validation so its destination EA must be completely empty, not merely `EA_NONE` by mode. `test_m68k_validate` covered a stray destination payload on `PEA (d16,An)` red first.
- local: Tightened `MOVE SR,<ea>` / `MOVE <ea>,SR` validation so the implicit SR side must have an empty EA payload in both directions. `test_m68k_validate` covered stray source payload on `MOVE SR,Dn` and stray destination payload on `MOVE (d16,PC),SR` red first.
- local: Tightened no-operand/control-immediate validation so `NOP`/`RESET`/`RTE`/`RTR`/`RTS`/`TRAPV`, `TRAP`/`STOP`/`ILLEGAL`, and immediate `CCR`/`SR` forms reject stray empty-EA payload fields. `test_m68k_validate` covered `ORI #imm,CCR`, `TRAP`, and `NOP` payload mutations red first.
- local: Tightened immediate logical/arithmetic/compare validation so `CMPI` and `ORI`/`ANDI`/`EORI`/`ADDI`/`SUBI` reject stray source-EA payload fields while still accepting their extension-word immediate and data-alterable destination metadata. `test_m68k_validate` covered `CMPI` and `EORI` no-source payload mutations red first.
- local: Added generated-exec oracle coverage for canonical `ILLEGAL`, A-line,
  and F-line exception entry. The oracle now vectors `$4AFC` through vector 4,
  `$Axxx` through vector 10, and `$Fxxx` through vector 11 while saving the
  pre-trap SR and next PC before handler dispatch.
- local: Added generated-exec oracle coverage for taken `TRAPV`, proving vector
  7 exception entry saves the pre-trap SR and next PC before dispatching to the
  handler. The red step failed in the interpreter oracle until the `0x4E76`
  path was modeled.
- local: Added generated-exec oracle coverage for `DIVU.W #0,D7` so
  divide-by-zero now proves vector-5 exception entry against the interpreter
  oracle, including destination preservation and pre-trap SR/next-PC frame
  saving before the handler `STOP` runs. Divide-by-zero condition-code bits
  remain architecturally undefined, so the invariant is exception-state parity.
- local: Tightened `MOVEA` PC-index decode/format coverage so word and long forms both expose source/destination EA metadata and format as `MOVEA.W`/`MOVEA.L` according to decoded size.
- local: Added generated-exec coverage for `MOVEA.W (d8,PC,Dn.W),An` so PC-index word sources are read with the extension-word PC base, sign-extended into the address register, and leave CCR unchanged. Removed the narrow PC-index `MOVEA.L` special path so `MOVEA` uses the shared EA reader and word sign-extension logic.
- local: Added generated-exec coverage for `CMPI.B #imm,Dn` signed-overflow flag behavior, proving `$80 - $01` sets `V`, clears `N/Z/C`, and preserves `X`. Removed the byte data-register special emission/oracle paths so `CMPI` uses the shared full compare flag logic.
- local: Tightened the post-decode validator fallback so out-of-range or
  otherwise unhandled mnemonic enum values are rejected instead of silently
  accepted. `test_m68k_validate` covered an unknown enum value red first.
- local: Tightened `BTST`/`BCHG`/`BCLR`/`BSET` validation so
  static and dynamic bit-op opcode kind, source register, and destination
  EA bits must match decoded metadata. `test_m68k_validate` covered
  static/dynamic `BTST`, `BSET`, `BCLR`, and `BCHG` opcode mismatches
  red first.
- local: Tightened `JMP`/`JSR` validation so address-indirect and
  address-displacement/indexed forms reject stray top-level target metadata,
  absolute forms reject stray top-level absolute/displacement metadata, and
  PC-displacement forms require decoded displacement metadata to match the
  source EA. `test_m68k_validate` covered stray `JMP (An)` target,
  missing/mismatched `JSR (d16,PC)` displacement, and stray absolute-source
  metadata red first.
- local: Tightened `PEA` validation so top-level target, absolute-address,
  and displacement metadata must stay empty while the control source EA carries
  the address payload. `test_m68k_validate` covered stray address-displacement
  and PC-relative `PEA` metadata red first.
- local: Tightened `MOVEP` validation so both data-register and
  address-displacement operands must use canonical no-stray EA payloads
  in both directions. `test_m68k_validate` covered stray source/destination
  register and address-displacement payloads red first.
- local: Tightened `LEA` validation so decoded PC-relative
  target/displacement metadata and absolute-source targets must match
  the source EA, while non-targeting control sources reject stray target
  metadata. `test_m68k_validate` covered mismatched PC-relative target,
  displacement, absolute target, and stray address-indirect target metadata
  red first.
- local: Tightened `MOVEM` validation so opcode direction, size bit,
  and transfer EA bits must match decoded register-list metadata.
  `test_m68k_validate` covered mismatched register-to-memory size and
  memory-to-register EA/direction opcode metadata red first.
- local: Tightened generic `MOVE` validation so opcode size, source EA
  bits, destination EA mode/register bits, extension lengths, and legacy
  metadata must match decoded metadata. `test_m68k_validate` covered
  mismatched destination-register and size opcode metadata red first.
- local: Tightened `JMP`/`JSR`/`PEA`/`LEA` validation so
  opcode family, source EA bits, and `LEA` destination register bits
  must match decoded metadata. `test_m68k_validate` covered mismatched
  `JMP`/`JSR` opcode-family plus `PEA`/`LEA` EA/register opcode metadata
  red first.
- local: Tightened `CMPM` validation so opcode size, source
  postincrement register, and destination postincrement register bits
  must match decoded metadata. `test_m68k_validate` covered mismatched
  destination-register and size opcode metadata red first.
- local: Tightened `MOVEA`/`ADDA`/`SUBA`/`CMPA` validation so
  opcode family, size/opmode, destination address register, and source
  EA bits must match decoded metadata. `test_m68k_validate` covered
  mismatched destination-register opcode metadata red first.
- local: Tightened `TST` validation so opcode size and source EA bits
  must match decoded metadata in addition to data-alterable EA, length,
  and no-stray-field checks. `test_m68k_validate` covered a mismatched
  opcode-size `TST.W D1` case red first.
- local: Tightened binary `ADD`/`SUB`/`CMP`/`OR`/`AND`/`EOR`
  validation so opcode operation, direction, size, source register,
  destination register, and EA bits must match decoded metadata.
  `test_m68k_validate` covered cross-operation, register-field, and
  direction-form opcode mismatches red first.
- local: Tightened immediate logical/arithmetic/compare validation so
  `ORI`/`ANDI`/`SUBI`/`ADDI`/`EORI`/`CMPI` opcodes must match decoded
  mnemonic, size, and destination EA bits in addition to immediate width
  and length checks. `test_m68k_validate` covered cross-opcode and
  opcode-size mismatches red first.
- local: Tightened word-sized `CHK`, `MULU`/`MULS`, and
  `DIVU`/`DIVS` validation so opcode class, destination register,
  and source EA bits must match decoded metadata. `test_m68k_validate`
  covered cross-mnemonic and destination-register opcode mismatches red first.
- local: Tightened `ADDX`/`SUBX` and byte-only `ABCD`/`SBCD`
  validation so register and predecrement forms require opcode mnemonic,
  size, R/M form, source register, and destination register bits to match
  the decoded metadata. `test_m68k_validate` covered mismatched extend/BCD
  opcode metadata red first.
- local: Tightened shift/rotate validation so register and memory
  forms require opcode direction, kind, size, count source, and EA bits
  to match the decoded metadata, and made memory shift/rotate decode
  reject non-canonical bit-11 encodings such as `$E8D0`. `test_m68k_validate`
  and `test_m68k_decode` covered mismatched immediate-count, register-count,
  memory-EA, cross-kind opcode metadata, and decoder over-recognition red first.
- local: Tightened `MOVE SR,<ea>`, `MOVE <ea>,CCR`, and
  `MOVE <ea>,SR` validation so opcode class and EA bits must match
  the decoded source/destination metadata, including PC-relative source
  forms. `test_m68k_validate` covered mismatched opcode EA and
  direction metadata red first.
- local: Tightened `ORI`/`ANDI`/`EORI #imm,CCR/SR` validation so
  each status-immediate form requires its exact opcode word in addition
  to the existing size, immediate-width, and no-operand metadata checks.
  `test_m68k_validate` covered mismatched CCR/SR/status opcodes red first.
- local: Tightened `ADDQ`/`SUBQ` decode and validation so word-sized
  address-register quick ops keep their word opcode size, byte-sized
  address-register quick opcodes decode as `UNKNOWN`, and validator
  opcode bits for quick count, operation, size, and destination EA
  must match the decoded metadata. `test_m68k_decode` and
  `test_m68k_validate` covered these red first.
- local: Tightened unary data-alterable validation so `CLR`, `NEG`,
  `NEGX`, `NOT`, `NBCD`, and `TAS` opcode class, size bits, and
  effective-address bits must match the decoded destination metadata.
  `test_m68k_validate` covered mismatched opcode register and
  cross-family metadata red first.
- local: Tightened `Scc`/`DBcc` validation so opcode condition bits
  and effective-address/register fields must match the decoded byte
  destination or DBcc counter metadata. `test_m68k_validate` covered
  mismatched condition and register/EA opcode metadata red first.
- local: Tightened `EXT`/`SWAP` validation so opcode class, size bit,
  and register bits must match the decoded data-register metadata
  (`$488n` for `EXT.W`, `$48Cn` for `EXT.L`, `$484n` for `SWAP`).
  `test_m68k_validate` covered mismatched opcode/register and
  cross-family opcode metadata red first.
- local: Tightened `MOVEQ` validation so opcode register bits and
  immediate byte must match the decoded data-register destination and
  sign-extended immediate. `test_m68k_validate` covered mismatched
  opcode register and immediate metadata red first.
- local: Tightened `EXG` validation so the opcode opmode and register
  bits must match the decoded Dn-Dn, An-An, or Dn-An exchange
  metadata. `test_m68k_validate` covered mismatched opmode and
  register opcode metadata red first.
- local: Tightened `MOVEP` validation so opcode data-register bits,
  address-register bits, size bit, and direction bit must match the
  decoded `Dn <-> (d16,An)` metadata. `test_m68k_validate` covered
  mismatched register and direction opcode metadata red first.
- local: Tightened `LINK`/`UNLK` validation so the opcode class and
  low register bits must match the decoded address-register metadata
  (`$4E50+n` for `LINK An,#disp`, `$4E58+n` for `UNLK An`).
  `test_m68k_validate` covered mismatched opcode/register and
  cross-family opcode metadata red first.
- local: Tightened privileged `MOVE USP` validation so `MOVE An,USP`
  now requires opcode `$4E6n`, `MOVE USP,An` requires opcode
  `$4E68+n`, and the opcode register/direction must match the decoded
  A-register operand. `test_m68k_validate` covered mismatched
  opcode/register and opcode/direction metadata red first.
- local: Tightened `STOP` and `TRAP` validation so `STOP` now
  requires opcode `$4E72`, and `TRAP #n` now requires a `$4E4n`
  opcode whose low nibble matches the decoded vector immediate.
  `test_m68k_validate` covered mismatched opcode/immediate metadata
  red first.
- local: Tightened `ILLEGAL`/A-line/F-line validation so the decoded
  emulator-exception vector now has to match the opcode class: `$4AFC`
  uses vector 4 metadata, A-line opcodes use vector 10 metadata, and
  F-line opcodes use vector 11 metadata. `test_m68k_validate` covered
  mismatched opcode/vector metadata red first.
- local: Tightened fixed no-operand system/control validation so `NOP`,
  `RESET`, `RTE`, `RTR`, `RTS`, and `TRAPV` now require their exact
  MC68000 opcode words instead of accepting a mismatched opcode attached to
  otherwise valid no-operand metadata. `test_m68k_validate` covered `NOP`
  and `TRAPV` opcode mismatches red first.
- local: Tightened `ORI`/`ANDI`/`EORI #imm,CCR/SR` validation so
  those no-EA forms now reject stray decoded register, form, address, or
  displacement metadata in addition to their existing exact size/immediate
  checks. `test_m68k_validate` covered malformed top-level fields red first.
- local: Fact-checked MC68000 `MOVEM` direction-specific EA classes
  against the Motorola/NXP Programmer's Reference Manual. The validator now
  requires exact register-to-memory control/predecrement destination payloads,
  exact memory-to-register control/postincrement source payloads, and
  MOVEM-specific PC-relative base calculations from the EA extension word
  after the register-mask word. `test_m68k_validate` covered malformed
  predecrement, PC-displacement, indexed, and postincrement payloads red
  first.
- local: Fact-checked MC68000 `BTST` and `BCHG`/`BCLR`/`BSET`
  static-immediate and dynamic-Dn bit-number forms against the Motorola/NXP
  Programmer's Reference Manual. The validator now requires exact bit-number
  source metadata, exact read-only `BTST` data-addressing destination
  payloads including PC-relative and dynamic-immediate destinations, exact
  data-alterable destination payloads for altering bit ops, canonical legacy
  fields, static/dynamic instruction lengths, and no stray condition/target
  metadata. `test_m68k_validate` covered malformed PC-relative, immediate,
  Dn, postincrement, and displacement metadata red first.
- local: Fact-checked privileged MC68000 `MOVE USP,An` / `MOVE An,USP`
  against the Motorola/NXP Programmer's Reference Manual. The validator now
  requires exact long-sized address-register payloads, the architectural
  direction encoded by the populated source or destination operand, an empty
  opposite operand, and no stray immediate/count/condition/form/target/
  absolute/displacement metadata. `test_m68k_validate` covered malformed
  AREG payloads, non-empty opposite operands, and stray target metadata red
  first.
- local: Fact-checked MC68000 `ADDQ`/`SUBQ` immediate 1-8 quick
  operations against the Motorola/NXP Programmer's Reference Manual,
  including word/long-only address-register destinations. The validator now
  requires exact quick-op destination EA payloads and canonical Dn,
  address-displacement, and absolute legacy metadata while rejecting stray
  source, condition, target, and malformed top-level form/register/absolute/
  displacement metadata. `test_m68k_validate` covered malformed Dn, An,
  address-displacement, and legacy-field metadata red first.
- local: Fact-checked MC68000 destination-only `CLR`/`NEG`/`NEGX`/`NOT`
  and byte-only `NBCD`/`TAS` data-alterable forms against the
  Motorola/NXP Programmer's Reference Manual. The shared unary validator now
  requires exact destination EA payloads, canonical Dn/address-displacement/
  absolute legacy metadata, and no stray source, count, condition, target,
  absolute, or displacement fields. `test_m68k_validate` covered malformed
  Dn, postincrement, absolute-long, and address-displacement metadata red
  first.
- local: Fact-checked MC68000 shift/rotate register-count, immediate-count,
  and memory one-bit forms against the Motorola/NXP Programmer's Reference
  Manual. The validator now rejects malformed Dn count/destination payloads,
  malformed memory-alterable EA payloads, and stray form/target/absolute/
  displacement metadata while preserving byte/word/long Dn forms and
  word-only one-bit memory forms. `test_m68k_validate` covered malformed
  Dn, source-count, absolute-word, absolute-long, and top-level metadata red
  first.
- local: Fact-checked MC68000 `Scc <ea>` byte-sized data-alterable
  destinations against the Motorola/NXP Programmer's Reference Manual. The
  validator now requires exact destination EA payloads for Dn, displacement,
  index, and absolute forms in addition to condition, length, and no-source
  metadata. `test_m68k_validate` covered malformed Dn, address-displacement,
  and absolute-word payloads red first.
- local: Tightened exact metadata for unsized system/control forms
  (`NOP`, `RESET`, `RTE`, `RTR`, `RTS`, `TRAPV`, `ILLEGAL`/A-line/F-line)
  plus immediate control forms (`STOP`, `TRAP`). The validator now rejects
  stray register, condition, form, target, absolute, displacement, and EA
  payload fields while preserving the canonical immediate/vector payloads for
  `STOP`, `TRAP`, and emulator-exception classes. `test_m68k_validate` covered
  malformed stray metadata red first.
- local: Fact-checked MC68000 `ADDX`/`SUBX` register and
  predecrement forms plus byte-only `ABCD`/`SBCD` forms against the
  Motorola/NXP Programmer's Reference Manual. The validator now requires exact
  Dn-Dn or predecrement-pair EA payloads, matching source/destination register
  fields, canonical form metadata, 2-byte instruction length, and no stray
  immediate, condition, target, absolute, or displacement metadata.
  `test_m68k_validate` covered malformed Dn payloads, predecrement payloads,
  and stray top-level metadata red first.
- local: Fact-checked MC68000 `CMPM (Ay)+,(Ax)+` against the
  Motorola/NXP Programmer's Reference Manual postincrement-only form. The
  validator now requires exact source/destination postincrement EA payloads,
  clean register fields, 2-byte instruction length, and no stray immediate,
  condition, target, form, absolute, or displacement metadata.
  `test_m68k_validate` covered malformed postincrement payloads and stray
  top-level metadata red first.
- local: Fact-checked MC68000 binary `ADD`/`SUB`/`CMP`/`OR`/`AND`/`EOR`
  register/EA forms against the Motorola/NXP Programmer's Reference Manual
  source/destination EA tables. The validator now requires exact source and
  destination EA payloads, PC-relative extension-word bases, index-register
  metadata, data-register payloads, instruction lengths, source-register
  metadata, and canonical legacy form/top-level fields for EA-to-Dn, Dn-to-
  memory, and `EOR` Dn-to-data-alterable forms. `test_m68k_validate` covered
  malformed indexed source/destination payloads, PC-relative absolute bases,
  data-register payloads, and legacy metadata red first.
- local: Fact-checked MC68000 `ADDI`/`SUBI`/`ORI`/`ANDI`/`EORI`
  and `CMPI` immediate-to-data-alterable forms against the Motorola/NXP
  Programmer's Reference Manual immediate and destination EA tables. The
  validator now requires exact destination EA payloads, immediate widths,
  immediate-plus-EA instruction lengths, Dn/absolute/displacement legacy
  metadata consistency, and no stray source, condition, or target fields.
  `test_m68k_validate` covered malformed Dn payload, address-displacement
  legacy metadata, and absolute-word payload metadata red first.
- local: Fact-checked MC68000 `TST <ea>` against the
  Motorola/NXP Programmer's Reference Manual data-alterable destination table
  and later-family PC/immediate exclusions. The validator now requires exact
  tested-operand EA payloads, instruction length, Dn/absolute/displacement
  legacy metadata consistency, and no stray destination, condition, target, or
  immediate fields. `test_m68k_validate` covered malformed Dn payload and
  displacement legacy metadata red first.
- local: Fact-checked MC68000 `CHK.W`, `MULU.W`/`MULS.W`, and
  `DIVU.W`/`DIVS.W` word data-source-to-data-register forms against the
  Motorola/NXP Programmer's Reference Manual source EA tables. The validator
  now requires exact source EA payloads, PC-relative extension-word bases,
  word immediate widths, Dn destination/register consistency, instruction
  length, and no stray legacy fields. `test_m68k_validate` covered malformed
  immediate, PC-relative, indexed-source, destination-register, and legacy-form
  metadata red first.
- local: Fact-checked MC68000 `MOVEA`, `ADDA`, `SUBA`, and `CMPA`
  against the Motorola/NXP Programmer's Reference Manual word/long
  address-register-operation forms and all-addressing-mode source tables. The
  validator now requires exact source EA payloads, word/long source
  extension lengths, PC-relative extension-word bases, immediate widths,
  address-register destination metadata, and no stray legacy fields; `ADDA`/
  `SUBA`/`CMPA` decode no longer claims a data-register-to-data-register
  legacy form. `test_m68k_decode` and `test_m68k_validate` covered malformed
  address-register operation metadata red first.
- local: Fact-checked MC68000 `MOVE SR,<ea>`, `MOVE <ea>,SR`, and
  `MOVE <ea>,CCR` against the Motorola/NXP Programmer's Reference Manual
  data/data-alterable EA tables. The validator now requires exact source or
  destination EA payloads, immediate width, PC-relative extension-word base,
  instruction length, and no stray top-level metadata while still rejecting the
  MC68010+ `MOVE CCR,<ea>` direction. `test_m68k_validate` covered malformed
  status-register move metadata red first.
- local: Tightened exact control-source metadata for `JMP`, `JSR`,
  `PEA`, and `LEA`. The validator now checks control EA payloads,
  PC-relative extension-word bases, index displacement ranges, and decoded
  mode/register fields; decode now fills the canonical mode/register metadata
  for direct absolute-long and PC-displacement control forms.
  `test_m68k_validate` and the opcode sweep covered malformed control-source
  metadata red first.
- local: Fact-checked MC68000 `MOVE` source and destination effective-address
  classes against the Motorola/NXP Programmer's Reference Manual. The
  post-decode validator now requires exact `MOVE` byte/word/long source and
  destination EA payloads, extension lengths, PC-relative source bases,
  immediate widths, index displacement ranges, and legacy form/register fields.
  `test_m68k_validate` covered malformed `MOVE` metadata red first.
- local: Fact-checked MC68000 `BRA`/`BSR`/`Bcc` branch
  displacements against the Motorola/NXP Programmer's Reference Manual. Decode
  now records byte/word branch displacements, and the validator requires exact
  byte-or-word branch metadata: opcode/mnemonic/condition consistency, `$FF` as
  a valid MC68000 byte displacement of `-1` rather than a 68020 long extension,
  no stray operands/legacy fields, and a target matching
  `PC + 2 + displacement`. `test_m68k_decode`, `test_m68k_validate`, and the
  opcode sweep covered malformed branch metadata red first.
- local: Fact-checked MC68000 `DBcc` against the Motorola/NXP
  Programmer's Reference Manual. The validator now requires exact 4-byte
  word-sized operand-free counter metadata, condition/register bounds,
  no stray immediate/form/absolute/EA payload fields, and a branch target that
  matches the extension-word displacement from the `PC + 2` base.
  `test_m68k_validate` covered malformed target and stray metadata red first.
- local: Fact-checked MC68000 `Scc` byte stores against the
  Motorola/NXP Programmer's Reference Manual. The validator now requires the
  exact data-alterable destination extension length, condition field range,
  byte size, no source operand, and no stray immediate, form, target, absolute,
  or top-level displacement metadata. `test_m68k_decode` and
  `test_m68k_validate` covered malformed metadata red first.
- local: Fact-checked MC68000 `MOVEQ`, `EXT`, and `SWAP` register-only
  forms against the Motorola/NXP Programmer's Reference Manual. `MOVEQ`
  decode now records its immediate-to-data-register destination metadata and
  the validator requires sign-extended 8-bit immediates. `SWAP` decode now
  records the PRM word attribute while preserving 32-bit result flag emission.
  `EXT`/`SWAP` validation now requires exact 2-byte data-register metadata,
  matching register fields, legal sizes, and no stray operands or legacy
  fields. `test_m68k_decode` and `test_m68k_validate` covered malformed
  metadata red first.
- local: Fact-checked MC68000 `EXG` register exchanges against the
  Motorola/NXP Programmer's Reference Manual. Decode now records the Rx/Ry
  register fields for Dn-Dn, An-An, and Dn-An exchanges, and the validator
  requires exact 2-byte long-sized register-only metadata with legal opmode
  shapes, matching top-level source/destination register fields, no extension
  operands, and no stray immediate, condition, form, target, displacement, or
  EA payload fields. `test_m68k_decode` and `test_m68k_validate` covered the
  malformed metadata red first.
- local: Fact-checked MC68000 `LINK`/`UNLK` stack-frame operations against
  the Motorola/NXP Programmer's Reference Manual. `LINK` decode now records
  the word-sized displacement used by the MC68000 encoding, and the validator
  requires exact 4-byte `LINK` metadata or exact 2-byte unsized `UNLK`
  metadata: address-register field range, no EA payload, no stray immediate,
  condition, form, target, or absolute-address fields, and zero displacement
  for `UNLK`. `test_m68k_decode` and `test_m68k_validate` covered the
  malformed metadata red first.
- local: Fact-checked MC68000 `MOVEP` peripheral data transfers against the
  Motorola/NXP Programmer's Reference Manual. The validator now requires exact
  4-byte word/long data-register-to-address-displacement or
  address-displacement-to-data-register metadata, data-register field
  consistency, displacement consistency, and no stray operands/legacy fields.
  Decode now records the source data register for Dn-to-memory `MOVEP`.
  `test_m68k_decode` and `test_m68k_validate` covered malformed metadata red
  first.
- local: Fact-checked MC68000 `MOVEM` register-list transfers against the
  Motorola/NXP Programmer's Reference Manual. The validator now requires
  word/long size, a 16-bit register mask, exact `4 + EA-extension` instruction
  lengths, no stray legacy fields, register-to-memory control-alterable or
  predecrement destinations, and memory-to-register control or postincrement
  sources. `test_m68k_validate` covered malformed metadata red first, and
  `test_m68k_decode` now covers a PC-displacement memory-to-register form.
- local: Fact-checked MC68000 `MOVE SR,<ea>`, `MOVE <ea>,SR`, and
  `MOVE <ea>,CCR` forms against the Motorola/NXP Programmer's Reference
  Manual. The validator now requires exact word-sized data/data-alterable EA
  lengths, one-sided source-vs-destination metadata, and no stray legacy fields
  for status-register transfers. Decode now rejects the 68010+ `MOVE CCR,<ea>`
  form for the MC68000 target and no longer lets its reserved size pattern fall
  through as a bogus `CLR`. `test_m68k_decode` and `test_m68k_validate` covered
  the malformed metadata red first.
- local: Fact-checked MC68000 `JMP`, `JSR`, `PEA`, and `LEA` control-source
  forms against the Motorola/NXP Programmer's Reference Manual. The validator
  now requires exact control-source extension lengths, no stray source/immediate
  metadata, unsized `JMP`/`JSR`, long-sized `PEA`/`LEA`, `LEA` destination
  address-register consistency, and static `JMP`/`JSR` form/target consistency.
  Decode now keeps generic `JMP`/`JSR` forms unsized. `test_m68k_decode` and
  `test_m68k_validate` covered the malformed metadata red first.
- local: Fact-checked MC68000 destination-only `CLR`/`NEG`/`NEGX`/`NOT`,
  `NBCD`, and `TAS` forms against the Motorola/NXP Programmer's Reference
  Manual. The validator now requires no source/immediate/count metadata,
  exact data-alterable destination instruction lengths, byte-only `NBCD`/`TAS`,
  and data-register legacy-field consistency. Decode now records Dn legacy
  metadata for `NBCD`/`TAS`. `test_m68k_decode` and `test_m68k_validate`
  covered malformed metadata first.
- local: Fact-checked MC68000 shift/rotate register-count, immediate-count,
  and memory one-bit forms against the Motorola/NXP Programmer's Reference
  Manual. The validator now enforces exact 2-byte register forms,
  immediate counts 1-8, dynamic Dn count metadata, Dn destination register
  consistency, word-only memory forms, exact memory-EA instruction lengths,
  and one-bit memory counts. `test_m68k_validate` covered malformed
  metadata first.
- local: Fact-checked MC68000 `ADDX`/`SUBX` extended arithmetic and
  `ABCD`/`SBCD` packed-BCD pair forms against the Motorola/NXP Programmer's
  Reference Manual. The validator now requires exact 2-byte register-pair or
  predecrement-pair metadata, matching source/destination register fields,
  register bounds, no stray immediate/condition fields, and byte-only BCD
  forms. `test_m68k_validate` covered malformed metadata first.
- local: Fact-checked MC68000 `CMPM (Ay)+,(Ax)+` metadata against
  the Motorola/NXP Programmer's Reference Manual. The validator now requires
  byte/word/long size, exact 2-byte length, postincrement source and
  destination address-register operands, source/destination register bounds,
  matching legacy register fields, and no stray immediate.
  `test_m68k_validate` covered the malformed metadata first.
- local: Fact-checked MC68000 binary `ADD`/`SUB`/`CMP` and logical
  `OR`/`AND`/`EOR` forms against the Motorola/NXP Programmer's Reference
  Manual. The decoder now preserves source-register metadata for
  `OR`/`AND <ea>,Dn`, and accepts valid word/long `ADD`/`SUB`/`CMP`
  address-register sources while rejecting byte address-register sources.
  The validator now enforces exact source/destination EA lengths,
  source-register consistency, `ADD`/`SUB`/`OR`/`AND` memory-alterable
  destination forms, and `EOR` data-alterable destinations.
  `test_m68k_decode` and `test_m68k_validate` covered the failures first.
- local: Fact-checked MC68000 `ADDQ`/`SUBQ` quick operations against
  the Motorola/NXP Programmer's Reference Manual. The validator now enforces
  quick counts 1-8 with no source operand, exact destination-EA instruction
  lengths, data-alterable destinations, and the word/long-only address
  register forms whose CCR is unaffected. `test_m68k_validate` covered the
  malformed metadata first.
- local: Fact-checked MC68000 `BCHG`/`BCLR`/`BSET` bit operations
  against the Motorola/NXP Programmer's Reference Manual. The validator now
  distinguishes dynamic Dn bit-number forms from static immediate-bit forms,
  enforces byte-vs-long destination sizes and exact source-EA instruction
  lengths, rejects oversized immediate bit numbers and stray source fields,
  and keeps altering bit ops constrained to data-alterable destinations.
  `test_m68k_validate` covered the failures first.
- local: Fact-checked MC68000 word-form `CHK`, `MULU`/`MULS`, and
  `DIVU`/`DIVS` source/destination metadata against the Motorola/NXP
  Programmer's Reference Manual. The validator now rejects malformed
  word-data-source-to-Dn metadata, including impossible source-EA instruction
  lengths, bad data-register destinations, and stray top-level immediate/source
  register fields. `test_m68k_validate` covered the failures first.
- local: Fact-checked immediate arithmetic/logical `#data,<ea>` forms against
  the Motorola/NXP Programmer's Reference Manual. The validator now rejects
  malformed `ADDI`/`SUBI`/`ORI`/`ANDI`/`EORI` metadata with impossible
  instruction lengths, oversized byte/word immediates, stray source operands,
  or non-data-alterable destinations. `test_m68k_validate` covered the
  failures first.
- local: Fact-checked `MOVEA`, `ADDA`, `SUBA`, and `CMPA` address-register
  destination forms against the Motorola/NXP Programmer's Reference Manual.
  The validator now rejects malformed address-register-operation metadata with
  invalid byte lengths, byte sizes, missing sources, bad destinations, or stray
  top-level immediates while preserving valid word/long source EA forms.
  `test_m68k_validate` covered the failures first.
- local: Fact-checked `TST <ea>` and `CMPI #data,<ea>` against the
  Motorola/NXP Programmer's Reference Manual with MC68000 scope. The validator
  now rejects malformed `TST` and `CMPI` metadata, including stray operands,
  impossible instruction lengths, oversized byte/word immediates, and
  later-family/non-data-alterable destination forms. `test_m68k_validate`
  covered the failures first.
- local: Fact-checked `Scc <ea>` against the Motorola/NXP Programmer's
  Reference Manual. The validator now rejects malformed `Scc` metadata unless
  it is a byte-sized condition-store with no source/immediate operands and a
  data-alterable destination encoded at a valid MC68000 instruction length.
  `test_m68k_validate` covered valid and invalid forms first.
- local: Fact-checked `DBcc Dn,<label>` against the Motorola/NXP Programmer's
  Reference Manual. Decode now records the architectural word-sized counter
  operation, and the validator rejects malformed `DBcc` metadata unless it is a
  4-byte, word-sized, operand-free instruction with a valid condition and data
  register counter. `test_m68k_decode` and `test_m68k_validate` covered the
  failures first.
- local: Fact-checked `BTST` effective-address tables against the
  Motorola/NXP Programmer's Reference Manual. Static immediate-bit `BTST` now
  decodes and emits read-only PC-relative data destinations, static
  `BTST #n,#data` remains invalid, and the validator distinguishes read-only
  `BTST` data destinations from altering bit-op data-alterable destinations.
  `test_m68k_decode`, `test_m68k_validate`, and `test_c_emitter` covered the
  failures first.
- local: Fact-checked `MOVE <ea>,<ea>` source legality against the
  Motorola/NXP Programmer's Reference Manual. Byte-sized `MOVE An,<ea>` is now
  rejected in decode as `UNKNOWN`, while `MOVE.W An,Dn` remains legal; the
  validator also rejects synthetic byte `MOVE` address-register sources and
  allows word/long address-register sources. `test_m68k_decode` and
  `test_m68k_validate` covered the failures first.
- local: Fact-checked `LEA` metadata against the Motorola/NXP Programmer's
  Reference Manual. `LEA (d16,PC),An` decode now records the long-sized
  control source and address-register destination, and the validator now
  rejects malformed `LEA` shapes with missing sources, non-long sizes, or
  non-address-register destinations. Tests in `test_m68k_decode` and
  `test_m68k_validate` covered the failures first.
- local: Fact-checked control-transfer source forms against the Motorola/NXP
  Programmer's Reference Manual. The post-decode validator now rejects stray
  destination operands on `JMP`/`JSR`, enforces long-sized `PEA` effective
  address pushes, and keeps all three instructions constrained to control
  source addressing; `test_m68k_validate` covered valid and invalid shapes
  first.
- local: Fact-checked register-only instruction metadata against the
  Motorola/NXP Programmer's Reference Manual. `MOVEQ` decode now records the
  architectural long-sized result, and the post-decode validator now rejects
  malformed `MOVEQ`, `EXT`, `SWAP`, `LINK`, and `UNLK` instruction shapes
  with bad sizes, lengths, or stray operands; `test_m68k_decode` and
  `test_m68k_validate` covered the failures first.
- local: Fact-checked unary RMW instruction sizes against the Motorola/NXP
  Programmer's Reference Manual. The post-decode validator now rejects
  malformed generic unary sizes for `CLR`/`NEG`/`NEGX`/`NOT` and enforces the
  MC68000 byte-only size for `NBCD` and `TAS`, with `test_m68k_validate`
  covering valid byte forms and invalid word/long/unsized forms first.
- local: Fact-checked `MOVEM` PC-indexed memory-to-register transfers against
  the Motorola/NXP Programmer's Reference Manual. Generated-exec now covers
  `MOVEM.W (d8,PC,Dn.W),D5/A6`, verifying the extension-word PC base,
  word-index sign extension, sparse D/A load order, word sign-extension into
  both data and address registers, and CCR preservation.
- local: Fact-checked `MOVEM` indexed control-mode ordering against the
  Motorola/NXP Programmer's Reference Manual. Generated-exec now covers sparse
  D/A-mask long transfers through `(d8,An,Dn.W)` in both directions,
  including word-index sign extension, D0/D3/A1/A6 register-to-memory order,
  D1/D4/A2/A5 memory-to-register order, CCR preservation, and effective
  address computation before loading a listed base address register. The
  generated-exec fixture generator now routes fixture seeds through the
  bounded discovery helper and has a larger discovery budget for the growing
  CPU fixture set.
- local: Fact-checked nonzero shift/rotate flag semantics against the
  Motorola/NXP Programmer's Reference Manual. Generated-exec now covers
  `ASL.B`, `ASR.W`, `LSR.L`, `ROXR.B`, and `ROR.W` data-register cases plus
  memory `ROXL.W` and `ASL.W`, verifying result bits, `X/C`
  last-shifted-bit behavior, pure-rotate `X` preservation, zero results,
  arithmetic-left overflow, and memory word writes.
- local: Fact-checked `MOVEM` control-mode ordering against the Motorola/NXP
  Programmer's Reference Manual. Generated-exec now covers D/A mask order for
  address-displacement and absolute-word register-to-memory long transfers,
  address-displacement memory-to-register long transfers, absolute-long and
  PC-relative memory-to-register word transfers, word sign-extension into both
  data and address registers, and CCR preservation.
- local: Fact-checked word-form `MULU.W`/`MULS.W` semantics against the
  Motorola/NXP Programmer's Reference Manual. Generated-exec now covers an
  unsigned product with bit 31 set, a signed negative product, and a signed
  zero product, verifying the destination's low word is multiplied into a
  full 32-bit result while `X` is preserved, `C/V` are cleared, and `N/Z`
  reflect the product.
- local: Fact-checked successful `DIVU.W`/`DIVS.W` word-form semantics against
  the Motorola/NXP Programmer's Reference Manual. Generated-exec now covers
  unsigned zero quotients, unsigned quotients with bit 15 set, signed negative
  quotient/remainder packing, and signed zero quotients, verifying
  upper-word remainder/lower-word quotient storage plus `X` preservation,
  `C/V` clearing, and quotient-derived `N/Z`.
- local: Added a deterministic generated-exec `DBcc` taken-branch matrix,
  fact-checked against the Motorola/NXP Programmer's Reference Manual. The
  fixture now covers all 16 predicates across all 16 `N/Z/V/C` combinations
  with `D0=1`, verifying false predicates decrement to zero and branch over a
  flag-neutral marker while true predicates fall through unchanged, with CCR
  snapshots preserved before any observer store changes flags.
- local: Added a deterministic generated-exec `Bcc` control-flow matrix. The
  fixture now covers all 14 architectural `Bcc` condition encodings across all
  16 `N/Z/V/C` combinations, using taken branches to skip a flag-neutral `ST`
  marker and verifying CCR preservation.
- local: Added a deterministic generated-exec `DBcc` condition matrix. The
  fixture now evaluates all 16 predicates across all 16 `N/Z/V/C`
  combinations, using a counter-exhaustion fall-through case to prove true
  predicates leave `D0` unchanged while false predicates decrement it to
  `$FFFF`, with CCR unchanged. Codegen also suppresses impossible local labels
  for never-branching `BF`/`DBT` target candidates.
- local: Added a deterministic generated-exec `Scc` condition matrix. The
  fixture now evaluates all 16 condition predicates across all 16 `N/Z/V/C`
  combinations, checks byte results, and verifies the CCR snapshot is unchanged
  after each group.
- local: Fact-checked `DIVU.W` quotient-overflow semantics. Generated-exec now
  covers an unsigned quotient larger than 16 bits, verifying `V` set, `C`
  clear, `X` preserved, and the destination data register left unchanged.
- local: Fact-checked `LEA` PC-index effective-address semantics.
  Generated-exec now covers `LEA (d8,PC,D0.W),A2`, proving the extension-word
  PC base plus word index calculation and condition-code preservation.
- local: Fact-checked `PEA` effective-address push semantics. Generated-exec
  now covers `PEA (d16,PC)`, proving the pushed long uses the extension-word
  PC-relative base and leaves condition codes unchanged.
- local: Fact-checked `EXG` register exchange forms. Generated-exec now covers
  address-register pairs and data-register/address-register pairs in addition
  to the existing data-register-pair coverage, confirming 32-bit swaps with
  condition codes unchanged.
- local: Fact-checked `TAS` test-before-set semantics. Generated-exec now
  covers `TAS D0`, proving flags are computed from the original byte, bit 7 is
  set in the data-register byte afterward, `V/C` are cleared, and `X` is
  preserved.
- local: Fact-checked `NBCD` byte-only BCD negate and predecrement addressing.
  Generated-exec now covers `NBCD -(A7)`, proving byte-sized stack-pointer
  predecrement by two, decimal borrow into `X/C`, sticky-`Z` clearing on a
  nonzero result, and memory writeback.
- local: Fact-checked `CMPM` postincrement addressing. Generated-exec now
  covers `CMPM.B (A7)+,(A0)+`, including the stack-pointer byte
  postincrement-by-two rule and `X` preservation while `N/V/C` are set.
- local: Fact-checked `ADDA.W`/`SUBA.W`/`CMPA.W` word-source semantics.
  Generated-exec now covers sign-extended immediate word sources,
  `ADDA`/`SUBA` leaving CCR untouched, and `CMPA` preserving `X` while
  clearing `N/Z/V/C` for a positive nonzero no-borrow comparison result.
- local: Fact-checked `MOVEA.W` sign-extension. Generated-exec now covers
  immediate word sources `#$FFFF`, `#$8000`, and `#$7FFF` loading into address
  registers as sign-extended 32-bit values while preserving CCR.
- local: Fact-checked `ADDQ`/`SUBQ` address-register behavior. The
  generated-exec oracle and fixture now cover word-sized quick operations using
  full 32-bit address arithmetic and preserving all CCR bits.
- local: Fact-checked `MOVEM` postincrement memory-to-register self-load
  behavior. Generated-exec now covers `MOVEM.L (A4)+,D0/A4`, proving the memory
  value for `A4` is ignored and `A4` receives the final postincremented EA,
  with condition codes unchanged.
- local: Added generated-exec `NBCD` data-register edge coverage. The
  fixture now checks the sticky-`Z` zero case with `X` clear and the tens
  complement case with `X` set, including decimal borrow into `X/C`.
- local: Fact-checked `ADD`/`SUB`/`CMP` condition-code rules.
  Generated-exec now covers byte `ADD` overflow plus carry into `X/C`, byte
  `SUB` borrow into `X/C`, and byte `CMP` overflow while preserving `X`.
- local: Fact-checked packed-BCD `ABCD`/`SBCD` condition-code rules.
  Generated-exec now covers predecrement-memory BCD arithmetic, including
  address predecrement writes, sticky zero on an `ABCD` zero result, decimal
  carry into `X/C`, and subsequent `SBCD` nonzero clearing of `Z/X/C`.
- local: Fact-checked `MOVEM.W` memory-to-register transfers against the
  Motorola/NXP programmer's reference. Generated-exec now covers word transfers
  sign-extending into both a data register and an address register, while
  leaving condition codes unchanged.
- local: Added generated-exec `MOVEP.L` coverage after cross-checking the
  Motorola/NXP programmer's reference and the local Genesis reference. The
  synthetic fixture now performs `D0 -> (d16,A0) alternate bytes -> D1`,
  verifies bytes land at offsets `+0/+2/+4/+6`, and confirms `MOVEP` leaves
  condition codes unchanged.
- local: Fact-checked MC68000 `MOVEM` predecrement ordering. The
  generated-exec oracle and fixture now cover the reversed predecrement mask
  correspondence (`#$8000` saving `D0`) and the MC68000/MC68010 rule that
  `MOVEM.L #$0001,-(A7)` stores the initial, not decremented, `A7` value when
  the addressing register is also in the saved list.
- local: Added generated-exec and static emitter coverage for
  `MOVE.L #imm,Dn`. This caught an immediate-to-data-register emission path
  that decoded as long but wrote only the low word; generated code now writes
  the full 32-bit immediate and applies long-sized `MOVE` `N/Z/V/C` flag
  behavior while preserving `X`.
- local: Fact-checked MC68000 signed divide overflow against the Motorola/NXP
  Programmer's Reference Manual. The generated-exec oracle and fixture now
  cover `DIVS.W #$FFFF,D0` with dividend `$80000000`, verifying that quotient
  overflow sets `V`, clears `C`, preserves `X`, and leaves the destination
  operand unaffected; emitted C now uses a 64-bit intermediate so the overflow
  case is detected before any host signed-division undefined behavior.
- local: Fact-checked MC68000 register-count-zero shift/rotate condition-code
  behavior against the Motorola/NXP Programmer's Reference Manual. The
  generated-exec oracle and fixture now cover `LSL.W Dn,Dm`, `ROL.W Dn,Dm`,
  and `ROXL.W Dn,Dm` with a zero register count, including X preservation,
  N/Z recomputation, C clearing for logical/pure rotates, and C mirroring X
  for ROX zero-count cases.
- local: Tightened CPU-only branch legality for MC68000 control flow.
  `test_m68k_validate` now covers valid byte/word `BRA`/`BSR`/`Bcc` forms,
  rejects 68020-style long branch lengths for the current MC68000 target, and
  rejects `Bcc` condition fields reserved for `BRA`/`BSR`.
- local: Added another CPU-only validator slice for MC68000 system/control
  forms. `test_m68k_validate` now covers `ORI`/`ANDI`/`EORI` to `CCR`/`SR`,
  `MOVE USP`, `TRAP`, `STOP`, `NOP`, `RESET`, `RTE`, `RTR`, `RTS`, `TRAPV`,
  and `ILLEGAL`/A-line/F-line legality, including invalid sizes,
  immediates, operands, directions, and instruction lengths.
- local: Continued the CPU-only MC68000 legality pass against the Motorola/NXP
  Programmer's Reference Manual. The validator now catches invalid
  `ADDA`/`SUBA`/`CMPA`, `MOVEA`, `EXG`, `LINK`/`UNLK`, `MOVEP`, `MOVEM`,
  `CMPM`, `ADDX`/`SUBX`, `ABCD`/`SBCD`, shift/rotate, `Scc`, and `DBcc`
  forms, and `EXG` decode now records its architectural long size; tests were
  added first in `test_m68k_validate`/`test_m68k_decode`.
- local: Refocused on pure MC68000 CPU legality and broadened the
  post-decode validator. It now rejects invalid immediate logical/arithmetic
  destinations, invalid `ADD`/`SUB`/`CMP`/`OR`/`AND`/`EOR` source/destination
  shapes, and invalid bit-op alterability/size forms, with failing tests first.
- local: Added a fact-checked cartridge-system BIOS/system ROM bus slice.
  `ng_neogeo_set_system_rom()` now backs `$C00000-$CFFFFF` reads, including
  the documented mirror area, while writes remain ignored/read-only.
- local: Added a fact-checked MVS backup RAM bus slice. The runtime now
  maps `$D00000-$DFFFFF` through a 64KiB backing store and honors
  `REG_SRAMLOCK/UNLOCK` write protection for backup RAM writes.
- local: Added a fact-checked NeoGeo palette RAM bus slice. The runtime
  now maps banked/mirrored palette RAM at `$400000-$7FFFFF`, handles
  `REG_PALBANK1/0` bank selection, and duplicates byte writes into both
  bytes of the addressed palette word.
- local: Added dispatch-audit gap enforcement. `ng_dispatch_audit_has_gaps()`
  is covered by unit tests, and the CLI now accepts `--fail-on-dispatch-gaps`
  to make smoke runs fail on missing direct targets, computed dispatches,
  unresolved table entries, or audit truncation.
- local: Added manual `[[jump_table]]` discovery metadata inspired by the
  reference pipeline. Game TOML now parses abs32, pcrel16, and BRA-table
  formats, discovery folds those targets into the function worklist, and the
  CLI reports jump-table counts.
- local: Adapted another `segagenesisrecomp` project-scaffolding pattern:
  `[game].discovery_files` now lets a game config merge additional TOML seed
  files relative to the parent config, and the CLI reports the discovery-file
  count.
- local: Added basic generated architectural PC maintenance. Dispatcher entry,
  instruction-boundary interrupt polls, linear fall-through, and `STOP`
  completion now update `g_ng_m68k.pc`, with generated-exec coverage for
  interrupt-resumed straight-line code, trace-handler `STOP`, and
  illegal-handler `STOP`.
- local: Added the first default runtime bus slice: fixed P-ROM reads,
  the repo's banked P-ROM window, 64KiB work RAM reads/writes, big-endian
  word/long helpers, and 24-bit wrapping for covered regions.
- local: Added a dispatch/control-flow audit artifact. `test_dispatch_audit`
  covers direct calls, missing direct targets, computed runtime dispatches,
  and PC-index jump tables; the CLI can now write it with
  `--emit-dispatch-audit <out.txt>`, raising the suite to 10 tests.
- local: Added a minimal game TOML parser and wired `--game` into function
  discovery. `[functions].entry` and `[functions].extra` now seed discovery
  alongside the cartridge entry, and `test_game_config` raises the suite to
  9 tests.
- local: Broadened the post-decode legality validator with failing tests
  first. It now catches invalid `ADDQ/SUBQ` quick/address-register forms,
  non-alterable `TST`/`CMPI` targets, invalid `CHK` operands, multiply/divide
  destination mistakes, and non-data-register `EXT`/`SWAP` forms.
- local: Switched the BIOS-backed Metal Slug smoke from direct poll-count
  auto-VBlank injection to the runtime's scanline path. The harness now advances
  one NTSC scanline per interrupt poll in BIOS mode, wraps every 264 scanlines,
  reports `frame=` plus the current scanline in the smoke summary, and the
  progress oracle requires cart/BIOS dispatch growth and frame growth across
  the default 10k/50k/100k/500k budgets plus late VRAM writes at the 500k
  checkpoint.
- local: Added unique-dispatch coverage and hot-dispatch ranking telemetry to
  the headless smoke. This makes long CPU-only probes easier to interpret when
  they do not hit a dispatch/bus miss or a short exact loop.
- local: Added runtime snapshot copy APIs for work RAM, palette RAM, and VRAM,
  plus optional `NG_MSLUG_SNAPSHOT_DIR` support in the Metal Slug smoke. The
  final-budget snapshot writes raw CPU-visible RAM/palette/VRAM artifacts and a
  text summary without adding renderer/input/sound complexity yet.
- local: Added `tools/render_snapshot_debug.py`, a no-dependency offline
  snapshot visualizer that converts the raw work RAM / palette RAM / VRAM dumps
  into PPM debug images. This is diagnostic only and not a full video renderer.
- local: Added an optional SDL2 `neo-snapshot-viewer` target, built only when
  SDL2 is available through `pkg-config`, for interactively inspecting the same
  final-budget snapshot artifacts without turning SDL into a required runtime or
  CI dependency.
- local: Added tested full `.neo` container region extraction for P/S/M/V1/V2/C.
  Program ROM loading still only exposes normalized P bytes to the CPU
  recompiler, but host/video work can now load S fix-layer data and interleaved
  C sprite data from the same cartridge image.
- local: Connected the LSPC timer model to NTSC frame/scanline advancement:
  runtime tests now cover 384-pixel scanlines, 264-scanline frame wrap,
  VBlank requests on frame start/wrap, and timer interrupts firing from
  frame-advanced pixel ticks.
- local: Added A-line and F-line emulator exception regressions with trace
  enabled. They vector through MC68000 vectors 10/11, save the next PC,
  and intentionally do not create a trace exception because the trapped
  opcode was not executed. The generated-exec fixture candidate capacity was
  raised to keep the growing regression corpus explicit.
- local: Added the first fact-checked LSPC timer/VBlank runtime slice:
  `REG_LSPCMODE`, `REG_TIMERHIGH`, `REG_TIMERLOW`, and `REG_TIMERSTOP`
  writes are modeled; odd-byte GPU/LSPC writes duplicate the byte into both
  halves; `ng_neogeo_begin_vblank()` requests VBlank and reloads the timer
  when configured; `ng_neogeo_advance_timer()` advances by pixel ticks and
  requests timer IRQs with reload-on-write/frame/zero behavior.
- local: Added the reference-inspired stack-manipulating return regression:
  a callee executes `ADDQ.L #4,A7; RTS` and correctly skips the local
  `JSR` continuation to dispatch to the caller's caller via the architectural
  stack.
- local: Closed another exception-priority gap from the official MC68000
  exception model: `DIVS`/`DIVU` divide-by-zero now uses the shared
  exception-frame path, switches user-mode faults onto `SSP`, clears live
  trace during exception entry, and services trace to the divide handler PC
  before handler execution when T was set at instruction start.
- local: Added generated-exec regression coverage for trace non-occurrence
  on not-executed illegal/privilege cases and for trace priority over a
  pending post-instruction interrupt.
- local: Added trace-priority handling for failed `CHK`: generated code now uses
  the shared exception frame path, then services trace to the CHK handler PC
  when tracing was enabled at instruction start.
- local: Added trace-priority handling for taken `TRAPV`: when V is set,
  generated code now stacks the TRAPV frame first, then services trace to the
  TRAPV handler PC when tracing was enabled at instruction start.
- local: Added trace-priority handling for `TRAP`: generated code now stacks the
  trap exception frame first, then services trace to the trap-handler PC when
  tracing was enabled at instruction start.
- local: Corrected trace eligibility to use the SR T bit captured at instruction
  start, then added trace coverage for `STOP` and `RTE`/`RTR` paths. This
  covers cases where the instruction itself changes SR before trace exception
  processing.
- local: Extended trace exception coverage to subroutine and return control
  flow. `JSR`/`BSR`/`JMP` now trace to the target PC after any return-address
  push, and `RTS` traces to the popped return PC before dispatching.
- local: Extended trace exception coverage to taken `DBcc` paths. The generated
  decrement-and-branch path now services trace with the taken target PC after
  updating the counter and before jumping.
- local: Extended trace exception coverage to taken `BRA`/`Bcc` paths. The
  generated branch path now services trace with the taken target PC before
  jumping, covered by a generated-exec `BMI` fixture.
- local: Added basic generated trace exception entry. Linear fall-through
  instructions now service SR T-bit tracing after execution, stack saved SR and
  next PC through vector 9, and clear the live T bit during exception entry.
  Branch/control-flow and instruction-generated exception priority cases remain
  tracked as partial.
- local: Added an initial post-decode MC68000 legality validator and wired
  checked emission through it. Validator coverage currently rejects
  `UNKNOWN`/`INVALID`, illegal control-EA uses, invalid `MOVE` destinations,
  invalid condition numbers, and selected data-alterable requirements.
- local: Added checked code-emission diagnostics. `ng_emit_c_checked()` now
  reports unsupported decoded instructions and decode errors through
  `NgEmitDiagnostics` and fails generation instead of only leaving runtime
  dispatch misses.
- local: Wired the first NeoGeo interrupt memory-map path: word writes to
  `$3C000C REG_IRQACK` and low-byte writes to `$3C000D` now clear pending IRQ
  sources through the runtime IRQACK handler, covered by runtime tests.
- local: Added NeoGeo cartridge interrupt source APIs for VBlank, timer, and
  reset-pending IRQs. Runtime tests verify cartridge autovector levels 1/2/3,
  source priority, and IRQACK-style clearing bits.
- local: Added a default runtime IPL controller with
  `ng_m68k_set_interrupt_level()`, `ng_m68k_clear_interrupt_level()`, and
  mask-aware `ng_m68k_take_interrupt()`. Unit coverage verifies ordinary mask
  inhibition plus the MC68000 level-7 lower-to-7 edge case.
- local: Added runtime-supplied instruction-boundary interrupts. Generated C now
  polls before each emitted instruction, accepted interrupts stack the current
  instruction PC, and `RTE` returns through instruction-start continuations
  discovered by `function_discovery`. Generated-exec covers a level-2
  interrupt landing between two `MOVEQ` instructions and resuming at the
  second instruction.
- local: Added basic interrupt entry from `STOP`: generated code now services
  runtime-approved interrupts after the stop hook, stacks an SR/PC frame on
  `SSP`, raises supervisor mode and the interrupt mask to the accepted level,
  vectors to the supplied handler, discovers the post-`STOP` continuation, and
  returns through `RTE`. Generated-exec covers both an accepted level-4
  autovector and a masked interrupt that must not wake.
- local: Added MC68000 privilege-violation guards for generated `STOP`,
  `RESET`, `RTE`, `MOVE <ea>,SR`, `MOVE USP`, and immediate-to-SR
  operations. User-mode violations now enter vector 8 with the faulting
  instruction PC, covered by generated-exec fixtures.
- local: Added generated supervisor/user stack switching for full-SR writes,
  `STOP`, exception entry, and `RTE`, with generated-exec coverage for
  `USP`/`SSP` preservation, exception entry from user mode, and `RTE` return
  to user mode.
- local: Added a local contrast against `segagenesisrecomp`, capturing
  reusable patterns for diagnostics, legality validation, game-TOML-driven
  discovery, dispatch audits, and oracle testing.
- `3ae6bea`: Fixed PC-relative `LEA` decode, rejected truncated decoded
  instructions, and made subroutine control flow stack-backed: `JSR`/`BSR`
  push return PCs, post-call continuations are discovered, and `RTS` pops and
  dispatches to the return PC with generated-exec coverage.
- local: Closed the synthetic opcode-sweep emission gap: all currently
  decoder-recognized non-`UNKNOWN` opcodes now emit without unsupported stubs.
  This tightened illegal effective-address filtering, added address-register
  `ADDQ/SUBQ`, predecrement `MOVEM` stores, divide-by-zero vectoring, `TRAPV`
  fall-through, and a runtime STOP hook.
- local: Added 68000-style exception vector stack semantics for `TRAP`,
  `TRAPV`, `ILLEGAL`, A-line/F-line emulator traps, and failed `CHK`, plus
  `RTE`/`RTR` stack unwinding back into generated dispatch.
- local: Added decode/format/emission paths for system/exception-control
  opcodes (`TRAP`, `TRAPV`, `ILLEGAL`, `RESET`, `STOP`, `RTE`, `RTR`).
- local: Added `CHK.W <ea>,Dn` decode/emission with in-range generated-exec
  coverage and a dispatch miss path for trap cases.
- local: Added `ABCD`/`SBCD` decode/emission for register and predecrement
  memory forms, including generated-exec BCD arithmetic coverage.
- local: Added `NBCD <ea>` decode/emission with BCD negate and extend-aware
  status updates, including generated-exec absolute-memory coverage.
- local: Generalized extend arithmetic to `ADDX/SUBX.B/W/L` for data-register
  and predecrement-memory forms, with generated-exec coverage for word
  subtract-extend.
- local: Added `MOVEP.W/L` decode/emission for staggered `(d16,An)` peripheral
  transfers in both directions, with decode and emitter coverage.
- local: Added `MOVE USP` transfers in both directions, including generated
  state coverage for USP and address-register updates.
- local: Added `CMPM (Ay)+,(Ax)+` decode/emission with postincrement compare
  flag behavior and generated-exec word coverage.
- local: Added `TAS <ea>` decode/emission with test-and-set byte semantics
  and generated-exec absolute-memory coverage.
- local: Added `NEGX <ea>` decode/emission on the existing unary RMW path,
  preserving the extend-aware Z behavior and adding generated-exec byte
  absolute-memory coverage.
- local: Generalized `JSR`/`JMP` decode/emission over control effective
  addresses, while keeping static target discovery constrained to absolute and
  PC-relative forms.
- local: Added memory word shift/rotate decode/emission for
  `ASL/ASR/LSL/LSR/ROXL/ROXR/ROL/ROR <ea>`, sharing the generated flag logic
  shape with data-register shifts and adding generated-exec absolute-memory
  coverage.
- local: Generalized immediate status-register operations to cover
  `ORI/ANDI/EORI #imm,CCR/SR`, with generated-exec coverage preserving CCR
  state through the existing SR store/check path.
- local: Added `MOVE <ea>,CCR/SR` and `MOVE SR/CCR,<ea>` decode/emission over
  the shared EA helpers, with generated-exec coverage for immediate CCR loads
  and absolute SR stores.
- local: Added `MOVEM.W/L` decode/emission for register-list memory transfers
  using generated unrolled register masks, with generated-exec coverage for
  long Dn saves/restores through address-indirect memory.
- local: Added data-register shift/rotate decode/emission for arithmetic,
  logical, extend, and pure rotate variants with generated-exec coverage for
  logical shifts.
- local: Added signed/unsigned 32-by-16 divide (`DIVS`/`DIVU`) over shared EA
  reads with generated-exec coverage for immediate operands and quotient flags.
- local: Generalized `LEA <ea>,An` onto the reusable effective-address-value
  helper with generated-exec coverage for displacement addressing.
- local: Added `EXG` register exchange decode/emission for data, address, and
  mixed register forms with generated-exec coverage for data-register swaps.
- local: Added signed/unsigned 16x16 multiply (`MULS`/`MULU`) over shared EA
  reads with generated-exec coverage for immediate operands.
- local: Added `Scc` and `DBcc` decode/emission using the generalized
  condition predicates, with generated-exec coverage for byte condition stores
  and DBF counter fallthrough.
- local: Generalized `Bcc` emission/oracle predicates to all 16 condition
  codes and added generated-exec coverage for an `N`-flag branch (`BMI`).
- local: Added stack-frame setup/teardown (`LINK`/`UNLK`) with generated-exec
  stack and frame-register validation.
- local: Added `PEA <ea>` decode/format/emission on control addressing modes,
  using a reusable effective-address-value helper and generated-exec stack
  push validation.
- local: Added generic `ADD/SUB Dn,<ea>` emission for memory destinations,
  sharing the same single-address RMW path as logical and immediate ops.
- local: Added generic register/logical binary operations (`OR`, `AND`, `EOR`)
  over the shared EA read/RMW helpers and raised the generated-exec window to
  keep the growing fixture covered.
- local: Added address-register arithmetic/compare (`ADDA`, `SUBA`, `CMPA`)
  over the shared EA reader, including generated-exec coverage for
  sign-extended word and long immediate sources.
- local: Added data-register transform ops `EXT.W`, `EXT.L`, and `SWAP` with
  decode/format/emission coverage and generated-exec sign-extension/register
  swap validation.
- local: Generalized unary read-modify-write `NEG <ea>` and `NOT <ea>` with
  correct flag behavior and generated-exec Dn/postincrement coverage.
- local: Added dynamic bit operations (`BTST/BCHG/BCLR/BSET Dn,<ea>`) on the
  same generic bit-op EA path, with generated-exec Dn and postincrement coverage.
- local: Generalized immediate bit operations (`BTST/BCHG/BCLR/BSET #imm,<ea>`)
  with correct Z flag behavior and shared EA/RMW handling.
- local: Generalized `ADDI/SUBI #imm,<ea>` using the shared arithmetic RMW
  path and generated-exec coverage for Dn and postincrement memory targets.
- local: Reused the logical-immediate RMW path for `ORI/EORI #imm,<ea>` and
  raised the generated-exec decode window to keep the growing fixture covered.
- local: Generalized `ADDQ/SUBQ #imm,<ea>` decode/emission with shared
  read-modify-write EA handling and generated-exec coverage for Dn and
  postincrement memory destinations.
- local: Added a reusable read-modify-write EA address helper and generalized
  `ANDI #imm,<ea>` without double-applying postincrement/predecrement effects.
- local: Generalized `CMPI #imm,<ea>` decode/emission with compare flag logic
  for non-register EA targets and generated-exec postincrement coverage.
- local: Generalized `TST <ea>` decode/emission onto the shared EA read helper,
  with generated-exec coverage for data-register and postincrement forms.
- local: Covered the generic MOVE/MOVEA frontier cluster (`$12D8`, `$10C1`,
  `$20C1`, `$207C`, `$2248`, `$32C0`, `$21BC`) with decode/emitter tests and
  expanded generated-exec oracle EA helpers.
- local: Added a generic 68k effective-address model and generic MOVE emission fallback.
- local: Added generic EA-to-data-register `ADD`, `SUB`, and `CMP` decode/emission.
- local: Added generic `CLR <ea>` decode/emission for address-register memory targets.
- local: Decode/emit byte `ADDX` data-register ops; verified `$D101` is `ADDX.B D1,D0`.
- `50bb51f` Emit byte `SUBQ` data-register ops.
- `8b0d1d5` Emit byte address-register loads.
- `d3c022a` Emit byte `TST` address-register reads.
- `c6a7e78` Emit byte `ANDI` address-register masks.
- `cd50959` Emit carry-based branches for byte compares.
- `039c319` Emit byte immediate compares.
- `c99382b` Emit byte data-register absolute stores.
- `7eff8a8` Emit data-register `CLR`.
- `7888f31` Emit byte immediate register moves.
- `d890ca4` Generalize address-register absolute stores.
- `c98e7e3` Emit absolute `TST` flags.
- `ceafbdb` Emit absolute `CLR` sizes.
- `fea7ba6` Emit byte immediate absolute moves.
- `4ebe4ba` Emit Z-flag conditional branches.

## Next Steps

Use this loop:

1. Pick the next real frontier opcode from `build\mslug_recomp.c`.
2. Add a failing decode test.
3. Add emitter behavior.
4. Add generated-exec oracle coverage if the instruction affects observable behavior.
5. Build and run `ctest`.
6. Regenerate `build\mslug_recomp.c` from `mslug.neo`.
7. Confirm the real frontier moved forward.
8. Update this document and [`68k_correctness_tracker.md`](68k_correctness_tracker.md).
9. Commit and push.

Immediate next slice:

- Decide whether to stop the CPU-only scope here or start runtime fallback work:
  the executable Metal Slug checkpoint now reaches BIOS/system-ROM dispatch
  `$C00444`.
- Keep NeoGeo timer/VBlank integration queued until the current CPU dispatch
  backlog is narrower.

Near follow-ups:

- Parse `games/*.toml` beyond function/discovery-file/jump-table metadata into machine-checkable discovery/runtime metadata.
- Add interior-label checks and broaden dispatch-audit target patterns beyond direct/computed/current table cases.
- Migrate more instruction families onto the generic EA helpers instead of adding bespoke forms.
- Continue broadening the decode/codegen legality layer so every remaining invalid source/destination EA combination fails loudly.
- Add byte/word/long helpers for arithmetic flags instead of ad hoc emitted flag code.
- Replace narrow condition handling with a tested condition-code helper table.
- Start separating instruction semantics into reusable generated helper functions when repeated emitted C becomes noisy.
- Add a tiny standalone ROM-like fixture that is closer to a hand-authored mini program than the current unit fixture.
- Continue runtime bus coverage for VRAM-facing ports, inputs, sound latch/Z80 communication, watchdog, protection/bank-switch variants, BIOS/system registers beyond current latch bits, and persistent save backing.
