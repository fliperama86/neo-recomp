# Progress Tracker

Last updated: 2026-06-18

This is the live working document for `neo-recomp`. Keep the README focused on
project orientation; update this file after each meaningful green slice.

## Current Snapshot

- Repository: `https://github.com/fliperama86/neo-recomp.git`
- Branch: `main`
- Latest pushed commit: see `git log --oneline -1` after each push
- Local validation: `ctest --test-dir build --build-config Debug --output-on-failure`
- Current test status: 6/6 passing
- Static opcode sweep: all decoder-recognized non-`UNKNOWN` opcodes emit without
  an unsupported-dispatch stub in the current synthetic sweep
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

The last checked Metal Slug generated C reached this point in candidate `$000656`:

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

`$D101` has since been confirmed as `ADDX.B D1,D0` and is now decoded/emitted
locally with generated-exec coverage. The real `.neo` smoke needs to be rerun to
find the next Metal Slug frontier.

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

Covered by executable generated-C validation:

- control flow: direct and control-EA `JSR`/tail `JMP`, local `BRA`,
  selected `Bcc`
- system/exception-control paths: `TRAP`, `TRAPV`, `ILLEGAL`, A-line,
  F-line, `RESET`, `STOP`, `RTE`, and `RTR` are recognized. `TRAP`,
  `TRAPV`, `ILLEGAL`, A-line, F-line, and failed `CHK` now push 68000-style
  SR/PC exception frames and dispatch through the vector table; divide-by-zero
  vectors through vector 5; `RTE`/`RTR` pop stack frames back into SR/CCR and
  PC. `STOP` installs the immediate SR and yields through the runtime stop
  hook. Full privilege/device/interrupt timing remains pending.
- branches: tested `BNE`, `BEQ`, `BCC`; `BCS` emission exists for carry cases
- conditional branches: all 68000 `Bcc` condition predicates are emitted;
  generated-exec coverage includes `BNE`, `BEQ`, `BCC`, and `BMI`
- condition operations: `Scc <ea>` and `DBcc Dn,disp` share the same
  condition predicate table, with generated-exec coverage for `SMI` and `DBF`
- flags: `N`/`Z` for covered operations; `C`/`X` for tested byte subtract/extend-add paths
- data movement:
  - `MOVEQ`
  - `MOVE.B/W #imm,Dn`
  - `MOVE.W #imm,abs`
  - `MOVE.B #imm,abs`
  - `MOVE.B Dn,abs`
  - `MOVE.B (d16,An),Dn`
  - generic `MOVE.B/W/L <ea>,<ea>` paths covered so far by data-register,
    immediate, postincrement, address-indirect, and indexed destination forms
  - generic `MOVEA.L <ea>,An` paths covered so far by immediate and
    address-register sources
  - `MOVE.L An,abs`
  - generic `LEA <ea>,An` control-address loads covered so far by PC-relative
    and displacement sources
  - `MOVEM.W/L` register-list transfers covered so far by long Dn masks to/from
    address-indirect memory and predecrement register-to-memory masks
  - `MOVEP.W/L` staggered peripheral transfers covered by decode/emitter tests
  - `MOVE <ea>,CCR/SR` and `MOVE SR/CCR,<ea>` covered so far by immediate CCR
    source and absolute SR destination forms
  - `MOVE An,USP` and `MOVE USP,An` user-stack-pointer transfers
  - `LINK An,#disp` and `UNLK An`
- arithmetic/logical:
  - `ADD.W Dn,Dn`
  - generic `ADD.B/W/L <ea>,Dn` paths covered so far by Dn reads
  - generic `ADDI.B/W/L #imm,<ea>` paths covered so far by Dn destinations
  - generic `ADD.B/W/L Dn,<ea>` paths covered so far by postincrement memory
    destinations
  - generic `ADDQ.B/W/L #imm,<ea>` paths covered so far by Dn destinations
    and address-register destinations
  - generic `SUB.B/W/L <ea>,Dn` paths covered so far by Dn reads
  - generic `SUB.B/W/L Dn,<ea>` paths covered so far by postincrement memory
    destinations
  - generic `SUBI.B/W/L #imm,<ea>` paths covered so far by postincrement
    memory destinations
  - generic `SUBQ.B/W/L #imm,<ea>` paths covered so far by Dn and
    postincrement memory destinations
  - generic `CMP.B/W/L <ea>,Dn` paths covered so far by Dn and abs reads
  - generic `ADDA.W/L <ea>,An`, `SUBA.W/L <ea>,An`, and
    `CMPA.W/L <ea>,An` paths covered so far by immediate sources
  - `CMPM.B/W/L (Ay)+,(Ax)+` covered so far by word postincrement memory
    comparisons
  - `CHK.W <ea>,Dn` covered so far by immediate in-range checks, with failed
    checks vectoring through the CHK exception path in emitted C
  - `ADDX/SUBX.B/W/L` register and predecrement-memory forms, covered so far
    by generated-exec byte add-extend and word subtract-extend paths
  - `ABCD/SBCD` register and predecrement-memory BCD extend forms, covered so
    far by generated-exec register BCD arithmetic
  - generic `OR.B/W/L <ea>,Dn` and `AND.B/W/L <ea>,Dn` paths covered so far
    by data-register sources
  - generic `OR.B/W/L Dn,<ea>`, `AND.B/W/L Dn,<ea>`, and
    `EOR.B/W/L Dn,<ea>` paths covered so far by data-register and
    postincrement memory destinations
  - `MULU.W <ea>,Dn` and `MULS.W <ea>,Dn` covered so far by immediate sources
  - `DIVU.W <ea>,Dn` and `DIVS.W <ea>,Dn` covered so far by immediate sources
    and divide-by-zero exception-vector emission
  - `EXG` register exchanges covered so far by data-register pairs
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
  - `TAS <ea>` covered so far by absolute byte memory destinations
  - `CLR.B/W/L abs`
  - `CLR.B/W/L Dn`
  - generic `CLR.B/W/L <ea>` paths covered so far by `(d16,An)` and `(An)+`
  - generic `BTST/BCHG/BCLR/BSET #imm,<ea>` and `BTST/BCHG/BCLR/BSET Dn,<ea>`
    paths covered so far by abs, Dn, and postincrement memory destinations
  - generic `NEG.B/W/L <ea>` and `NOT.B/W/L <ea>` paths covered so far by Dn,
    abs, and postincrement memory destinations
  - generic `NEGX.B/W/L <ea>` paths covered so far by absolute byte memory
    destinations
  - `NBCD <ea>` covered so far by absolute byte memory destinations
  - `EXT.W Dn`, `EXT.L Dn`, and `SWAP Dn`
  - data-register `ASL/ASR/LSL/LSR/ROXL/ROXR/ROL/ROR` decode/emission,
    covered so far by generated-exec logical shift pairs
  - memory `ASL/ASR/LSL/LSR/ROXL/ROXR/ROL/ROR` word forms, covered so far by
    an absolute-memory logical shift
  - `PEA <ea>` control-address pushes covered so far by displacement sources
  - `ORI/ANDI/EORI #imm,CCR/SR` immediate status-register operations

Important caveat: this is not a complete 68000 condition-code model. `C`/`X`
and `V` are only trusted where generated-exec tests cover them.

## Recent Green Slices

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
8. Update this document.
9. Commit and push.

Immediate next slice:

- Rerun the Metal Slug `.neo` smoke with the new `$D101` support.
- Confirm `$00067E` moves forward in generated C.
- Pick the next `DC.W` frontier from the regenerated `build\mslug_recomp.c`.

Near follow-ups:

- Migrate more instruction families onto the generic EA helpers instead of adding bespoke forms.
- Add a decode/codegen legality layer so invalid source/destination EA combinations fail loudly.
- Add byte/word/long helpers for arithmetic flags instead of ad hoc emitted flag code.
- Replace narrow condition handling with a tested condition-code helper table.
- Start separating instruction semantics into reusable generated helper functions when repeated emitted C becomes noisy.
- Add a tiny standalone ROM-like fixture that is closer to a hand-authored mini program than the current unit fixture.
- Begin runtime bus coverage for Neo Geo-visible regions that generated code already touches.
