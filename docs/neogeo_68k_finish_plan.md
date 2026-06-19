# NeoGeo 68000 Finish Plan

Last updated: 2026-06-19

## TL;DR

68000 opcode decode/emission coverage is complete for the current decoder model.
What remains is making the generated 68000 behavior faithful inside the NeoGeo
system: exact instruction semantics, exception/privilege behavior, interrupts,
NeoGeo bus/peripheral integration, and real-ROM smoke validation. The concrete
done/partial/missing checklist lives in [`68k_correctness_tracker.md`](68k_correctness_tracker.md).

## Phase 1 — CPU Correctness Harness

Goal: prove emitted C matches a trusted 68000 reference, not just that it emits.

Tasks:

- Add broad oracle tests against a trusted 68000 interpreter/reference.
- Generate randomized instruction fixtures for:
  - every decoded instruction family
  - all supported operand sizes
  - all valid effective-address modes
  - edge cases around zero, sign bits, overflow, carry, extend, and alignment
- Compare generated execution against the oracle for:
  - data/address registers
  - `SR`/`CCR` flags
  - `PC`/control flow
  - memory reads/writes
  - exception vectors and stack frames

Exit criteria:

- Every implemented instruction family has oracle-backed coverage.
- Randomized/generated fixtures can be run repeatedly without semantic drift.

## Phase 2 — Exact 68000 Semantics

Goal: make instruction behavior faithful, including all annoying edge cases.

Focus areas:

- Full `X/N/Z/V/C` correctness across all arithmetic/logical/shift operations.
- `DIVU`/`DIVS` divide-by-zero, overflow, quotient/remainder, and flag behavior.
- `CHK`, `TRAPV`, `ILLEGAL`, A-line, and F-line trap details.
- BCD instructions: `ABCD`, `SBCD`, `NBCD`.
- `MOVEM` ordering, predecrement masks, and word sign-extension behavior.
- `TAS`, `NEGX`, shifts/rotates, extend arithmetic, and condition-code edge cases.
- Address alignment/address-error behavior if real software requires it.

Exit criteria:

- Instruction semantic fuzz/oracle tests pass for every decoded family.
- Known edge-case fixtures are locked as regressions.

## Phase 3 — Privilege and Exception Model

Goal: behave like a real 68000 across user/supervisor transitions.

Tasks:

- Implement/verify `SSP`/`USP` stack switching.
- Track supervisor bit behavior correctly.
- Enforce privilege violations for privileged operations, including:
  - `STOP`
  - `RESET`
  - `RTE`
  - `MOVE <ea>,SR` (`MOVE SR,<ea>` is unprivileged on the MC68000)
  - `MOVE USP`
  - immediate SR operations
- Verify exception stack-frame layout and return behavior.
- Broaden interrupt entry beyond runtime-supplied instruction-boundary polling,
  the basic IPL controller, and the cartridge IRQ source API by wiring
  memory-mapped IRQACK/LSPC timer behavior and priority interactions.

Exit criteria:

- Exception, privilege, and stack-switch tests pass against expected 68000 behavior.

## Phase 4 — NeoGeo 68000 Bus/System Integration

Goal: make 68000 code interact correctly with the NeoGeo hardware map.

Tasks:

- Implement/verify key memory regions:
  - program ROM
  - work RAM
  - palette RAM
  - backup/system RAM if needed
- Implement/verify memory-mapped hardware behavior:
  - watchdog
  - input ports
  - sound latch / Z80 communication
  - BIOS/system registers
  - video/VRAM-facing ports used by boot/game code
- Implement interrupt behavior:
  - vector fetches
  - IRQ mask interaction
  - VBlank/level interrupts
  - `STOP` wake on interrupt
- Preserve 24-bit 68000 address behavior and wrapping rules.

Exit criteria:

- BIOS/game startup code can execute through core hardware interactions without
  fake dispatch misses for expected 68000/system behavior.

## Phase 5 — Real-ROM Smoke Frontier

Goal: use a real game, currently Metal Slug, as the integration driver.

Loop:

1. Generate Metal Slug C.
2. Compile generated C.
3. Run a boot/trace smoke test.
4. Capture the first failure/frontier.
5. Fix the CPU or system gap.
6. Add a regression test.
7. Commit and push.

Exit criteria:

- Metal Slug reaches a stable boot/attract-mode checkpoint.
- New real-ROM failures are reproducible and covered by tests before fixing.

## Phase 6 — Hardening and Performance

Goal: keep the implementation maintainable and fast after correctness is stable.

Tasks:

- Clean up generic instruction helpers.
- Reduce duplicated emitter logic.
- Split validation into quick CI tests and longer fuzz/smoke suites.
- Document remaining caveats clearly.
- Optimize hot emitted code patterns only after semantic correctness is locked.

Exit criteria:

- CI has fast regressions.
- Long-running oracle/smoke tests are available for deeper validation.
- Generated C remains readable enough to debug real-ROM frontiers.

## Recommended Order

1. CPU oracle/fuzz harness.
2. CPU semantic fixes.
3. Exception and privilege model.
4. NeoGeo memory map, interrupts, and bus behavior.
5. Metal Slug smoke loop.
6. Performance and cleanup.

## Current Status

- Detailed checklist: [`68k_correctness_tracker.md`](68k_correctness_tracker.md).
- Current decoder-recognized opcode emission coverage: complete.
- Current tests: `ctest --test-dir build --output-on-failure`.
- Current caveat: completion here means decode/emission coverage, not yet
  cycle-perfect or fully hardware-integrated NeoGeo behavior.
