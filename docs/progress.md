# Progress Tracker

Last updated: 2026-06-18

This is the live working document for `neo-recomp`. Keep the README focused on
project orientation; update this file after each meaningful green slice.

## Current Snapshot

- Repository: `https://github.com/fliperama86/neo-recomp.git`
- Branch: `main`
- Latest pushed commit: `50bb51f Emit byte SUBQ data-register ops`
- Local validation: `ctest --test-dir build --build-config Debug --output-on-failure`
- Current test status: 5/5 passing
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

Current Metal Slug generated C reaches this point in candidate `$000656`:

```text
$000656: LEA $100080,A6
$00065C: MOVE.L A6,$106E84
$000662: ANDI.B #$1,($5A,A6)
$000668: ANDI.B #$17,($69,A6)
$00066E: TST.B ($44,A6)
$000672: BEQ $000684
$000676: MOVE.B ($44,A6),D0
$00067A: MOVEQ #0,D1
$00067C: SUBQ.B #1,D0
$00067E: DC.W $D101
```

Next opcode frontier: `$D101`, likely `ADD.B D1,D0`.

Other visible real-output frontiers include:

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

## Supported Generated Behavior

Covered by executable generated-C validation:

- control flow: direct `JSR`, direct tail `JMP`, local `BRA`, selected `Bcc`
- branches: tested `BNE`, `BEQ`, `BCC`; `BCS` emission exists for carry cases
- flags: `N`/`Z` for covered operations; `C` for tested byte compare/subtract paths
- data movement:
  - `MOVEQ`
  - `MOVE.B/W #imm,Dn`
  - `MOVE.W #imm,abs`
  - `MOVE.B #imm,abs`
  - `MOVE.B Dn,abs`
  - `MOVE.B (d16,An),Dn`
  - `MOVE.L An,abs`
  - `LEA`
- arithmetic/logical:
  - `ADD.W Dn,Dn`
  - `SUBQ.B #imm,Dn`
  - `ANDI.B #imm,(d16,An)`
  - `CMPI.B #imm,Dn`
  - `TST.B abs`
  - `TST.B (d16,An)`
  - `CLR.B/W/L abs`
  - `CLR.B Dn`
  - `BCLR #imm,abs`
  - `ANDI #imm,SR`

Important caveat: this is not a complete 68000 condition-code model. `V` is not
implemented, and `C` is only trusted where tests cover it.

## Recent Green Slices

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
8. Update this document.
9. Commit and push.

Immediate next slice:

- Decode/emit `$D101`, likely `ADD.B D1,D0`.
- Add generated-exec coverage for byte register-to-register add.
- Verify `$00067E` moves forward in Metal Slug generated C.

Near follow-ups:

- Add byte/word/long helpers for arithmetic flags instead of ad hoc emitted flag code.
- Replace narrow condition handling with a tested condition-code helper table.
- Start separating instruction semantics into reusable generated helper functions when repeated emitted C becomes noisy.
- Add a tiny standalone ROM-like fixture that is closer to a hand-authored mini program than the current unit fixture.
- Begin runtime bus coverage for Neo Geo-visible regions that generated code already touches.
