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
| CPU correctness strategy | Synthetic generated-C oracle fixture plus unit tests and opcode emission sweep. Recent slices cover exception stack/trace behavior and generated-exec `MOVEP` transfer semantics. | L1 decoder tests, synthetic decoder tests, validator tests, and L3 per-function oracle against `clown68000`; current local source implements more CPU families than its older `COVERAGE.md` audit claims. | Our next large harness should be a trusted 68000 oracle/fuzz layer, not only a hand-written mini interpreter. |
| Function discovery | Conservative static seeds from cartridge entry, direct call targets, continuations, tail jumps, and one PC-index jump-table shape. | Static worklist plus disassembly-generated seeds, labels, jump tables, code-address oracles, protected ranges, blacklists, dispatch-miss feedback, and interior-label auditing. | Add machine-checkable CFG/discovery inputs before expecting real-ROM boot progress. |
| Unsupported behavior visibility | Checked emission reports unsupported/decode failures through `NgEmitDiagnostics`; generated code logs runtime dispatch misses; a dispatch audit now classifies direct, computed, and PC-index jump-table sites and can fail smoke runs via `--fail-on-dispatch-gaps`. | Centralized codegen diagnostics count unsupported/TODO paths and can fail generation; dispatch audit classifies dynamic jump sites. | Keep broadening unresolved dynamic control-flow classification and interior-label checks. |
| Opcode legality | Tracker marks this partial; an initial `m68k_validate.c` now rejects selected illegal post-decode forms. | Has `m68k_validator.c` as a post-decode MC68000 legality gate. Coverage doc still calls legality non-exhaustive. | Broaden our validator into a spec-driven legality layer before broad real-ROM scanning. |
| Runtime model | Small runtime API boundary only; no real NeoGeo bus yet. | Full runner with Genesis bus, VDP/audio/input, cooperative fibers, VBlank integration, traces, and optional enhancements. | Our runtime needs an equivalent NeoGeo bus/interrupt loop, but only after CPU-visible semantics are stable. |
| `RTS`/subroutine model | Current generated code uses stack-backed return dispatch: `JSR`/`BSR` push return PC, `RTS` pops PC and dispatches. A generated-exec regression covers a stack-skip idiom (`ADDQ.L #4,A7; RTS`). | Uses C calls for known `JSR` plus explicit return push/pop and special propagation for stack-skip idioms like `addq.l #4,sp; rts`. | Keep our stack-backed model for architectural correctness and retain stack-mutation regressions instead of relying on host call return. |
| Supervisor/user stacks | Active `SSP`/`USP` switching is now covered for full-SR writes, exception entry, `STOP`, and `RTE`. | Runtime state comments assume `A7 = SSP` and `USP` is a separate shadow; good enough for Genesis game paths, not a complete user-mode model. | Do **not** copy this simplification; keep the official MC68000 stack model. |
| Interrupts/STOP | Runtime-approved `STOP` wake, instruction-boundary interrupts, a basic IPL/level-7-edge runtime controller, cartridge VBlank/timer/reset-pending source APIs, and memory-mapped `REG_IRQACK` clearing are covered; timer/VBlank scheduling remains open. | Runtime has cooperative VBlank and STOP yield hooks, but many semantics are tuned to Genesis/Sonic. | Use it as a design reference for runtime scheduling, not as a 68000 spec oracle. |
| Cycle timing | Missing. | Emits estimated per-instruction cycles and uses a cycle accumulator for VBlank/audio timing. | Add only after semantic correctness; make cycle estimates separately testable. |
| Game config | `--game` now parses `[functions].entry`, `[functions].extra`, `[game].discovery_files`, and manual `[[jump_table]]` metadata as mergeable discovery inputs. | TOML config drives output prefix, disassembly discovery files, RAM layout, jump tables, protected ranges, code-address oracles, and per-game hooks. | Keep expanding `games/*.toml` beyond discovery inputs before serious real-ROM smoke work. |
| Tracking docs | Added `docs/68k_correctness_tracker.md`; progress and finish plan exist. | Has `COVERAGE.md`, `PRINCIPLES.md`, debug docs, and generated dispatch audits. | Keep our tracker, and add machine-generated audits as implementation catches up. |

## 2026-06-19 Follow-Up Contrast

The reference audit is useful, but it is not a drop-in checklist for our current
CPU work. Fact-checked local files show two different strengths:

- `segagenesisrecomp/COVERAGE.md` is stale for some CPU families: current
  local source has codegen cases for `MOVEP`, `CHK`, `ABCD`/`SBCD`/`NBCD`,
  `TRAPV`, `RTR`, `RESET`, and `ILLEGAL`/A-line/F-line paths. It is still not
  equivalent to our CPU work because the reference runtime routes traps through
  loud abort hooks rather than architectural SR/PC exception frames, and its
  `STOP`/interrupt behavior is Genesis/Sonic scheduling glue rather than our
  tested interrupt-frame wake/resume model.
- The reference is ahead on **project scaffolding**: `game_config.c`, generated
  dispatch audits, disassembly-sourced seeds/jump tables, interior-label checks,
  and L3 oracle tests. We have now adapted discovery-file seed merging plus
  manual jump-table metadata, but protected ranges, code-address oracles, and L3
  oracle parity remain actionable because they reduce real-ROM guesswork.
- The reference README advertises a stack-skip fix for `addq.l #4,sp; rts`
  because its generated calls can behave like host calls. Our stack-backed
  `JSR`/`RTS` model now has a generated-exec regression proving the
  architectural SP mutation drives the return target.

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
   - NeoGeo adaptation: `[game].discovery_files` now merges additional seed TOML
     files and `[[jump_table]]` metadata now seeds abs32/pcrel16/BRA table
     targets; next adapt protected ranges and code-address oracles for known
     BIOS/game disassembly sources or analysis outputs.

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

1. Add **LSPC timer / VBlank scheduling** beyond the current runtime-supplied
   polling/basic IPL/source APIs and `REG_IRQACK` clearing: timer registers,
   VBlank/timer scheduling, and interrupt priority.
2. Keep extending the **dispatch/jump-table audit** beyond the current
   direct/computed/PC-index classifications and add interior-label checks before
   the next serious real-ROM smoke loop.
3. Expand **game TOML parsing** beyond function/discovery-file/jump-table
   metadata so `--game` drives protected-range, code-address, and runtime
   metadata.
4. **Broaden the post-decode legality validator** and route invalid encodings
   to loud diagnostics/trap behavior.
5. Add a **trusted oracle harness** for CPU semantics and per-function parity.
6. Broaden **dispatch/jump-table audits** and treat remaining dispatch misses as graph failures.
