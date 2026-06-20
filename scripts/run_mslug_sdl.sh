#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
NEO_PATH="${1:-$HOME/Documents/Games/Mister/NEOGEO/mslug.neo}"
BIOS_PATH="${2:-$HOME/Documents/Games/Mister/NEOGEO/bios/sp-s2.sp1}"
LO_ROM_PATH="${3:-}"
FAST_FORWARD="${NG_MSLUG_SDL_FAST_FORWARD:-500000}"
DISPATCHES_PER_REFRESH="${NG_MSLUG_SDL_DISPATCHES_PER_REFRESH:-2000}"
SCANLINE_POLL_INTERVAL="${NG_MSLUG_SCANLINE_POLL_INTERVAL:-64}"
SCALE="${NG_MSLUG_SDL_SCALE:-3}"
MAX_REFRESHES="${NG_MSLUG_SDL_MAX_REFRESHES:-}"
STATUS_INTERVAL="${NG_MSLUG_SDL_STATUS_INTERVAL:-}"
STALL_REFRESHES="${NG_MSLUG_SDL_STALL_REFRESHES:-}"
NO_THROTTLE="${NG_MSLUG_SDL_NO_THROTTLE:-0}"
BUILD_ONLY="${NG_MSLUG_SDL_BUILD_ONLY:-0}"

CFLAGS=(-std=c99 -Wall -Wextra -DNG_GENERATED_INSTRUCTION_HOOK=ng_generated_instruction_hook -I"$ROOT/include" -I"$ROOT/recompiler/src")

log_step() {
  printf '\n==> %s\n' "$*" >&2
}

log_note() {
  printf '    %s\n' "$*" >&2
}

is_fresh() {
  local output="$1"
  shift
  [[ -f "$output" ]] || return 1
  local input
  for input in "$@"; do
    [[ -e "$input" ]] || continue
    if [[ "$input" -nt "$output" ]]; then
      return 1
    fi
  done
  return 0
}

trap 'printf "\nerror: run_mslug_sdl.sh failed near line %d while running: %s\n" "$LINENO" "$BASH_COMMAND" >&2' ERR

if [[ ! -f "$NEO_PATH" ]]; then
  echo "mslug .neo not found: $NEO_PATH" >&2
  echo "usage: $0 [path/to/mslug.neo] [path/to/sp-s2.sp1] [path/to/000-lo.lo]" >&2
  exit 2
fi
if [[ ! -f "$BIOS_PATH" ]]; then
  echo "BIOS not found: $BIOS_PATH" >&2
  echo "usage: $0 [path/to/mslug.neo] [path/to/sp-s2.sp1] [path/to/000-lo.lo]" >&2
  exit 2
fi
if ! pkg-config --exists sdl2; then
  echo "SDL2 not found via pkg-config; install SDL2 or set PKG_CONFIG_PATH" >&2
  exit 2
fi

log_step "Preparing Metal Slug SDL host"
log_note "game: $NEO_PATH"
log_note "bios: $BIOS_PATH"
if [[ -n "$LO_ROM_PATH" ]]; then
  log_note "lo/zoom ROM: $LO_ROM_PATH"
else
  log_note "lo/zoom ROM: auto-detect near game path if present"
fi
log_note "fast-forward: $FAST_FORWARD dispatches (set NG_MSLUG_SDL_FAST_FORWARD=10000 for faster startup)"
log_note "dispatches/refresh: $DISPATCHES_PER_REFRESH"

log_step "Configuring/building recompiler tools"
cmake -S "$ROOT" -B "$BUILD_DIR" >/dev/null
cmake --build "$BUILD_DIR" --target neo-recomp generate-bios-recomp -j4

CART_C="$BUILD_DIR/mslug_recomp.c"
CART_AUDIT="$BUILD_DIR/mslug_dispatch_audit.txt"
BIOS_C="$BUILD_DIR/bios_recomp_mslug_headless.c"
CART_OBJ="$BUILD_DIR/mslug_recomp_cart.o"
BIOS_OBJ="$BUILD_DIR/bios_recomp_mslug_headless.o"
HARNESS_OBJ="$BUILD_DIR/generated_smoke_harness_live.o"
RUNTIME_OBJ="$BUILD_DIR/neogeo_runtime_live.o"
VIDEO_OBJ="$BUILD_DIR/neogeo_video_live.o"
P_ROM_OBJ="$BUILD_DIR/p_rom_live.o"
SDL_HOST_OBJ="$BUILD_DIR/sdl_live_host.o"
HOST_BIN="$BUILD_DIR/mslug_sdl_host"

RECOMP_LOG="$BUILD_DIR/mslug_recomp.log"
if is_fresh "$CART_C" "$BUILD_DIR/neo-recomp" "$ROOT/games/mslug.toml" "$NEO_PATH"; then
  log_step "Recompiling Metal Slug cart CPU code"
  log_note "cached: $CART_C"
else
  log_step "Recompiling Metal Slug cart CPU code"
  if ! "$BUILD_DIR/neo-recomp" \
    --game "$ROOT/games/mslug.toml" \
    --neo "$NEO_PATH" \
    --emit-c "$CART_C" \
    --emit-dispatch-audit "$CART_AUDIT" \
    --fail-on-dispatch-gaps >"$RECOMP_LOG"; then
    cat "$RECOMP_LOG" >&2
    exit 1
  fi
  grep -E "function candidates|generated C:|dispatch audit:" "$RECOMP_LOG" || true
fi

BIOS_SEEDS="0xC00438,0xC00444,0xC0044A,0xC004C2,0xC004CE,0xC11142,0xC187C4,0xC187CC,0xC187D4,0xC1881A,0xC187B6,0xC17F0E,0xC1868A,0xC18690,0xC18832,0xC18012,0xC18074,0xC18082,0xC180DC,0xC1811A,0xC18194,0xC181AE,0xC18208,0xC188DC,0xC182A6"
if is_fresh "$BIOS_C" "$BUILD_DIR/generate-bios-recomp" "$BIOS_PATH"; then
  log_step "Generating BIOS recomp slice"
  log_note "cached: $BIOS_C"
else
  log_step "Generating BIOS recomp slice (slow/silent; this can take a few minutes)"
  "$BUILD_DIR/generate-bios-recomp" \
    "$BIOS_PATH" \
    "$BIOS_SEEDS" \
    "$BIOS_C"
fi

if is_fresh "$CART_OBJ" "$CART_C"; then
  log_step "Compiling generated cart C"
  log_note "cached: $CART_OBJ"
else
  log_step "Compiling generated cart C (slow/silent; this can take a minute or more)"
  cc "${CFLAGS[@]}" \
    -DNG_GENERATED_CALL=ng_cart_generated_call \
    -DNG_GENERATED_DISPATCH=ng_generated_call \
    -c "$CART_C" \
    -o "$CART_OBJ"
fi
if is_fresh "$BIOS_OBJ" "$BIOS_C"; then
  log_step "Compiling generated BIOS C"
  log_note "cached: $BIOS_OBJ"
else
  log_step "Compiling generated BIOS C"
  cc "${CFLAGS[@]}" \
    -DNG_GENERATED_CALL=ng_bios_generated_call \
    -DNG_GENERATED_DISPATCH=ng_generated_call \
    -c "$BIOS_C" \
    -o "$BIOS_OBJ"
fi

log_step "Compiling live host support"
cc "${CFLAGS[@]}" \
  -DNG_GENERATED_SMOKE_NO_MAIN \
  -DNG_GENERATED_SMOKE_HAS_BIOS \
  -DNG_GENERATED_SMOKE_COMBINED_DISPATCH \
  -c "$ROOT/tools/generated_smoke_harness.c" \
  -o "$HARNESS_OBJ"
cc "${CFLAGS[@]}" -c "$ROOT/runtime/src/neogeo_runtime.c" -o "$RUNTIME_OBJ"
cc "${CFLAGS[@]}" -c "$ROOT/runtime/src/neogeo_video.c" -o "$VIDEO_OBJ"
cc "${CFLAGS[@]}" -c "$ROOT/recompiler/src/p_rom.c" -o "$P_ROM_OBJ"
# shellcheck disable=SC2207
SDL_CFLAGS=($(pkg-config --cflags sdl2))
# shellcheck disable=SC2207
SDL_LIBS=($(pkg-config --libs sdl2))
cc "${CFLAGS[@]}" "${SDL_CFLAGS[@]}" \
  -c "$ROOT/tools/sdl_live_host.c" \
  -o "$SDL_HOST_OBJ"

log_step "Linking SDL host"
cc \
  "$SDL_HOST_OBJ" \
  "$HARNESS_OBJ" \
  "$CART_OBJ" \
  "$BIOS_OBJ" \
  "$RUNTIME_OBJ" \
  "$VIDEO_OBJ" \
  "$P_ROM_OBJ" \
  "${SDL_LIBS[@]}" \
  -o "$HOST_BIN"

if [[ "$BUILD_ONLY" != "0" ]]; then
  log_step "Build-only requested; not launching"
  log_note "host binary: $HOST_BIN"
  exit 0
fi

HOST_ARGS=(
  --fast-forward "$FAST_FORWARD"
  --dispatches-per-refresh "$DISPATCHES_PER_REFRESH"
  --scanline-poll-interval "$SCANLINE_POLL_INTERVAL"
  --scale "$SCALE"
)
if [[ -n "$MAX_REFRESHES" ]]; then
  HOST_ARGS+=(--max-refreshes "$MAX_REFRESHES")
fi
if [[ -n "$STATUS_INTERVAL" ]]; then
  HOST_ARGS+=(--status-interval "$STATUS_INTERVAL")
fi
if [[ -n "$STALL_REFRESHES" ]]; then
  HOST_ARGS+=(--stall-refreshes "$STALL_REFRESHES")
fi
if [[ "$NO_THROTTLE" != "0" ]]; then
  HOST_ARGS+=(--no-throttle)
fi
HOST_ARGS+=("$NEO_PATH" "$BIOS_PATH")
if [[ -n "$LO_ROM_PATH" ]]; then
  HOST_ARGS+=("$LO_ROM_PATH")
fi

log_step "Launching SDL host"
log_note "if the window takes a moment, the host is fast-forwarding to a useful in-game frame"
exec "$HOST_BIN" "${HOST_ARGS[@]}"
