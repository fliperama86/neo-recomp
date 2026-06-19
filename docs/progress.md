# Progress Tracker

Last updated: 2026-06-19

This is the live working document for `neo-recomp`. Keep the README focused on
project orientation; update this file after each meaningful green slice.

## Current Snapshot

- Repository: `https://github.com/fliperama86/neo-recomp.git`
- Branch: `main`
- Latest pushed commit: see `git log --oneline -1` after each push
- Local validation: `ctest --test-dir build --build-config Debug --output-on-failure`
- Current test status: 10/10 passing
- Detailed CPU correctness tracker: [`docs/68k_correctness_tracker.md`](68k_correctness_tracker.md)
- Reference contrast: [`docs/segagenesisrecomp_contrast.md`](segagenesisrecomp_contrast.md)
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

For a concrete done/partial/missing checklist, see [`68k_correctness_tracker.md`](68k_correctness_tracker.md).

Covered by executable generated-C validation:

- control flow: direct and control-EA `JSR`, `BSR`, `RTS` stack-return
  dispatch, a reference-inspired stack-manipulating return regression, tail
  `JMP`, local `BRA`, and selected `Bcc`
- system/exception-control paths: `TRAP`, `TRAPV`, `ILLEGAL`, A-line,
  F-line, `RESET`, `STOP`, `RTE`, and `RTR` are recognized. `TRAP`,
  `TRAPV`, `ILLEGAL`, A-line, F-line, failed `CHK`, and divide-by-zero now
  push 68000-style SR/PC exception frames and dispatch through the vector
  table; `RTE`/`RTR` pop stack frames back into SR/CCR and
  PC. Full-SR writes, `STOP`, exception entry, and `RTE` use the
  generated SR helper to switch active `A7` between `USP` and `SSP` when the
  S bit changes. User-mode privileged operations now vector through
  privilege-violation vector 8 with the faulting instruction PC. `STOP`
  installs the immediate SR, yields through the runtime stop hook when
  executed in supervisor mode, and can wake through a runtime-approved
  interrupt that returns via `RTE`. Full device/interrupt timing remains
  pending.
- branches: tested `BNE`, `BEQ`, `BCC`; `BCS` emission exists for carry cases
- conditional branches: all 68000 `Bcc` condition predicates are emitted;
  generated-exec coverage includes `BNE`, `BEQ`, `BCC`, and `BMI`
- condition operations: `Scc <ea>` and `DBcc Dn,disp` share the same
  condition predicate table, with generated-exec coverage for `SMI` and `DBF`
- flags: `N`/`Z` for covered operations; `C`/`X` for tested byte
  subtract/extend-add paths and register-count-zero shift/rotate cases
- data movement:
  - `MOVEQ`
  - `MOVE.B/W/L #imm,Dn`
  - `MOVE.W #imm,abs`
  - `MOVE.B #imm,abs`
  - `MOVE.B Dn,abs`
  - `MOVE.B (d16,An),Dn`
  - generic `MOVE.B/W/L <ea>,<ea>` paths covered so far by data-register,
    immediate, postincrement, address-indirect, and indexed destination forms
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
    list
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
  - `CHK.W <ea>,Dn` covered so far by immediate in-range checks, with failed
    checks vectoring through the CHK exception path in emitted C
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
  - `MULU.W <ea>,Dn` and `MULS.W <ea>,Dn` covered so far by immediate sources
  - `DIVU.W <ea>,Dn` and `DIVS.W <ea>,Dn` covered so far by immediate sources,
    divide-by-zero exception-vector emission, signed and unsigned
    quotient-overflow cases that leave the destination operand unchanged, and
    required `X/V/C` behavior on overflow
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
    covered so far by generated-exec logical shift pairs and zero-count
    register-count flag cases
  - memory `ASL/ASR/LSL/LSR/ROXL/ROXR/ROL/ROR` word forms, covered so far by
    an absolute-memory logical shift
  - `PEA <ea>` control-address pushes covered so far by displacement and
    PC-relative sources, including extension-word PC base calculation and CCR
    preservation
  - `ORI/ANDI/EORI #imm,CCR/SR` immediate status-register operations

Important caveat: this is not a complete 68000 condition-code model. `C`/`X`
and `V` are only trusted where generated-exec tests cover them.

## Recent Green Slices

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

- Connect the covered frame/scanline LSPC timer model to generated CPU cycle
  accounting or host pacing.
- Add priority tests for timer/VBlank interactions with CPU-generated
  exceptions once the next exception-priority slice is selected.

Real-ROM smoke remains a near follow-up once a local `.neo` input is available.

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
