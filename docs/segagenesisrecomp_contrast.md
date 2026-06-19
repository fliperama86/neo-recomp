# segagenesisrecomp Contrast Notes

Last updated: 2026-06-19

Reference inspected locally at:

```text
/Users/dudu/Projects/references/segagenesisrecomp
```

This document records implementation lessons to compare against `neo-recomp`.
It is not a source-copy plan. The reference repo is Genesis-specific and uses a
different runtime, game-disassembly pipeline, and license context; treat it as a
set of patterns to adapt deliberately.

## High-Level Contrast

| Area | `neo-recomp` now | `segagenesisrecomp` reference | Takeaway for `neo-recomp` |
| --- | --- | --- | --- |
| Target | Neo Geo P-ROM / 68000 bring-up. | Sega Genesis 68000 with playable Sonic milestones. | Keep NeoGeo hardware/runtime design separate; do not import Genesis assumptions. |
| CPU correctness strategy | Synthetic generated-C oracle fixture plus unit tests and opcode emission sweep. | L1 decoder tests, synthetic decoder tests, validator tests, and L3 per-function oracle against `clown68000`. | Our next large harness should be a trusted 68000 oracle/fuzz layer, not only a hand-written mini interpreter. |
| Function discovery | Conservative static seeds from cartridge entry, direct call targets, continuations, tail jumps, and one PC-index jump-table shape. | Static worklist plus disassembly-generated seeds, labels, jump tables, code-address oracles, protected ranges, blacklists, dispatch-miss feedback, and interior-label auditing. | Add machine-checkable CFG/discovery inputs before expecting real-ROM boot progress. |
| Unsupported behavior visibility | Current tests catch recognized-opcode emission stubs; generated code logs dispatch misses. | Centralized codegen diagnostics count unsupported/TODO paths and can fail generation; dispatch audit classifies dynamic jump sites. | Add a centralized diagnostic/fail-on-unsupported layer. |
| Opcode legality | Tracker marks this partial; decoder has ad hoc legality checks. | Has `m68k_validator.c` as a post-decode MC68000 legality gate. Coverage doc still calls it non-exhaustive. | Build a spec-driven legality layer before broad real-ROM scanning. |
| Runtime model | Small runtime API boundary only; no real NeoGeo bus yet. | Full runner with Genesis bus, VDP/audio/input, cooperative fibers, VBlank integration, traces, and optional enhancements. | Our runtime needs an equivalent NeoGeo bus/interrupt loop, but only after CPU-visible semantics are stable. |
| `RTS`/subroutine model | Current generated code now uses stack-backed return dispatch: `JSR`/`BSR` push return PC, `RTS` pops PC and dispatches. | Uses C calls for known `JSR` plus explicit return push/pop and special propagation for stack-skip idioms like `addq.l #4,sp; rts`. | Keep our stack-backed model for architectural correctness; later add tests for stack-skip idioms instead of relying on host call return. |
| Supervisor/user stacks | Active `SSP`/`USP` switching is now covered for full-SR writes, exception entry, `STOP`, and `RTE`. | Runtime state comments assume `A7 = SSP` and `USP` is a separate shadow; good enough for Genesis game paths, not a complete user-mode model. | Do **not** copy this simplification; keep the official MC68000 stack model. |
| Interrupts/STOP | Runtime-approved `STOP` wake and instruction-boundary interrupts are covered; level-7/IPL nuance and NeoGeo IRQ sources remain open. | Runtime has cooperative VBlank and STOP yield hooks, but many semantics are tuned to Genesis/Sonic. | Use it as a design reference for runtime scheduling, not as a 68000 spec oracle. |
| Cycle timing | Missing. | Emits estimated per-instruction cycles and uses a cycle accumulator for VBlank/audio timing. | Add only after semantic correctness; make cycle estimates separately testable. |
| Game config | Current `--game` is accepted but not actually parsed into discovery/runtime metadata. | TOML config drives output prefix, disassembly discovery files, RAM layout, jump tables, protected ranges, code-address oracles, and per-game hooks. | `neo-recomp` should parse `games/*.toml` for real metadata before serious real-ROM smoke work. |
| Tracking docs | Added `docs/68k_correctness_tracker.md`; progress and finish plan exist. | Has `COVERAGE.md`, `PRINCIPLES.md`, debug docs, and generated dispatch audits. | Keep our tracker, and add machine-generated audits as implementation catches up. |

## Reference Patterns Worth Adapting

1. **Centralized codegen diagnostics**
   - Reference files: `recompiler/src/codegen_diag.{h,c}`.
   - Useful behavior: every TODO/comment-only path records a structured event;
     generation can summarize or fail on unsupported behavior.
   - NeoGeo adaptation: add a diagnostics module for `ng_emit_c` and make real-ROM
     smoke fail loudly on unsupported decoded behavior, invalid stores, branch
     targets that cannot be resolved, and dynamic control-flow gaps.

2. **Post-decode legality validation**
   - Reference files: `recompiler/src/m68k_validator.{h,c}`.
   - Useful behavior: separates permissive decode from MC68000 legality decisions.
   - NeoGeo adaptation: build a stricter, spec-driven 68000 legality matrix for
     instruction size, source EA, destination EA, and CPU-family scope.

3. **Multi-level validation**
   - Reference tests include decoder fixtures, validator fixtures, diagnostics
     tests, and a per-function oracle harness against `clown68000`.
   - NeoGeo adaptation: add an external/trusted 68000 oracle harness for generated
     C, then move individual instruction families out of "Partial" only after
     oracle-backed edge-case tests.

4. **Discovery data pipeline**
   - Reference TOML can merge disassembly-generated seeds, subroutine labels,
     jump tables, protected ranges, and code-address lists.
   - NeoGeo adaptation: parse `games/*.toml`, then support generated discovery
     files for known BIOS/game disassembly sources or analysis outputs.

5. **Dispatch and interior-label audits**
   - Reference generated dispatch exposes table accessors and writes a dispatch
     audit for jump sites.
   - NeoGeo adaptation: add a generated dispatch audit that classifies direct,
     computed, table, external, and unresolved targets. Dispatch misses should be
     test failures during smoke runs, not quiet runtime events.

6. **Cycle accounting as a separate signal**
   - Reference emits cycle increments and runtime checks VBlank timing.
   - NeoGeo adaptation: after semantic parity, add a cycle model with explicit
     tests and docs. Do not mix cycle correctness with instruction semantic tests.

## Reference Patterns Not To Copy Directly

- **Genesis bus/VDP/Z80/audio hooks**: useful shape, wrong hardware.
- **`A7 = SSP` simplification**: not a complete 68000 user/supervisor model.
- **Game-specific Sonic heuristics**: examples are useful, but NeoGeo needs BIOS,
  cartridge, and game-specific metadata.
- **Generated-output edits or manual patches**: keep `neo-recomp` generated output
  reproducible from source/tests/config.
- **License-sensitive code reuse**: use ideas only unless we explicitly verify
  license compatibility for copied code.

## Backlog Changes Inferred From This Contrast

These reinforce items already in `68k_correctness_tracker.md`:

1. Add **full IPL / NeoGeo interrupt integration** beyond the current
   runtime-supplied polling: level-7/IPL details, interrupt priority, and
   NeoGeo IRQ sources.
2. Add **codegen diagnostics/fail-on-unsupported** before the next serious
   real-ROM smoke loop.
3. Add **game TOML parsing** so `--game` drives real metadata instead of being a
   printed path.
4. Add a **post-decode legality validator** and route invalid encodings to loud
   diagnostics/trap behavior.
5. Add a **trusted oracle harness** for CPU semantics and per-function parity.
6. Add **dispatch/jump-table audits** and treat dispatch misses as graph failures.
