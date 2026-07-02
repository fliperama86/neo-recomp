#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
NEO_PATH="${1:-$HOME/Documents/Games/Mister/NEOGEO/mslug.neo}"
BIOS_PATH="${2:-$HOME/Documents/Games/Mister/NEOGEO/bios/sp-s2.sp1}"
LO_ROM_PATH="${3:-}"
FAST_FORWARD="${NG_MSLUG_SDL_FAST_FORWARD:-500000}"
DISPATCHES_PER_REFRESH="${NG_MSLUG_SDL_DISPATCHES_PER_REFRESH:-50000}"
PRESENT_MODE="${NG_MSLUG_SDL_PRESENT_MODE:-frame}"
PALETTE_BANK="${NG_MSLUG_SDL_PALETTE_BANK:-active}"
SCANLINE_POLL_INTERVAL="${NG_MSLUG_SCANLINE_POLL_INTERVAL:-0}"
WATCHDOG_TIMEOUT_CYCLES="${NG_MSLUG_WATCHDOG_TIMEOUT_CYCLES:-1622015}"
VIDEO_SETTLE_DISPATCHES="${NG_MSLUG_SDL_VIDEO_SETTLE_DISPATCHES:-16}"
FRAME_HOLD="${NG_MSLUG_SDL_FRAME_HOLD:-1}"
START_MODE="${NG_MSLUG_START_MODE:-cart}"
SCALE="${NG_MSLUG_SDL_SCALE:-3}"
MAX_REFRESHES="${NG_MSLUG_SDL_MAX_REFRESHES:-}"
STATUS_INTERVAL="${NG_MSLUG_SDL_STATUS_INTERVAL:-}"
DIAGNOSTICS_INTERVAL="${NG_MSLUG_SDL_DIAGNOSTICS_INTERVAL:-}"
PERF_LOG="${NG_MSLUG_SDL_PERF_LOG:-0}"
DUMP_STATE_DIR="${NG_MSLUG_SDL_DUMP_STATE_DIR:-}"
STATE_FILE="${NG_MSLUG_SDL_STATE_FILE:-$BUILD_DIR/mslug_live.sav}"
LOAD_STATE="${NG_MSLUG_SDL_LOAD_STATE:-}"
AUTOSAVE_STATE="${NG_MSLUG_SDL_AUTOSAVE_STATE:-$BUILD_DIR/mslug_autosave.sav}"
SAVE_STATE_ON_EXIT="${NG_MSLUG_SDL_SAVE_STATE_ON_EXIT:-0}"
STALL_REFRESHES="${NG_MSLUG_SDL_STALL_REFRESHES:-}"
NO_THROTTLE="${NG_MSLUG_SDL_NO_THROTTLE:-0}"
AUDIO_OUTPUT="${NG_MSLUG_SDL_AUDIO:-1}"
AUDIO_TEST_COMMAND="${NG_MSLUG_SDL_AUDIO_TEST_COMMAND:-}"
AUTO_COIN_FRAME="${NG_MSLUG_SDL_AUTO_COIN_FRAME:-}"
AUTO_START_FRAME="${NG_MSLUG_SDL_AUTO_START_FRAME:-}"
AUTO_P1_A_FRAME="${NG_MSLUG_SDL_AUTO_P1_A_FRAME:-}"
BUILD_ONLY="${NG_MSLUG_SDL_BUILD_ONLY:-0}"
FORCE_BIOS_RECOMP="${NG_MSLUG_FORCE_BIOS_RECOMP:-0}"
DEFAULT_CART_SHARD_JOBS="$(sysctl -n hw.ncpu 2>/dev/null || getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
CART_SHARD_SIZE="${NG_MSLUG_C_SHARD_SIZE:-2048}"
CART_SHARD_JOBS="${NG_MSLUG_C_SHARD_JOBS:-$DEFAULT_CART_SHARD_JOBS}"

OPTFLAGS_VALUE="${NG_MSLUG_SDL_OPTFLAGS:--O1 -DNDEBUG}"
# shellcheck disable=SC2206
OPTFLAGS=($OPTFLAGS_VALUE)
CFLAGS=(-std=c99 -Wall -Wextra -Wno-unused-function "${OPTFLAGS[@]}" -DNG_GENERATED_INSTRUCTION_HOOK=ng_generated_instruction_hook -DNG_GENERATED_CYCLE_HOOK=ng_generated_cycle_hook -DNG_GENERATED_SHOULD_YIELD=ng_generated_should_yield -I"$ROOT/include" -I"$ROOT/recompiler/src")
CXXFLAGS=(-std=c++17 -Wall -Wextra "${OPTFLAGS[@]}" -I"$ROOT/include" -I"$ROOT/runtime/src" -isystem "$ROOT/third_party/ymfm")

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
log_note "safety dispatch cap/refresh: $DISPATCHES_PER_REFRESH"
log_note "present mode: $PRESENT_MODE"
log_note "palette bank: $PALETTE_BANK"
log_note "watchdog timeout cycles: $WATCHDOG_TIMEOUT_CYCLES"
log_note "video settle dispatches: $VIDEO_SETTLE_DISPATCHES"
log_note "frame hold: $FRAME_HOLD"
log_note "start mode: $START_MODE"
log_note "audio output: $AUDIO_OUTPUT"
log_note "savestate: $STATE_FILE (F5 save, F8 load)"
log_note "autosave on miss: $AUTOSAVE_STATE"
if [[ -n "$LOAD_STATE" ]]; then
  log_note "load state: $LOAD_STATE"
fi
if [[ "$SAVE_STATE_ON_EXIT" != "0" ]]; then
  log_note "save state on exit: yes"
fi
if [[ -n "$AUDIO_TEST_COMMAND" ]]; then
  log_note "audio test command: $AUDIO_TEST_COMMAND"
fi
if [[ -n "$AUTO_COIN_FRAME" ]]; then
  log_note "auto coin frame: $AUTO_COIN_FRAME"
fi
if [[ -n "$AUTO_START_FRAME" ]]; then
  log_note "auto start frame: $AUTO_START_FRAME"
fi
if [[ -n "$AUTO_P1_A_FRAME" ]]; then
  log_note "auto P1 A frame: $AUTO_P1_A_FRAME"
fi
log_note "native C optimization flags: ${OPTFLAGS[*]:-(none)}"
log_note "cart C sharding: size=$CART_SHARD_SIZE jobs=$CART_SHARD_JOBS"

log_step "Configuring/building recompiler tools"
cmake -S "$ROOT" -B "$BUILD_DIR" >/dev/null
cmake --build "$BUILD_DIR" --target neo-recomp generate-bios-recomp -j4

CART_SHARD_DIR="$BUILD_DIR/mslug_recomp_shards"
CART_SHARD_LIST="$CART_SHARD_DIR/shards.list"
CART_AUDIT="$BUILD_DIR/mslug_dispatch_audit.txt"
BIOS_C="$BUILD_DIR/bios_recomp_mslug_headless.c"
BIOS_SEEDS_STAMP="$BUILD_DIR/bios_recomp_mslug_headless.seeds"
BIOS_OBJ="$BUILD_DIR/bios_recomp_mslug_headless.o"
HARNESS_OBJ="$BUILD_DIR/generated_smoke_harness_live.o"
RUNTIME_OBJ="$BUILD_DIR/neogeo_runtime_live.o"
VIDEO_OBJ="$BUILD_DIR/neogeo_video_live.o"
AUDIO_OBJ="$BUILD_DIR/neogeo_audio_live.o"
YM2610_OBJ="$BUILD_DIR/neogeo_ym2610_live.o"
Z80_OBJ="$BUILD_DIR/superzazu_z80_live.o"
YMFM_ADPCM_OBJ="$BUILD_DIR/ymfm_adpcm_live.o"
YMFM_OPN_OBJ="$BUILD_DIR/ymfm_opn_live.o"
YMFM_SSG_OBJ="$BUILD_DIR/ymfm_ssg_live.o"
P_ROM_OBJ="$BUILD_DIR/p_rom_live.o"
SDL_HOST_OBJ="$BUILD_DIR/sdl_live_host.o"
HOST_BIN="$BUILD_DIR/mslug_sdl_host"
FLAGS_STAMP="$BUILD_DIR/mslug_live_cflags.stamp"
CART_SHARD_SIZE_STAMP="$CART_SHARD_DIR/shard_size.stamp"

FLAGS_TEXT="$(printf '%q ' "${CFLAGS[@]}")"
if [[ ! -f "$FLAGS_STAMP" ]] || [[ "$(cat "$FLAGS_STAMP")" != "$FLAGS_TEXT" ]]; then
  printf '%s\n' "$FLAGS_TEXT" >"$FLAGS_STAMP"
fi
mkdir -p "$CART_SHARD_DIR"
if [[ ! -f "$CART_SHARD_SIZE_STAMP" ]] || [[ "$(cat "$CART_SHARD_SIZE_STAMP")" != "$CART_SHARD_SIZE" ]]; then
  printf '%s\n' "$CART_SHARD_SIZE" >"$CART_SHARD_SIZE_STAMP"
fi

RECOMP_LOG="$BUILD_DIR/mslug_recomp.log"
if is_fresh "$CART_SHARD_LIST" \
  "$BUILD_DIR/neo-recomp" \
  "$ROOT/games/mslug.toml" \
  "$ROOT/games/mslug.residual.toml" \
  "$ROOT/games/mslug.mined_record_tables.toml" \
  "$CART_SHARD_SIZE_STAMP" \
  "$NEO_PATH"; then
  log_step "Recompiling Metal Slug cart CPU code"
  log_note "cached shards: $CART_SHARD_LIST"
else
  log_step "Recompiling Metal Slug cart CPU code into shards"
  if ! "$BUILD_DIR/neo-recomp" \
    --game "$ROOT/games/mslug.toml" \
    --neo "$NEO_PATH" \
    --emit-c-shards "$CART_SHARD_DIR" \
    --emit-c-shard-size "$CART_SHARD_SIZE" \
    --emit-dispatch-audit "$CART_AUDIT" \
    --fail-on-dispatch-gaps >"$RECOMP_LOG"; then
    cat "$RECOMP_LOG" >&2
    exit 1
  fi
  grep -E "game config functions|function candidates|generated C shards:|dispatch audit:" "$RECOMP_LOG" || true
fi

BIOS_SEEDS="0xC00402,0xC00438,0xC00444,0xC0044A,0xC00468,0xC004C2,0xC004CE,0xC11142,0xC133BA,0xC187C4,0xC187CC,0xC187D4,0xC18814,0xC1881A,0xC187B6,0xC17F0E,0xC1868A,0xC18690,0xC18832,0xC18012,0xC18074,0xC18082,0xC180DC,0xC1811A,0xC18194,0xC181AE,0xC18208,0xC188DC,0xC182A6"
if [[ ! -f "$BIOS_SEEDS_STAMP" ]] || [[ "$(cat "$BIOS_SEEDS_STAMP")" != "$BIOS_SEEDS" ]]; then
  printf '%s\n' "$BIOS_SEEDS" >"$BIOS_SEEDS_STAMP"
fi
if [[ "$FORCE_BIOS_RECOMP" != "0" ]]; then
  rm -f "$BIOS_C" "$BIOS_OBJ"
fi
if is_fresh "$BIOS_C" "$BIOS_PATH" "$BIOS_SEEDS_STAMP"; then
  log_step "Generating BIOS recomp slice"
  log_note "cached: $BIOS_C"
else
  log_step "Generating BIOS recomp slice (slow/silent; this can take a few minutes)"
  "$BUILD_DIR/generate-bios-recomp" \
    "$BIOS_PATH" \
    "$BIOS_SEEDS" \
    "$BIOS_C"
fi

CART_OBJS=()
CART_COMPILE_LIST="$BUILD_DIR/mslug_cart_shards_to_compile.txt"
: >"$CART_COMPILE_LIST"
if [[ ! -f "$CART_SHARD_LIST" ]]; then
  echo "cart shard list missing: $CART_SHARD_LIST" >&2
  exit 1
fi
while IFS= read -r shard_c; do
  [[ -n "$shard_c" ]] || continue
  shard_obj="${shard_c%.c}.o"
  CART_OBJS+=("$shard_obj")
  if ! is_fresh "$shard_obj" "$shard_c" "$FLAGS_STAMP"; then
    printf '%s\n%s\n' "$shard_c" "$shard_obj" >>"$CART_COMPILE_LIST"
  fi
done <"$CART_SHARD_LIST"
if [[ -s "$CART_COMPILE_LIST" ]]; then
  CART_COMPILE_SCRIPT="$BUILD_DIR/compile_mslug_cart_shard.sh"
  {
    printf '#!/usr/bin/env bash\n'
    printf 'set -euo pipefail\n'
    printf 'cc'
    printf ' %q' "${CFLAGS[@]}"
    printf ' -DNG_GENERATED_CALL=ng_cart_generated_call -DNG_GENERATED_DISPATCH=ng_generated_call -c "$1" -o "$2"\n'
  } >"$CART_COMPILE_SCRIPT"
  chmod +x "$CART_COMPILE_SCRIPT"
  STALE_SHARDS=$(( $(wc -l <"$CART_COMPILE_LIST") / 2 ))
  log_step "Compiling generated cart C shards"
  log_note "stale: $STALE_SHARDS/${#CART_OBJS[@]} objects, jobs=$CART_SHARD_JOBS"
  xargs -n 2 -P "$CART_SHARD_JOBS" "$CART_COMPILE_SCRIPT" <"$CART_COMPILE_LIST"
else
  log_step "Compiling generated cart C shards"
  log_note "cached: ${#CART_OBJS[@]} objects"
fi
if is_fresh "$BIOS_OBJ" "$BIOS_C" "$FLAGS_STAMP"; then
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
cc "${CFLAGS[@]}" -I"$ROOT/runtime/src" -I"$ROOT/third_party/superzazu" \
  -c "$ROOT/runtime/src/neogeo_audio.c" \
  -o "$AUDIO_OBJ"
cc "${CFLAGS[@]}" -I"$ROOT/third_party/superzazu" \
  -c "$ROOT/third_party/superzazu/z80.c" \
  -o "$Z80_OBJ"
c++ "${CXXFLAGS[@]}" \
  -c "$ROOT/runtime/src/neogeo_ym2610.cpp" \
  -o "$YM2610_OBJ"
c++ -std=c++17 "${OPTFLAGS[@]}" -I"$ROOT/third_party/ymfm" \
  -c "$ROOT/third_party/ymfm/ymfm_adpcm.cpp" \
  -o "$YMFM_ADPCM_OBJ"
c++ -std=c++17 "${OPTFLAGS[@]}" -I"$ROOT/third_party/ymfm" \
  -c "$ROOT/third_party/ymfm/ymfm_opn.cpp" \
  -o "$YMFM_OPN_OBJ"
c++ -std=c++17 "${OPTFLAGS[@]}" -I"$ROOT/third_party/ymfm" \
  -c "$ROOT/third_party/ymfm/ymfm_ssg.cpp" \
  -o "$YMFM_SSG_OBJ"
cc "${CFLAGS[@]}" -c "$ROOT/recompiler/src/p_rom.c" -o "$P_ROM_OBJ"
# shellcheck disable=SC2207
SDL_CFLAGS=($(pkg-config --cflags sdl2))
# shellcheck disable=SC2207
SDL_LIBS=($(pkg-config --libs sdl2))
cc "${CFLAGS[@]}" "${SDL_CFLAGS[@]}" \
  -c "$ROOT/tools/sdl_live_host.c" \
  -o "$SDL_HOST_OBJ"

log_step "Linking SDL host"
c++ \
  "$SDL_HOST_OBJ" \
  "$HARNESS_OBJ" \
  "${CART_OBJS[@]}" \
  "$BIOS_OBJ" \
  "$RUNTIME_OBJ" \
  "$VIDEO_OBJ" \
  "$AUDIO_OBJ" \
  "$YM2610_OBJ" \
  "$Z80_OBJ" \
  "$YMFM_ADPCM_OBJ" \
  "$YMFM_OPN_OBJ" \
  "$YMFM_SSG_OBJ" \
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
  --present-mode "$PRESENT_MODE"
  --palette-bank "$PALETTE_BANK"
  --scanline-poll-interval "$SCANLINE_POLL_INTERVAL"
  --watchdog-timeout-cycles "$WATCHDOG_TIMEOUT_CYCLES"
  --video-settle-dispatches "$VIDEO_SETTLE_DISPATCHES"
  --frame-hold "$FRAME_HOLD"
  --scale "$SCALE"
  --state-file "$STATE_FILE"
  --autosave-state "$AUTOSAVE_STATE"
)
if [[ -n "$LOAD_STATE" ]]; then
  HOST_ARGS+=(--load-state "$LOAD_STATE")
fi
if [[ "$SAVE_STATE_ON_EXIT" != "0" ]]; then
  HOST_ARGS+=(--save-state-on-exit)
fi
if [[ -n "$MAX_REFRESHES" ]]; then
  HOST_ARGS+=(--max-refreshes "$MAX_REFRESHES")
fi
if [[ -n "$STATUS_INTERVAL" ]]; then
  HOST_ARGS+=(--status-interval "$STATUS_INTERVAL")
fi
if [[ -n "$DIAGNOSTICS_INTERVAL" ]]; then
  HOST_ARGS+=(--diagnostics-interval "$DIAGNOSTICS_INTERVAL")
fi
if [[ "$PERF_LOG" != "0" ]]; then
  HOST_ARGS+=(--perf-log)
fi
if [[ -n "$DUMP_STATE_DIR" ]]; then
  mkdir -p "$DUMP_STATE_DIR"
  HOST_ARGS+=(--dump-state-dir "$DUMP_STATE_DIR")
fi
if [[ -n "$STALL_REFRESHES" ]]; then
  HOST_ARGS+=(--stall-refreshes "$STALL_REFRESHES")
fi
if [[ "$NO_THROTTLE" != "0" ]]; then
  HOST_ARGS+=(--no-throttle)
fi
if [[ "$AUDIO_OUTPUT" == "0" ]]; then
  HOST_ARGS+=(--no-audio)
fi
if [[ -n "$AUDIO_TEST_COMMAND" ]]; then
  HOST_ARGS+=(--audio-test-command "$AUDIO_TEST_COMMAND")
fi
if [[ -n "$AUTO_COIN_FRAME" ]]; then
  HOST_ARGS+=(--auto-coin-frame "$AUTO_COIN_FRAME")
fi
if [[ -n "$AUTO_START_FRAME" ]]; then
  HOST_ARGS+=(--auto-start-frame "$AUTO_START_FRAME")
fi
if [[ -n "$AUTO_P1_A_FRAME" ]]; then
  HOST_ARGS+=(--auto-p1-a-frame "$AUTO_P1_A_FRAME")
fi
if [[ "$START_MODE" == "bios" ]]; then
  HOST_ARGS+=(--start-bios)
fi
HOST_ARGS+=("$NEO_PATH" "$BIOS_PATH")
if [[ -n "$LO_ROM_PATH" ]]; then
  HOST_ARGS+=("$LO_ROM_PATH")
fi

log_step "Launching SDL host"
log_note "if the window takes a moment, the host is fast-forwarding to a useful in-game frame"
exec "$HOST_BIN" "${HOST_ARGS[@]}"
