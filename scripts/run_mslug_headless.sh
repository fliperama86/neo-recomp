#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
NEO_PATH="${1:-$HOME/Documents/Games/Mister/NEOGEO/mslug.neo}"
BIOS_PATH="${2:-$HOME/Documents/Games/Mister/NEOGEO/bios/sp-s2.sp1}"
DISPATCH_BUDGET="${NG_MSLUG_DISPATCH_BUDGET:-100000}"

CFLAGS=(-std=c99 -Wall -Wextra -I"$ROOT/include" -I"$ROOT/recompiler/src")

if [[ ! -f "$NEO_PATH" ]]; then
  echo "mslug .neo not found: $NEO_PATH" >&2
  echo "usage: $0 [path/to/mslug.neo] [path/to/sp-s2.sp1]" >&2
  exit 2
fi

cmake -S "$ROOT" -B "$BUILD_DIR" >/dev/null
cmake --build "$BUILD_DIR" --target neo-recomp generate-bios-recomp -j4

RECOMP_LOG="$BUILD_DIR/mslug_recomp.log"
if ! "$BUILD_DIR/neo-recomp" \
  --game "$ROOT/games/mslug.toml" \
  --neo "$NEO_PATH" \
  --emit-c "$BUILD_DIR/mslug_recomp.c" \
  --emit-dispatch-audit "$BUILD_DIR/mslug_dispatch_audit.txt" \
  --fail-on-dispatch-gaps >"$RECOMP_LOG"; then
  cat "$RECOMP_LOG" >&2
  exit 1
fi
grep -E "function candidates|generated C:|dispatch audit:" "$RECOMP_LOG"

if [[ ! -f "$BIOS_PATH" ]]; then
  echo "BIOS not found: $BIOS_PATH" >&2
  echo "Running non-BIOS smoke only; this currently stops at the BIOS fallback frontier."
  cc "${CFLAGS[@]}" \
    "$ROOT/tools/generated_smoke_harness.c" \
    "$BUILD_DIR/mslug_recomp.c" \
    "$ROOT/runtime/src/neogeo_runtime.c" \
    "$ROOT/recompiler/src/p_rom.c" \
    -o "$BUILD_DIR/mslug_smoke_harness"
  exec "$BUILD_DIR/mslug_smoke_harness" "$NEO_PATH"
fi

# Local BIOS frontier set used by the current headless smoke. Users must provide
# their own BIOS; generate-bios-recomp normalizes common word-swapped MiSTer dumps.
BIOS_SEEDS="0xC00438,0xC00444,0xC0044A,0xC004C2,0xC004CE,0xC11142,0xC187C4,0xC187CC,0xC187D4,0xC1881A,0xC187B6,0xC17F0E,0xC1868A,0xC18690,0xC18832,0xC18012,0xC18074,0xC18082,0xC180DC,0xC1811A,0xC18194,0xC181AE,0xC18208,0xC188DC,0xC182A6"

"$BUILD_DIR/generate-bios-recomp" \
  "$BIOS_PATH" \
  "$BIOS_SEEDS" \
  "$BUILD_DIR/bios_recomp_mslug_headless.c"

cc "${CFLAGS[@]}" \
  -DNG_GENERATED_CALL=ng_cart_generated_call \
  -DNG_GENERATED_DISPATCH=ng_generated_call \
  -c "$BUILD_DIR/mslug_recomp.c" \
  -o "$BUILD_DIR/mslug_recomp_cart.o"
cc "${CFLAGS[@]}" \
  -DNG_GENERATED_CALL=ng_bios_generated_call \
  -DNG_GENERATED_DISPATCH=ng_generated_call \
  -c "$BUILD_DIR/bios_recomp_mslug_headless.c" \
  -o "$BUILD_DIR/bios_recomp_mslug_headless.o"
cc "${CFLAGS[@]}" \
  -DNG_GENERATED_SMOKE_HAS_BIOS \
  -DNG_GENERATED_SMOKE_COMBINED_DISPATCH \
  -c "$ROOT/tools/generated_smoke_harness.c" \
  -o "$BUILD_DIR/generated_smoke_harness_bios.o"
cc "${CFLAGS[@]}" -c "$ROOT/runtime/src/neogeo_runtime.c" -o "$BUILD_DIR/neogeo_runtime.o"
cc "${CFLAGS[@]}" -c "$ROOT/recompiler/src/p_rom.c" -o "$BUILD_DIR/p_rom.o"
cc \
  "$BUILD_DIR/generated_smoke_harness_bios.o" \
  "$BUILD_DIR/mslug_recomp_cart.o" \
  "$BUILD_DIR/bios_recomp_mslug_headless.o" \
  "$BUILD_DIR/neogeo_runtime.o" \
  "$BUILD_DIR/p_rom.o" \
  -o "$BUILD_DIR/mslug_bios_smoke_harness"

exec "$BUILD_DIR/mslug_bios_smoke_harness" \
  --max-dispatches "$DISPATCH_BUDGET" \
  --bios "$BIOS_PATH" \
  "$NEO_PATH"
