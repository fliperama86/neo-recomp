# Phase 0.5 oracle fixture

This directory holds a tiny 68000 oracle program for discovery validation. It is
not a playable Neo Geo title. The source plants the structural patterns that the
recompiler must recover from raw P-ROM bytes: linked state tables, tagged and
fixed callback records, object-state installs, routine tables, and banked record
callbacks.

Use `scripts/build_oracle_fixture.py --out-dir build/oracle_fixture` to assemble
it when `m68k-elf-*` tools are available. The script also has a deterministic
Python emitter so CI can still build the P-ROM fixture without the cross-toolchain.
