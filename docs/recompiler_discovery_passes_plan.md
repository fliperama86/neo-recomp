# Recompiler Discovery Passes Plan

Last updated: 2026-07-01

> Revised after a Codex review pass. Corrections are tagged inline with
> "(review)": the discovery model emits instruction labels (not just function
> entries), `[dispatch].runtime` sites are heterogeneous, `bank:*` needs bank
> identity in the address model, `is_probable_function_target` is a weak filter,
> candidate truncation is not yet a hard failure, and the `[[state_table]]` /
> `[[record_format]]` schemas needed explicit bounds/stride keys.

## Live status

Use this section as the running tracker. Update it whenever a phase, pass, or
coverage checkpoint changes.

| Item | Status | Current result |
| --- | --- | --- |
| Phase 0, safety net | **Done** | `--emit-discovery-set`, golden snapshots, superset checker, and hard discovery-truncation failure are in place. |
| Phase 0.5, oracle ROM | **Done** | `games/oracle/` now contains the source oracle, linker script, and manifest. `scripts/build_oracle_fixture.py` builds deterministic P-ROM/`.neo` outputs and validates exported function/relocation truth against `m68k-elf` when available. `test_oracle_discovery` and `test_oracle_fixture` enforce completeness, soundness, code-vs-data relocations, banked records, and trace-import coverage. |
| Pass 1, linked state-table scanner | **Done** | `[[state_table]]` parser/scanner landed. The `$000B92..$000E8E` mode/substate table cluster in `games/mslug.toml` is now one descriptor instead of three `[[jump_table]]` blocks. |
| Pass 2, tagged / fixed record scanner | **Done** | `[[record_format]]` parser/scanner landed. Tagged streams, fixed callback fields, object-vector slices, and exact single-longword representatives are collapsed into 12 descriptors. Remaining jump tables are branch/inline/script dispatch shapes. |
| Pass 3, object dispatcher recognizer | **Done** | `[[dispatcher]]` parser/config landed for object-state install/spawn constants, and the audit derives `$0006C8`/`$000FDE`. Direct PC-index branch tables, PC-index JSR branch tables, self-overwriting static-index JSR tables, repeated inline-code PC-index tables, mixed branch/body PC-index tables, spawn-helper wrappers, repeated direct-dispatch stubs, and fixed-stride `[[routine_table]]` code slots with optional shared-tail fallthrough are now classified structurally, removing 252 table-slot `extra` seeds, 26 manual `[dispatch].runtime` entries, 4 exact callback-slot ranges, and 18 redundant `[[jump_table]]` declarations while keeping the golden set as a strict superset. |
| Pass 4, bank-aware pointer discovery | **Done** | `scan = ["bank:*"]` iterates derived physical banks or explicit `[[bank]]` mappings, and `scan = ["bank:N"]` targets one bank for `[[record_format]]` and `[[state_table]]`. Bank identity is threaded through discovery de-duplication, worklist scanning, `--emit-discovery-set` (`bank:N 0xADDR` rows), dispatch audit checks, and generated symbol names (`ng_func_bNNN_ADDR`) for banked duplicates. Fixed-region behavior is unchanged. Runtime bank-register dispatch is future work outside this static discovery plan. |
| Pass 5, diagnostics / residual split | **Done** | Manual residual seeds moved from `games/mslug.toml` into `games/mslug.residual.toml` via `[game].discovery_files`; golden discovery is unchanged. `--emit-dispatch-suggestions` emits generic TOML suggestions for audit gaps. `scripts/generate_residual_toml.py` regenerates residual TOML from set differences. Runtime dispatch misses can now be logged as JSONL via `ng_neogeo_set_dispatch_miss_log_path()` or `NG_NEO_DISPATCH_MISS_LOG` and converted to TOML suggestions with `scripts/runtime_miss_suggestions.py`. Execution PC traces can be captured with `scripts/mame_trace_capture.py`, diffed against discovery sets with `scripts/trace_pc_residual.py`, and covered by `test_trace_tools`; Phase 0.5 now supplies a reproducible oracle source fixture plus exported function/relocation truth and trace-import validation. |

Latest checkpoint, 2026-07-01:

- `ctest --test-dir build --output-on-failure`: 19/19 passed.
- `scripts/check_mslug_discovery_golden.sh`: passed.
- Explicit `[[bank]]` metadata is covered by parser, P-ROM mapping, and
  bank-targeted discovery tests.
- PC-index JSR branch tables, self-overwriting static-index JSR tables, mixed
  branch/body PC-index tables, spawn-helper wrappers, repeated direct-dispatch
  stubs, routine tables, stage 2 object-vector callback expansion, and trace
  diagnostic tools are covered by tests.
- Golden discovery set: 63,854 addresses.
- Current discovery set: 64,412 addresses.
- Current discovery additions over golden: 558 addresses.
- Dispatch audit gaps: not worse (`missing_direct=0`, `computed=0`,
  `table_missing=0`).
- `games/mslug.toml`: 148 lines.
- `games/mslug.residual.toml`: 541 lines.
- Current structural descriptors: 1 `[[state_table]]`, 12
  `[[record_format]]`, 2 `[[routine_table]]`, 1 `[[dispatcher]]`.
- Current `[[jump_table]]` blocks: 2.
- Current `[functions].extra` entries in the manifest: 0.
- Current residual `[functions].extra` entries: 434.
- Current `[dispatch].runtime` entries: 34, with `$0006C8` and `$000FDE`
  derived from `[[dispatcher]]`, 18 direct PC-index branch-table dispatches
  classified as jump tables, 1 PC-index JSR branch-table dispatch classified as
  a jump table, 3 self-overwriting static-index JSR dispatches classified as
  jump tables, 4 repeated inline-code PC-index dispatches classified as jump
  tables, 4 mixed branch/body PC-index tables no longer needing explicit
  `[[jump_table]]` blocks, 1 repeated direct-dispatch stub table no longer
  needing an explicit `[[jump_table]]` block, 2 fixed-code routine regions
  no longer abusing `[[jump_table]] format = "bra16"`, and the
  `$0E80C8..$0E80D4` object-vector callback group covering the stage 2
  `$0619E4` runtime miss.

Optional follow-up: use trace capture plus `trace_pc_residual.py` to mine the
remaining residual families. The oracle track is now available as the
ground-truth regression suite for future recognizer changes.

## TL;DR

Original baseline: `games/mslug.toml` had grown to **1614 lines**:
~700 hand-enumerated
`[functions].extra` addresses, ~80 mostly single-representative
`[[jump_table]]` blocks, and a 60-entry `[dispatch].runtime` list. It grows by
hand every time a runtime dispatch miss is investigated and pasted back in.

N64Recomp is the benchmark. It (and the 68000-based `segagenesisrecomp`) stays
compact because **a machine pass discovers the bulk** from ground-truth metadata
(ELF symbols/relocations for N64Recomp; a full `s1disasm` listing plus Python
extractors for `segagenesisrecomp`). `neo-recomp` has no symbol table and no
ground-truth disassembly, so the same compactness has to come from
**structural recognition of the patterns** that currently get hand-enumerated.

This plan refactors discovery into five N64Recomp-style passes so that
`mslug.toml` becomes a compact, high-level manifest and the recompiler does the
discovery:

1. Linked state-table scanner
2. Tagged / fixed record scanner
3. Object dispatcher recognizer
4. Bank-aware data/code pointer discovery
5. Dispatch-miss feedback as diagnostics, not permanent config growth

The hard guardrail throughout: the set of discovered functions must stay a
**superset** of what today's `mslug.toml` produces. The 1614 lines encode
hard-won coverage; no refactor step may drop a currently-discovered address.

To make the passes measurable against *truth* (not just against today's
hand-built config), Phase 0.5 builds a small Neo Geo "oracle ROM" from source,
whose linker symbol/relocation tables are exactly the ground truth a commercial
cart never ships. That manufactures the input N64Recomp and segagenesisrecomp
are simply handed.

---

## Problem: why the manifest grows

Discovery today (`recompiler/src/function_discovery.c`) is good at *local*
control flow: from each seed it sweeps up to 64 instructions
(`scan_function_candidate`) and follows direct calls/branches, call-return
continuations, a few PC-indexed/static-indexed jump-table shapes
(`m68k_analyze.c`), and two object idioms (`is_task_state_store`,
`is_task_spawn_call`). The transitive closure over `out->addrs` makes that a
real worklist.

What it cannot do is recognize *data-defined* control flow at structural scale.
Metal Slug reaches most code through:

- **Linked mode/substate tables** consumed by the `$000FC6`/`$000FDE`
  dispatcher through `A6+$70`, with `$FFFFFFFF` chain sentinels and next-table
  pointers (see `mslug.toml:1005-1016`, plus the hand-split siblings at
  `:988-1003`).
- **Banked object/action records**: fixed-width records (strides `0x0A`,
  `0x14`, `0x1E`) and tag-`0x0800` streams that embed a callback longword at a
  known offset (`:878-906`, `:1236-1540`). Today each record block, and even
  many *single* embedded callbacks, is its own 4-line `[[jump_table]]`
  (`:1332`, `:1342`, `:1352`, `:1362`, `:1372`, ...).
- **Object-state installs**: `LEA target,A1; MOVE.L A1,(A6+$70)` and
  `LEA target,A1; JSR <spawn-helper>` idioms whose targets get dispatched later
  (hundreds of the `extra` entries, e.g. `:263-335`).

Because none of those are recognized as *families*, each newly reached address
is added individually. The result is the 1614-line file and a manual loop:

```
run smoke (scripts/mslug) -> "dispatch miss at $X" (ng_log_dispatch_miss)
   -> investigate $X by hand -> append address/table to mslug.toml -> repeat
```

### How the benchmark avoids this

| Project | Ground truth | How the manifest stays small |
| --- | --- | --- |
| N64Recomp | ELF symbols + relocations | Recompiler reads symbols/relocs; TOML carries structure/overrides only. |
| segagenesisrecomp | `s1disasm` listing (~7300 labels) | Python extractors machine-generate `*.disasm_*.toml`; `game.toml` carries only "IRREDUCIBLE residual entries" and an explicit goal that the residual "shrinks to zero as extractors improve" (`sonicthehedgehog/game.toml:189-269`). |
| **neo-recomp (target)** | **none (raw P-ROM); manufacture our own** | **Structural passes recover the families; a purpose-built oracle ROM (Phase 0.5) supplies symbol/reloc truth for validation; a machine-generated residual file holds what's left; an optional MAME trace gives a ground-truth seed set.** |

The honest consequence: passes 1-4 collapse the bulk, but without a symbol
table the residual will not reach exactly zero by static analysis alone. Pass 5
makes the residual *machine-generated and regeneratable* (and proposes a
trace-derived ground truth) so it shrinks instead of being hand-grown.

---

## Target manifest shape

Original baseline (`mslug.toml`, abridged):

```toml
[functions]
extra = [ 0x0008F6, 0x000122, /* ...~700 addresses with prose comments... */ ]

[[jump_table]]            # x ~80, many of the form:
start = 0x27BB26
end   = 0x27BB2A          # one 4-byte "representative" callback
stride = 4
format = "abs32"

[dispatch]
runtime = [ 0x0006C8, 0x000FDE, /* ...60 sites... */ ]
```

After (sketch; exact keys finalized per pass below):

```toml
[program]
address_space = "m68k"
fixed_base = 0x000000
fixed_size = 0x100000
bank_window_base = 0x200000
bank_window_size = 0x100000

[[bank]]                       # Pass 4: optional non-uniform cart bank map
id = 0
offset = 0x100000              # physical P-ROM offset
size = 0x100000                # mapped bytes in the bank window

[[dispatcher]]                 # Pass 3: recognize the shape, derive the sites
kind = "object_state"          # MOVEA.L (A0),A0 ; JMP (A0) / JSR (A0) via A6+slot
state_slot = 0x70
spawn_helpers = [0x0004AE, 0x0006FE]
install_slots = [0x70, 0x3C]

[[state_table]]                # Pass 1: linked abs32 tables w/ chain sentinels
root = 0x000B92
table_start = 0x000B92         # in-window entry => next-table pointer (follow)
table_end = 0x000E8E
sentinel = 0xFFFFFFFF
follow_chain = true
target = "0x000E00-0x001B00"   # in-target entry => code target

[[record_format]]              # Pass 2 + 4: one descriptor, many regions/banks
name = "action_record"
tag = 0x0800
stride = 2                     # tagged stream: search granularity
callback_offsets = [2]
scan = ["fixed", "bank:*"]

[[record_format]]
name = "banked_object_record"
width = 0x0A                   # fixed record size (stride defaults to width)
callback_offsets = [4, 6]
sentinel = 0xFFFFFFFF
scan = ["bank:*"]

residual_seeds = "mslug.residual.toml"   # Pass 5: machine-generated, regen-able
```

Target size: **< ~150 lines** for `mslug.toml`, with the irreducible address
list moved into a regeneratable `mslug.residual.toml` whose length is the
headline coverage metric.

---

## Phase 0 - Safety net and instrumentation (do this first)

Goal: make every later step verifiable as "no coverage lost."

Tasks:

- Add `--emit-discovery-set <file>` to `recompiler/src/main.c` that writes the
  sorted, deduplicated discovered address set (it already computes
  `NgFunctionDiscovery`; just serialize `discovery.addrs`).
- Capture a golden snapshot for the *current* `mslug.toml`:
  `tests/golden/mslug_discovery.txt` (sorted addrs) and
  `tests/golden/mslug_dispatch_audit.txt` (the `ng_dispatch_audit_write`
  summary line + counts).
- Add a check (test or `scripts/` helper) that re-runs discovery and asserts the
  new set is a **superset** of the golden set (additions allowed, drops fail).
- Make candidate truncation a hard failure (review). Today only
  `game_config.truncated` aborts the run (`main.c:245-251`); `discovery.truncated`
  is merely printed (`main.c:93`). Phase 0 must add an explicit post-discovery
  abort on `discovery.truncated`, because hitting
  `NG_FUNCTION_DISCOVERY_MAX_CANDIDATES` means silently dropped coverage.
- Add a guard against garbage growth: every discovered address should at least
  satisfy `is_probable_function_target`. Note (review) this is a **weak** filter:
  it validates only the first decoded instruction (`function_discovery.c:168-181`)
  and 68k data frequently decodes as one valid instruction. It is necessary, not
  sufficient; broad scans need the stronger provenance described in Pass 2.

Exit criteria:

- Golden snapshot committed; superset check green against the unchanged toml.
- A deliberate deletion of one `extra` address makes the check fail (proves it
  works).

---

## Phase 0.5 - Ground-truth oracle ROM (parallel track)

Goal: manufacture the symbol/relocation ground truth that a commercial Neo Geo
cart never ships, so every pass is measurable against *compiler truth* instead
of against today's hand-built config.

Rationale: N64Recomp and segagenesisrecomp consume a pre-existing symbolized
artifact (ELF relocations; the `s1disasm` listing). A raw Metal Slug cart has
neither. We can produce an equivalent by building a small Neo Geo program from
source with an open toolchain (`ngdevkit` is the modern GCC-based m68k + z80
option; it emits a standard `.neo` cart plus a symbolized ELF). Because we
compile it, the linker gives us the exact function table and the
pointer-vs-data relocations for free. This does **not** make Metal Slug itself
easier (it still has no symbols); it makes the *recompiler* correct and its
gaps measurable.

Tasks:

- Stand up an `ngdevkit` (or equivalent open m68k Neo Geo) build that produces a
  `.neo` plus a symbolized ELF / map file, reproducible from source and kept
  in-repo (e.g. `games/oracle/` + `tests/oracle/`).
- Author a deliberate **torture ROM** that plants one known instance of each
  pass's target structure, so each pass has a known-answer fixture in real
  compiled 68k:
  - a linked state table with `$FFFFFFFF` chain sentinels and a next-table
    pointer (Pass 1);
  - a tagged-record stream and a fixed-width record table with the callback
    longword at a known offset (Pass 2);
  - an object dispatcher (`MOVEA.L (A0),A0; JMP (A0)` via `A6+slot`) plus
    `LEA;MOVE.L (A6+slot)` and spawn-call installs (Pass 3);
  - the same record/table replicated across two cart banks (Pass 4);
  - a routine reached only through data (no static call site), so it surfaces
    only via the trace importer (Pass 5).
- Export the ELF symbol table as the expected discovery set
  (`tests/oracle/oracle_functions.txt`) and the relocations as the expected
  pointer set, via a small `m68k-elf-nm`/`objdump`/`readelf` extractor.
- Add an oracle test that matches how discovery actually behaves (review).
  Implemented by `test_oracle_discovery` plus `test_oracle_fixture`: the former
  is a toolchain-free in-memory oracle, and the latter builds the source-backed
  `games/oracle/` fixture, compares exported function/relocation truth, runs
  `neo-recomp` on the generated `.neo`, and validates the trace importer.
  Discovery adds every reachable *instruction* start, not just function entries
  (`scan_function_candidate`, `function_discovery.c:607-609`), and the emitter
  names every such address `ng_func_%06X` / `ng_label_%06X`. So do **not** assert
  set equality with the function symbols; assert instead:
  - **completeness:** every ELF function symbol is in the discovered set (symbols
    are a required subset; no entry is missed);
  - **soundness:** every discovered address lies inside some ELF function's
    `[start, start+size)` extent (no code invented in data);
  - **pointers:** every ELF relocation that targets code resolves to a discovered
    address, and data relocations are excluded.
  Plus: each pass recovers its planted structure.
- Validate the trace importer with an oracle trace fixture whose reached PCs
  are known from the source-backed function extents. The checked-in trace uses
  MAME-style `:maincpu: AAAAAA:` lines and `test_oracle_fixture` confirms that
  `trace_pc_residual.py` reports zero missing PCs against the generated oracle
  discovery set. Running the fixture inside MAME remains unnecessary for the
  static recompiler regression suite because the trace syntax and truth set are
  already deterministic.

Exit criteria:

- An in-repo, reproducible oracle fixture with exported symbol/relocation
  ground truth. **Done:** `games/oracle/`, `scripts/build_oracle_fixture.py`,
  and `tests/oracle/`.
- Each pass has a completeness + soundness test against the oracle (function
  symbols are a subset of discovery; discovery stays within known function
  extents; code relocations resolve), not just the Metal Slug superset check.
  **Done:** `test_oracle_discovery` and `test_oracle_fixture`.
- The trace importer is validated against the oracle's known function set.
  **Done:** `tests/oracle/oracle_trace.log` through `trace_pc_residual.py`.

Scope and non-goals:

- This is a development and validation oracle, not a shipped title. It does not
  remove the need for passes 1-4 plus a trace on Metal Slug itself.
- A full **port of an open-licensed homebrew** (which would additionally make it
  legal to distribute recompiled output, unlike Metal Slug) is a separate,
  larger, optional track. Note it as a possible flagship deliverable, but defer
  it; the small torture ROM is the high-leverage piece.
- Toolchain dependency is real but well-trodden: `ngdevkit` needs a GCC m68k
  cross-toolchain. Treat standing it up as its own bounded setup task.

---

## Pass 1 - Linked state-table scanner

Collapses: `mslug.toml:847-858` (`abs32_sparse` table_call at `$000772`),
`:878-1016` (mode/substate `abs32` tables, including the three hand-split
`$000B92..$000E8E` ranges), and the many `format = "abs32"` callback tables that
are really one linked structure.

Build on: `add_sparse_abs32_table_targets` (`function_discovery.c:183`) already
does sentinel-skip + stop-on-non-code. It is rooted at a single helper/table and
does not follow chains.

New config (`[[state_table]]`):

| key | meaning |
| --- | --- |
| `root` | first table address, or `helper`+`table_reg` to harvest the root from a dispatcher call site (reuse `add_config_table_call_targets` plumbing) |
| `stride` | entry size (default 4) |
| `sentinel` | value to skip without ending the table (default `0xFFFFFFFF`) |
| `follow_chain` | if an entry resolves inside `[table_start, table_end)`, treat it as a next-table pointer and follow it (bounded, visited-set) |
| `table_start`, `table_end` | bounds of the table region (review). Required for `follow_chain` to tell next-table pointers from code targets and data; a `root`/`target`-only schema cannot disambiguate them |
| `target` | `lo-hi` code-target filter (reuse the existing `target_start/target_end` + `is_probable_function_target` gate) |
| `max_tables`, `max_entries` | bounds |

Algorithm: from `root`, read `stride`-sized entries; skip `sentinel`; if an
entry resolves into the table region and `follow_chain`, push it as a next table
(dedup via visited set); if it passes the target filter and
`is_probable_function_target`, add it; stop a table at the first word that is
none of {code target, sentinel, next-table pointer}.

Plug-in point: new branch in `ng_function_discover_from_game_config`
(`function_discovery.c:702-715`), alongside the existing per-format dispatch.

Tests (`tests/test_function_discovery.c` style, synthetic ROM): chained tables
linked by a next-table pointer; embedded `$FFFFFFFF` sentinels; a data word that
must terminate a table; target-filter rejection of an in-range-but-not-code word.

Exit criteria: the `$000772` table_call and the `$000B92..$000E8E` jump_table
cluster are replaced by one `[[state_table]]`; golden superset check stays green.

---

## Pass 2 - Tagged / fixed record scanner

Collapses the largest chunk: `mslug.toml:878-906` (tag-`0x0800` streams),
`:1236-1540` (banked object/action callback records of strides `0x0A`/`0x14`/
`0x1E`), every 4-byte "one representative callback" `[[jump_table]]`, and the
`extra` entries described as "record embeds this fixed-code callback."

Build on: `add_tagged_abs32_stream_targets` (`function_discovery.c:227`) and
`add_tagged_abs32_targets` (`:472`) already match a tag word and read a callback
longword at an offset, with target filtering. Generalize into a reusable record
descriptor with multiple callback offsets, fixed or tag-delimited width, and
multiple scan regions.

New config (`[[record_format]]`):

| key | meaning |
| --- | --- |
| `name` | label for diagnostics |
| `tag`, `tag_offset` | optional opcode/tag match (omit for pure fixed-width) |
| `width` | fixed record size in bytes (records are non-overlapping at this cadence); omit for tagged streams |
| `stride` | scan step in bytes (review). Defaults to `width` for fixed records; for tagged streams set the search granularity (e.g. `2`). The earlier schema referenced `stride` without defining it |
| `callback_offsets` | list of byte offsets holding callback longwords (handles the "+4 and +0x0A sibling" case at `:1320-1326`) |
| `sentinel` | longword to skip (e.g. `0xFFFFFFFF` gaps at `:1294`) |
| `target` | `lo-hi` code-target filter |
| `scan` | list of regions: `"fixed"`, explicit `"0xSTART-0xEND"`, or `"bank:*"` (Pass 4) |

Algorithm: for each scan region, step at `stride` (= `width` for fixed records);
for each record, optionally require `tag` at `tag_offset`; for each
`callback_offsets` entry read the longword, skip `sentinel`, and add iff it
passes `target` + `is_probable_function_target`.

Validation caveat (review): `is_probable_function_target` only validates the
*first* decoded instruction, and 68k data often decodes as one valid
instruction, so it does **not** by itself make whole-bank sweeps safe. The
`target`-range filter and the record cadence carry most of the safety. For broad
sweeps, require stronger provenance: multi-instruction validation (decode several
instructions and require they all validate and reach a sane terminator), tight
`target` bounds, and/or trace corroboration (Pass 5). The current comments' fear
of "absorbing surrounding data" (e.g. `:1348-1354`) is exactly why narrow bounds
plus provenance matter.

Plug-in point: same per-format dispatch in
`ng_function_discover_from_game_config`. Parser work: extend
`game_config.c`/`game_config.h` for list-valued `callback_offsets` and `scan`.

Tests: fixed-width record table with two callback fields; tagged stream with
non-matching records interleaved; sentinel gap; a region full of pointer-shaped
data words that the validator must reject.

Exit criteria: the ~40 banked record `[[jump_table]]`s and their single-callback
representatives collapse to a handful of `[[record_format]]` blocks; golden
superset check green.

---

## Pass 3 - Object dispatcher recognizer

Collapses: `[dispatch].runtime` (`mslug.toml:1610-1614`, 60 sites) and a large
share of object-state `extra` entries (the `LEA target,A1; MOVE.L A1,(A6+slot)`
and `... ; JSR <helper>` families, e.g. `:263-335`).

Build on: `is_task_state_store` and `is_task_spawn_call` already recognize these
idioms. First landing moved the install slots and spawn helpers into
`[[dispatcher]]`; the remaining work is deriving the runtime-allowed dispatch
sites that are still hand-listed and checked by `runtime_dispatch_allowed`.

New config (`[[dispatcher]]`): move those constants into config and make the
recompiler *derive* the runtime allowlist.

| key | meaning |
| --- | --- |
| `kind` | dispatcher shape to recognize. `object_state` (`MOVEA.L (A0),A0; JMP/JSR (A0)` via `A6+slot`) is the first kind. The 60 `[dispatch].runtime` sites are heterogeneous (review): also `JSR ($2,A1)` script calls, `JMP ($04381C,PC,D0.W)` and `JMP ($0,A0,D0.W)` indexed tables, and `JSR (A1)/(A3)/(A5)` indirect-through-register, so additional kinds (or existing jump-table recognition) are needed for the rest |
| `state_slot` | A6-relative state field (`0x70`) |
| `install_slots` | slots whose `LEA;MOVE.L` installs are harvested (`0x70`, `0x3C`) |
| `spawn_helpers` | helper addresses for the spawn-call idiom (`0x0004AE`, `0x0006FE`) |

Behavior:

- Generalize `is_task_state_store`/`is_task_spawn_call` to read slot offsets and
  helper addresses from `[[dispatcher]]` instead of literals. **Done for first
  landing.**
- Add a recognizer that, during the existing scan, flags computed dispatch sites
  matching the declared shape and auto-populates the runtime-allowed set, so
  `[dispatch].runtime` is *derived* (the audit's `runtime_allowed` then comes
  from recognition, not a hand list). **Partially done:** direct `(A6)` and
  `A6+state_slot` object-state dispatches now derive `$0006C8` and `$000FDE`.

Scope (review): Pass 3 derives the `object_state` *subset* of
`[dispatch].runtime`. The table-shaped sites (`JMP (table,PC,D0.W)`,
`JMP (table,A0,D0.W)`) are classified by the static jump-table matchers
(`m68k_analyze.c`) and Pass 1/2. **Direct PC-index branch-table matching,
PC-index JSR branch-table matching, self-overwriting static-index JSR matching,
repeated inline-code PC-index table matching, and mixed branch/body PC-index
table matching are now landed. Repeated direct-dispatch stub tables are also
recognized, and direct wrappers around configured spawn helpers are accepted as
spawn helpers when they preserve A1.** A residual of
genuinely RAM-computed indirect sites (`JSR (A1)/(A3)/(A5)` with no static
table) stays as a small declared `runtime_allowed` list until a dedicated
recognizer exists. One dispatcher kind does not zero out the whole list.

Plug-in point: `scan_function_candidate` (`function_discovery.c:611-631`) for
install harvesting; `dispatch_audit.c` for derived runtime classification.

Tests: install into `A6+$70` and `A6+$3C`; spawn via each configured helper;
a non-dispatcher `JSR (A0)` that must stay classified as an unresolved computed
gap (so we do not silently whitelist everything).

Exit criteria: the `object_state` subset of `[dispatch].runtime` is derived from
`[[dispatcher]]` and removed; table-shaped sites are covered by jump-table
recognition; only genuinely-computed indirect sites remain as a small declared
list; the object-state `extra` families that match the idioms are dropped; the
audit's computed-gap count does not increase.

---

## Pass 4 - Bank-aware data/code pointer discovery

Collapses the per-bank-page duplication: the same record/table shapes are
re-declared for `$279xxx`, `$27Axxx`, `$2A0000`, `$2C2xxx`, `$2CAxxx`,
`$2EBxxx`, `$2F3xxx`, `$2F7xxx`, ... because there is no notion of "this format
recurs across banks."

Build on: `NgProgramRom` carries `fixed_*` and `bank_window_*` (`p_rom.h:5-13`).
Today the window mapping is a **single static window** (review): `p_rom.c:305-309`
maps `[bank_window_base, +bank_window_size)` to one physical region
(`offset = fixed_size + rel`); there is no bank register and no aliasing.
The current implementation supports the non-aliasing Metal Slug model, derived
multi-bank ROM layouts, and explicit non-uniform bank maps: `record_format` or
`state_table` `scan = ["bank:*"]` scans the configured bank window for each
configured physical bank. Derived banks use
`fixed_size + N * bank_window_size`; explicit `[[bank]]` entries override that
mapping with an `id`, physical `offset`, and `size`. Bank-specific discoveries
carry a bank id when the CPU address lies in the bank window and there is more
than one bank.

Prerequisite - bank identity in the address model (review): discovery stores
flat 24-bit addresses (`NgFunctionDiscovery.addrs`, `uint32_t`) and the emitter
names everything `ng_func_%06X` / `ng_label_%06X` (24-bit). If `bank:*` maps
multiple physical banks into the same `$2xxxxx` window, two distinct code/data
blocks share one 24-bit address and dedup collapses them. Therefore:

- For mslug today (P2 fits a single window, no aliasing): `bank:*` reduces to
  scanning the one P2 region; no address-model change is required. **Done.**
- For the general multi-bank case: first thread `{bank_id or physical_offset,
  cpu_addr}` through `NgFunctionDiscovery`, the dispatch audit, and the generated
  symbol names. Treat that as a blocking sub-task of Pass 4, not an afterthought.
  **Core landed:** the discovery set now keeps a bank id sidecar for bank-window
  addresses, the worklist scans each banked candidate through the correct ROM
  view, audit membership checks use the same bank context, and generated C uses
  bank-qualified function symbols for duplicate CPU addresses.

Changes:

- Add a `[[bank]]` model (or derive banks from program geometry) describing the
  physical cart banks that map into the window. **Done:** derived geometry is
  built by `NgProgramRom`, and optional explicit `[[bank]]` entries with
  `id`, `offset`/`physical_offset`, and `size` override individual mappings.
  Bad or out-of-range explicit banks fail early instead of silently producing
  partial aliases.
- Let `scan = ["bank:*"]` (or `"bank:N"`) in `[[record_format]]`/`[[state_table]]`
  iterate physical banks, applying the descriptor to each and resolving abs32
  targets that point into banked space to the correct bank. **Done for
  `[[record_format]]` and `[[state_table]]` with derived and explicit banks,
  including `bank:N` targeting and unconfigured-bank skips.**
- Keep fixed-region scans (`"fixed"`) unchanged.

Dependency note: Pass 2 should first land against the currently-mapped window
(covers the fixed region and one bank). Pass 4 then generalizes the *same*
descriptors across all banks, so the per-page `[[jump_table]]`s disappear
without re-implementing the scanners.

Tests: a record format present in two synthetic banks resolved via `bank:*`; a
`bank:N` scan through an explicit non-uniform mapping; p-rom tests for invalid
explicit banks and unconfigured gaps; ensure fixed-region behavior is
unchanged.

Exit criteria: the banked `[[jump_table]]` pages collapse into the existing
`[[record_format]]`/`[[state_table]]` descriptors with `scan = ["bank:*"]`;
golden superset check green.

---

## Pass 5 - Dispatch-miss feedback as diagnostics

Goal: stop growing the canonical manifest by hand. Turn each miss into a
*structured suggestion* and keep the irreducible residual in a regeneratable
file, mirroring `segagenesisrecomp`'s "residual shrinks to zero" model.

Today there are two miss channels: static (`ng_dispatch_audit`,
`--fail-on-dispatch-gaps`) and runtime (`ng_log_dispatch_miss` -> stderr
"dispatch miss at $X", `neogeo_runtime.c:860`). Both currently end in a human
pasting `$X` into `mslug.toml`.

Tasks:

- **Structured suggestions.** Extend the audit writer (`dispatch_audit.c:325`)
  so each missing/computed site is classified by *what structure reaches it*:
  e.g. "missing $X = abs32 longword at $Y inside a `0x14`-cadence region ->
  candidate `[[record_format]]`"; "missing $X stored into `A6+$70` at $Z ->
  object-state install (Pass 3)"; "computed `JSR (A0)` at $W, source unknown ->
  needs `[[dispatcher]]` or trace." Emit both human text and a machine-readable
  form (TOML/JSON) so the suggestion can be reviewed or auto-applied.
  **First landing done:** `--emit-dispatch-suggestions` writes generic TOML gap
  suggestions (`missing_direct`, `computed_dispatch`, `jump_table_missing`).
- **Residual file split.** Move the irreducible address list out of
  `mslug.toml` into `mslug.residual.toml` referenced via the existing
  `[game].discovery_files` include mechanism
  (`game_config.c:703-716`). The canonical manifest holds descriptors only; the
  residual file is machine-generated and its line count is the coverage metric.
  **Done:** split landed, and `scripts/generate_residual_toml.py` can regenerate
  a residual TOML from `required - covered` discovery sets. The current curated
  Metal Slug residual remains much smaller than a conservative descriptor-only
  generated residual, so do not replace it blindly.
- **Unify runtime misses.** Route `ng_log_dispatch_miss` into a structured sink
  (a miss log) that the audit/suggestion tool can ingest, so a `scripts/mslug`
  smoke run produces a *diff against the descriptor set* plus ready-made
  suggestions, not a list to hand-copy. **Done:** runtime misses still print the
  existing stderr diagnostics, and can additionally append JSONL when
  `ng_neogeo_set_dispatch_miss_log_path()` or `NG_NEO_DISPATCH_MISS_LOG` is set.
  `scripts/runtime_miss_suggestions.py` ingests that JSONL (or legacy stderr
  lines), marks misses already present in a discovery set, emits TOML
  suggestions, and can write a residual seed file for uncovered miss targets.
- **Optional ground truth (the real N64Recomp/segagenesisrecomp parity move).**
  Add a MAME execution-trace importer: the reached-PC set from a MAME run of the
  `.neo` is the neo-recomp analog of `s1disasm` labels. This `.neo` -> PC-trace
  pipeline does not exist yet (review): `docs/audio_mame_reuse_plan.md` is a
  one-off MAME romset/audio reconstruction, not a general tracer, so building it
  is part of this task. Diffing reached-PCs against the static discovery set
  yields an exact, machine-generated residual and a precise "what did we miss"
  signal, instead of waiting to stumble on a runtime miss. This is what lets the
  residual file be fully machine-generated rather than hand-grown. Validate the
  importer against the Phase 0.5 oracle first, where the reached-PC set has a
  known-correct answer, before pointing it at Metal Slug.
  **Importer and capture helper landed:** `scripts/mame_trace_capture.py`
  generates a MAME debugger trace script and can run MAME to collect a
  `maincpu` PC trace. `scripts/trace_pc_residual.py` ingests trace text with
  `PC=...` or MAME-style address prefixes, filters optional address ranges,
  diffs PCs against a discovery set, emits TOML suggestions, and can write a
  residual seed file. The remaining work is validating the capture/import loop
  against the Phase 0.5 oracle.

Exit criteria: `mslug.toml` contains descriptors only; the residual list lives
in a regeneratable file; a dispatch miss yields a structured suggestion (and,
with the trace importer, the residual is produced by tooling, not by hand).

---

## Sequencing and dependencies

1. **Phase 0** (safety net) - blocking prerequisite for everything.
2. **Phase 0.5** (oracle ROM) - can proceed in parallel with Pass 1. Once it
   lands, each pass's exit criterion upgrades from "superset against the Metal
   Slug golden" to "completeness + soundness against the oracle" (ELF symbols are
   a required subset of discovery, and discovery stays inside known function
   extents), and the Pass 5 trace importer gets validated against known truth.
3. **Pass 1** (state tables) - self-contained; biggest single structural win in
   the fixed region.
4. **Pass 2** (record scanner) against the fixed region + current window.
5. **Pass 3** (dispatcher recognizer) - removes `[dispatch].runtime` and the
   install-idiom `extra` families.
6. **Pass 4** (bank-aware) - generalizes Pass 1/2 descriptors across banks;
   removes per-page duplication.
7. **Pass 5** (diagnostics + residual split + optional trace) - makes the
   remaining residual machine-generated and self-shrinking.

Bank-awareness (Pass 4) is woven into Passes 1-2: implement them window-local
first, then lift to `bank:*`. Each pass ends with the same ritual: add the
descriptor, delete the addresses/tables it now covers, run the golden superset
check.

---

## Acceptance criteria and metrics

| Metric | Original baseline | Current | Target |
| --- | --- | --- | --- |
| `mslug.toml` lines | 1614 | 148 | < ~150 (descriptors only) |
| `[functions].extra` entries | ~700 | 0 in manifest, 434 in residual | ~0 in the manifest; irreducibles in `mslug.residual.toml` |
| `[[jump_table]]` blocks | ~80 | 2 | a handful of structural descriptors |
| `[dispatch].runtime` entries | 60 | 34 | `object_state` subset derived; table sites reclassified; small genuinely-computed residual stays declared |
| Discovered function set | baseline | superset, 64,412 addresses (+558 over golden) | **superset** (0 regressions on golden diff) |
| Dispatch-audit gaps | baseline | not worse, all tracked gaps at 0 | <= baseline |
| New unit tests | - | synthetic tests for landed passes, routine tables, diagnostics, and Phase 0.5 oracle discovery/fixture validation | one synthetic-ROM suite per pass |
| Oracle discovery checks (Phase 0.5) | n/a | done | function symbols subset of discovered; discovered within function extents; code relocs resolved; data relocs excluded; trace import reports zero missing oracle PCs |

Determinism must be preserved: same inputs produce the same discovery-set
ordering (the golden check depends on it).

---

## Risks and mitigations

- **Over-broad scans treating data as code.** The current comments repeatedly
  narrow tables to avoid this, and `is_probable_function_target` is only a weak
  single-instruction filter (review). Mitigation: lean on `target` range filters,
  record cadence, and bank scoping (not the decode check) for safety; add
  multi-instruction/terminator validation and trace corroboration before
  whole-bank sweeps.
- **Coverage regressions.** Mitigation: the golden superset check is mandatory
  in every pass's exit criteria; migrate one descriptor at a time.
- **Hand-rolled parser fragility.** `game_config.c` is a line-based TOML-ish
  parser. New list-valued keys (`callback_offsets`, `scan`, `install_slots`)
  need careful additions with `tests/test_game_config.c` coverage. Keep the
  schema as flat as the parser allows.
- **No ground truth means residual may not hit zero statically.** Honest
  framing: Passes 1-4 recover the families; Pass 5's trace importer is the path
  to driving the residual toward zero the way the reference projects do with a
  real disassembly/symbol table.
- **Generated-output reproducibility.** Per `docs/architecture.md`, keep
  everything regeneratable from source/config/tests; no manual edits to
  generated output or to the machine-generated residual file.
