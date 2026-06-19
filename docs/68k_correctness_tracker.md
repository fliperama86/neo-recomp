# 68000 Correctness Tracker

Last updated: 2026-06-19

This is the detailed done/missing tracker for CPU-visible 68000 behavior. Keep
`docs/progress.md` as the narrative log, and update this file whenever a TDD
slice changes what is covered, partial, or missing.

## Status Key

- **Done**: implemented and covered by a passing unit/generated-exec test.
- **Partial**: some decode/emission/runtime behavior exists, but the entry is not
  complete enough to trust as correct 68000 behavior.
- **Missing**: required for a correct 68000/NeoGeo implementation and not yet
  covered by the current tests.
- **Unknown**: needs a real-ROM smoke, oracle/fuzz run, or spec-driven audit
  before it can be classified.

Current verification command:

```sh
ctest --test-dir build --output-on-failure
```

Last local verification: **7/7 passing** on 2026-06-19.

## Done and Covered

| Area | Status | Evidence | Notes |
| --- | --- | --- | --- |
| Behavior-based generated-C harness | Done | `tests/test_generated_exec.c` builds emitted C into the test and compares it with a small interpreter oracle. | This is the strongest current correctness check, but it only covers the synthetic fixture. |
| Static emission sweep for recognized opcodes | Done | `tests/test_opcode_sweep.c`; current `ctest` passes. | Scope is limited to opcodes the decoder already recognizes as non-`UNKNOWN`. It does not prove every valid 68000 encoding is decoded. |
| Decode truncation rejection | Done | `tests/test_m68k_decode.c`; decoder uses `finish_decode()` in `recompiler/src/m68k_decode.c`. | Decoded instructions whose declared byte length extends outside the ROM are rejected. |
| PC-relative `LEA` base | Done | `tests/test_m68k_decode.c`, `tests/test_generated_exec.c`; implementation in `recompiler/src/m68k_decode.c`. | The displacement base is the extension-word address, not the following instruction address. |
| `JSR`/`BSR` return push | Done | `tests/test_c_emitter.c`, `tests/test_generated_exec.c`; `emit_push_return_address()` in `recompiler/src/c_emitter.c`. | Generated code pushes the following PC as a 32-bit return address. |
| `JSR`/`BSR` tail-dispatch | Done | `tests/test_c_emitter.c`; `recompiler/src/c_emitter.c`. | Generated code dispatches to the subroutine and returns from the host function instead of continuing host-side immediately. |
| Post-`JSR`/`BSR` continuation discovery | Done | `tests/test_function_discovery.c`; `recompiler/src/function_discovery.c`. | Continuation addresses are added so `RTS` can return through the generated dispatch table. |
| `RTS` stack return dispatch | Done | `tests/test_c_emitter.c`, `tests/test_generated_exec.c`; `recompiler/src/c_emitter.c`. | `RTS` reads the return PC from `(A7)`, increments `A7` by 4, and dispatches to that PC. |
| Direct and control-EA `JMP`/`JSR` emission | Done | `tests/test_c_emitter.c`, `tests/test_m68k_decode.c`. | Static discovery is still only conservative for direct/control targets it can prove. |
| `RTE`/`RTR` basic stack pop and dispatch | Done | `tests/test_c_emitter.c`, `tests/test_generated_exec.c`; `recompiler/src/c_emitter.c`. | `RTE` is supervisor-guarded and restores SR through the generated SR helper, so returning to user mode switches from `SSP` back to `USP`. Broader interrupt recognition remains separate partial work. |
| Basic exception vector stack path | Done | `tests/test_c_emitter.c`, `tests/test_generated_exec.c`; `ng_push_exception_frame()` emitted by `recompiler/src/c_emitter.c`. | Covered for emitted trap-style paths, including user-to-supervisor stack selection before pushing SR/PC frames. Exact exception priorities and bus/address-error frames remain partial. |
| `STOP` decode/emission hook | Done | `tests/test_c_emitter.c`, `tests/test_generated_exec.c`; runtime hook `ng_m68k_stop_until_interrupt()`. | Immediate SR install, supervisor enforcement, and the runtime stop hook are covered. Basic interrupt wake from `STOP` is tracked separately below. |
| `MOVE USP` transfer emission | Done | `tests/test_c_emitter.c`, `tests/test_generated_exec.c`; state fields in `include/ngrecomp/m68k_state.h`. | Transfer instructions are emitted and guarded by supervisor-mode privilege checks. |
| Supervisor/user stack switching for emitted code | Done | `tests/test_c_emitter.c`, `tests/test_generated_exec.c`; generated `ng_set_sr()` helper in `recompiler/src/c_emitter.c`. | Full-SR writes, `STOP`, exception entry, and `RTE` now switch active `A7` between `USP` and `SSP` when the S bit changes. |
| Privilege violations for emitted privileged instructions | Done | `tests/test_c_emitter.c`, `tests/test_generated_exec.c`; generated `ng_require_supervisor()` helper in `recompiler/src/c_emitter.c`. | User-mode `STOP`, `RESET`, `RTE`, `MOVE <ea>,SR`, `MOVE USP`, and `ORI/ANDI/EORI #imm,SR` now vector through privilege-violation vector 8 with the faulting instruction PC. MC68000 `MOVE SR,<ea>` remains unprivileged; later-family `MOVE from SR` privilege is out of current MC68000 scope. |
| Basic interrupt entry from `STOP` | Done | `tests/test_c_emitter.c`, `tests/test_function_discovery.c`, `tests/test_generated_exec.c`; generated `ng_service_interrupt()` helper and runtime `ng_m68k_take_interrupt()` hook. | `STOP` now installs the immediate SR, lets the runtime arm an interrupt, accepts runtime-approved interrupts according to the current mask, stacks SR/PC on `SSP`, sets supervisor mode plus the interrupt mask, vectors through the supplied vector number, and returns through `RTE` to the post-`STOP` continuation. A masked interrupt fixture verifies no wake/frame when the level is not allowed. |
| Runtime-supplied instruction-boundary interrupts | Done | `tests/test_c_emitter.c`, `tests/test_function_discovery.c`, `tests/test_generated_exec.c`; per-instruction `ng_service_interrupt()` checks in generated C. | Generated code now polls for runtime-approved interrupts before each emitted instruction. Accepted interrupts stack the current instruction PC, vector to the handler, and `RTE` returns through discovered instruction-start continuations. |
| Runtime interrupt mask and level-7 controller | Done | `tests/test_runtime_interrupts.c`; runtime `ng_m68k_set_interrupt_level()`, `ng_m68k_clear_interrupt_level()`, and `ng_m68k_take_interrupt()`. | The default runtime now tracks an asserted IPL level/vector, inhibits levels lower than or equal to the current SR mask, and treats a lower-to-level-7 transition as a one-shot nonmaskable edge even when the current mask is 7. |
| NeoGeo cartridge interrupt source model | Done | `tests/test_runtime_interrupts.c`; runtime `ng_neogeo_request_*_interrupt()` and `ng_neogeo_ack_interrupts()`. | VBlank, timer, and reset-pending sources map to cartridge-system autovector levels 1, 2, and 3 respectively, with IRQACK-style bits selecting which pending sources to clear. Highest pending level drives the runtime IPL controller. |
| Memory-mapped `REG_IRQACK` | Done | `tests/test_runtime_interrupts.c`; runtime `ng68k_write8()`/`ng68k_write16()` handling for `$3C000C`. | Word writes to `$3C000C` and low-byte writes to `$3C000D` clear the corresponding pending NeoGeo IRQ sources through `ng_neogeo_ack_interrupts()`. |
| All 16 condition predicates available to generated code | Done | `tests/test_c_emitter.c`, `tests/test_generated_exec.c`; condition predicate helper in `recompiler/src/c_emitter.c`. | Only selected branch/condition behavior is oracle-covered; full flag correctness is partial. |

## Partial / Needs Broader Proof

| Area | Status | Current behavior | Missing before it is trusted |
| --- | --- | --- | --- |
| Opcode coverage | Partial | Current sweep proves every decoder-recognized non-`UNKNOWN` opcode emits without an unsupported stub. | A spec-driven legality matrix and/or trusted-disassembler audit for every valid 68000 opcode/addressing form. |
| Effective-address legality | Partial | Generic EA helpers and many decode/emission paths exist. | Reject every illegal source/destination EA combination with tests; verify all valid EA forms per instruction family. |
| Condition codes | Partial | `N`/`Z` and selected `C`/`X`/`V` paths are implemented where tests cover them. | Oracle/fuzz coverage for exact `X/N/Z/V/C` behavior across arithmetic, logical, shifts/rotates, BCD, compare, extend, and divide edge cases. |
| Arithmetic/divide semantics | Partial | `MULS/MULU`, `DIVS/DIVU`, `ADD/SUB/CMP`, extend arithmetic, BCD, and unary RMW paths exist. | Exact overflow, quotient/remainder, divide-by-zero, undefined/unaffected flags, and edge-case behavior against a trusted 68000 oracle. |
| `MOVEM` | Partial | Long/word register-list transfers and predecrement paths are emitted for covered cases. | Full ordering, mask, word sign-extension, and all valid EA modes verified against oracle fixtures. |
| Exception stack semantics | Partial | Basic SR/PC exception frames, supervisor stack selection, and vector dispatch are emitted for covered generated paths. | Exact vector fetch ordering, address/bus error frames, and all exception priorities. |
| Interrupt model | Partial | `STOP` and ordinary instruction boundaries can accept runtime-approved interrupts, stack a format-0 frame, update the interrupt mask, vector to a handler, and return via `RTE`. The default runtime has IPL mask, level-7-edge tracking, cartridge VBlank/timer/reset-pending source APIs, and memory-mapped `REG_IRQACK` clearing. | Interrupt priority against other exceptions, LSPC timer registers, and actual VBlank/timer scheduling. |
| Function discovery / CFG | Partial | Direct calls, instruction-start continuations, `JSR`/`BSR` continuations, `STOP` continuations, tail jumps, and a known PC-index jump-table shape are discovered conservatively. | Conditional branch basic-block splitting, indirect jump tables beyond the current pattern, data-driven targets, and robust real-ROM CFG recovery without excessive duplicate generation. |
| `g_ng_m68k.pc` | Partial | Dispatch uses function addresses and local labels; some stack frames include explicit return PCs. | Generated code does not consistently maintain architectural `PC` after each instruction or at every exception boundary. |
| Real-ROM frontier tracking | Partial | `docs/progress.md` records the last Metal Slug frontier and notes it is stale. | Rerun the Metal Slug smoke and replace stale `DC.W` frontier data with the current first failure. |
| Runtime bus boundary | Partial | Generated code only uses `ng68k_read*`/`ng68k_write*` and runtime hooks. | Correct NeoGeo memory map/device behavior in the runtime, with tests. |

## Missing for Correct 68000 Behavior

| Area | Status | Required work |
| --- | --- | --- |
| LSPC timer / VBlank scheduling | Missing | Implement LSPC timer registers, VBlank scheduling, and exception-priority rules. |
| Trace mode | Missing | Implement trace-bit behavior and trace exceptions. |
| Address/bus error precision | Missing | Handle odd word/long accesses and unmapped/bus-error cases with correct exception behavior if required by real software. |
| Full oracle/fuzz harness | Missing | Add a trusted 68000 reference runner or generated randomized fixtures for every implemented instruction family, size, EA mode, and edge case. |
| Cycle timing / prefetch | Missing | No cycle timing or prefetch behavior is modeled. Only add this once functional correctness and real-ROM needs demand it. |
| NeoGeo hardware integration | Missing | Implement/test program ROM mapping, RAM, palette RAM, backup/system RAM, watchdog, inputs, sound latch/Z80 communication, BIOS/system registers, VRAM-facing ports, VBlank/level interrupts, and 24-bit address wrapping. |
| Real game boot checkpoint | Missing | Metal Slug has not yet reached a documented boot/attract-mode checkpoint in this tracker. |

## Cross-Repo Reference Notes

A local contrast against `/Users/dudu/Projects/references/segagenesisrecomp` lives in
[`segagenesisrecomp_contrast.md`](segagenesisrecomp_contrast.md). Main actionable
lessons: add centralized codegen diagnostics, a post-decode legality validator,
game-TOML-driven discovery metadata, dispatch/jump-table audits, and a trusted
68000 oracle harness. The reference's Genesis-specific `A7 = SSP` simplification
should **not** replace the real supervisor/user stack switching tracked here.

## Ordered TDD Backlog

1. **LSPC timer / VBlank scheduling**: add tests for timer registers, VBlank
   scheduling, and interrupt priority against other exceptions.
2. **Trusted oracle/fuzz harness**: compare generated C against an external or
   embedded 68000 reference for per-instruction semantic edge cases.
3. **Real-ROM smoke refresh**: regenerate Metal Slug C, capture the current first
   frontier, add a regression, and update this tracker.
4. **Codegen diagnostics/fail-on-unsupported**: make unsupported decoded
   behavior and unresolved dynamic control flow machine-checkable before smoke.
5. **NeoGeo bus slices**: implement one hardware-visible region at a time with a
   failing runtime/generated-exec test first.

## Update Rules

- Move an item to **Done** only with a named passing test or generated-exec
  oracle fixture.
- Keep **Partial** when code exists but coverage is narrow or architectural
  behavior is not complete.
- Add the test name, source file, and behavioral invariant whenever a row moves.
- After each green slice, update both this tracker and `docs/progress.md` before
  committing.
