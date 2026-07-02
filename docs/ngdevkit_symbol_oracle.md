# ngdevkit symbol oracle

This is the Phase 0.6 source-built oracle track. It uses public ngdevkit example
programs as symbolized Neo Geo inputs, then compares those ELF function symbols
against what `neo-recomp` discovers from the P-ROM bytes.

Why this exists:

- The project goal is general recompiler capability, not a Metal Slug-only loop.
- Metal Slug has no symbols, so a live miss only tells us one PC was not covered.
- ngdevkit examples are built from source, so `rom.elf` gives us real function
  names, sizes, and bank placement for validation.

## Inputs

Expected reference checkout:

```sh
~/Projects/references/ngdevkit-examples
```

The script expects a built example containing `build/rom.elf`, or banked examples
containing `build/rom0.elf`, `build/rom1.elf`, etc. It uses the generic Neo Geo
program map in `games/ngdevkit_example.toml`.

Required tools for live extraction:

- `m68k-neogeo-elf-readelf`
- `m68k-neogeo-elf-nm`
- `m68k-neogeo-elf-objcopy`
- `neo-recomp` built in this repo

The in-repo tests only cover parser/comparison behavior. They do not require the
external ngdevkit toolchain.

## Run

After building an example with ngdevkit:

```sh
python3 scripts/ngdevkit_symbol_oracle.py \
  --example-dir ~/Projects/references/ngdevkit-examples/01-helloworld
```

Or let the script invoke `make` first:

```sh
python3 scripts/ngdevkit_symbol_oracle.py \
  --example-dir ~/Projects/references/ngdevkit-examples/01-helloworld \
  --build
```

Outputs go to:

```text
build/ngdevkit_symbol_oracle/<example>/
```

Key files:

- `ngdevkit_symbols.txt`: parsed symbol truth
- `ngdevkit_p1_cpu.bin`: CPU-order P1 generated from ELF
- `ngdevkit_p2_cpu.bin`: CPU-order P2 for PROM2 or banked examples, when needed
- `ngdevkit_discovery.txt`: `neo-recomp --emit-discovery-set` output

By default the script uses `readelf` function symbols. `--symbol-source nm` is
available as a fallback, but it is noisier because ngdevkit ROM-header labels
also appear as text symbols.

The stdout summary reports:

```text
symbols=<N> discovered_symbols=<N> missing=<N> discovery_rows=<N> unattributed_rows=<N>
```

Use `--fail-on-missing` in CI-style runs once a given example is expected to be
fully covered by the generic discovery passes.

Latest live sweep, 2026-07-01:

- 18/18 examples build and run through the oracle.
- Aggregate result: 597 ELF `FUNC` symbols, 441 exact discovered symbol entries,
  156 missing symbol entries, 22,565 discovery rows, 0 unattributed rows.
- The fixed generic gaps were absolute bank-window function-pointer loads,
  static A-register indirect calls, branch-target probes preserving static
  callback registers, D-register immediate propagation, ABI A2-A6 preservation,
  mapped vector roots, Neo Geo header callback roots, and work-RAM callback
  dispatch audit classification.
- The remaining missing entries are classified as linked-only CRT/library
  helpers, aliases, address-only PROM filler functions, or functions inlined at
  all call sites. They are not currently evidence of discovered code outside
  known symbol extents.

## Scope

This does not add ngdevkit descriptors that are tailored to one example. It is a
measurement harness. If symbols are missing, the next step is to classify the
missing family and improve the generic recognizer.
