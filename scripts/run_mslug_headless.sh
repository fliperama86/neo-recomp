#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
NEO_PATH="${1:-$HOME/Documents/Games/Mister/NEOGEO/mslug.neo}"
BIOS_PATH="${2:-$HOME/Documents/Games/Mister/NEOGEO/bios/sp-s2.sp1}"
DISPATCH_BUDGETS="${NG_MSLUG_PROGRESS_BUDGETS:-${NG_MSLUG_DISPATCH_BUDGET:-10000 50000 100000 500000}}"
SNAPSHOT_DIR="${NG_MSLUG_SNAPSHOT_DIR:-}"
SCANLINE_POLL_INTERVAL="${NG_MSLUG_SCANLINE_POLL_INTERVAL:-0}"
SNAPSHOT_SCANLINE="${NG_MSLUG_SNAPSHOT_SCANLINE:-}"
SNAPSHOT_EXTRA_DISPATCHES="${NG_MSLUG_SNAPSHOT_EXTRA_DISPATCHES:-250000}"

CFLAGS=(-std=c99 -Wall -Wextra -DNG_GENERATED_INSTRUCTION_HOOK=ng_generated_instruction_hook -DNG_GENERATED_CYCLE_HOOK=ng_generated_cycle_hook -DNG_GENERATED_SHOULD_YIELD=ng_generated_should_yield -I"$ROOT/include" -I"$ROOT/recompiler/src")

if [[ ! -f "$NEO_PATH" ]]; then
  echo "mslug .neo not found: $NEO_PATH" >&2
  echo "usage: $0 [path/to/mslug.neo] [path/to/sp-s2.sp1]" >&2
  exit 2
fi
if [[ -n "$SNAPSHOT_DIR" ]]; then
  mkdir -p "$SNAPSHOT_DIR"
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
grep -E "game config functions|function candidates|generated C:|dispatch audit:" "$RECOMP_LOG"

if [[ ! -f "$BIOS_PATH" ]]; then
  echo "BIOS not found: $BIOS_PATH" >&2
  echo "Running non-BIOS smoke only; this currently stops at the BIOS fallback frontier."
  cc "${CFLAGS[@]}" \
    "$ROOT/tools/generated_smoke_harness.c" \
    "$BUILD_DIR/mslug_recomp.c" \
    "$ROOT/runtime/src/neogeo_runtime.c" \
    "$ROOT/recompiler/src/p_rom.c" \
    -o "$BUILD_DIR/mslug_smoke_harness"
  SNAPSHOT_ARGS=()
  if [[ -n "$SNAPSHOT_DIR" ]]; then
    SNAPSHOT_ARGS=(--snapshot-dir "$SNAPSHOT_DIR")
  fi
  exec "$BUILD_DIR/mslug_smoke_harness" "${SNAPSHOT_ARGS[@]}" "$NEO_PATH"
fi

# Local BIOS frontier set used by the current headless smoke. Users must provide
# their own BIOS; generate-bios-recomp normalizes common word-swapped MiSTer dumps.
BIOS_SEEDS="0xC00438,0xC00444,0xC0044A,0xC00468,0xC004C2,0xC004CE,0xC11142,0xC133BA,0xC187C4,0xC187CC,0xC187D4,0xC1881A,0xC187B6,0xC17F0E,0xC1868A,0xC18690,0xC18832,0xC18012,0xC18074,0xC18082,0xC180DC,0xC1811A,0xC18194,0xC181AE,0xC18208,0xC188DC,0xC182A6"

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

python3 - "$DISPATCH_BUDGETS" "$BUILD_DIR/mslug_bios_smoke_harness" "$BIOS_PATH" "$NEO_PATH" "$SNAPSHOT_DIR" "$SCANLINE_POLL_INTERVAL" "$SNAPSHOT_SCANLINE" "$SNAPSHOT_EXTRA_DISPATCHES" <<'PY'
import re
import subprocess
import sys

(
    budget_text,
    harness,
    bios_path,
    neo_path,
    snapshot_dir,
    scanline_poll_interval,
    snapshot_scanline_text,
    snapshot_extra_dispatches_text,
) = sys.argv[1:]
try:
    budgets = [int(part) for part in budget_text.split() if part]
except ValueError:
    print(f"invalid NG_MSLUG_PROGRESS_BUDGETS/NG_MSLUG_DISPATCH_BUDGET: {budget_text!r}", file=sys.stderr)
    raise SystemExit(2)
if not budgets or any(b <= 0 for b in budgets):
    print("at least one positive dispatch budget is required", file=sys.stderr)
    raise SystemExit(2)
try:
    snapshot_scanline = (
        int(snapshot_scanline_text) if snapshot_scanline_text else None
    )
    snapshot_extra_dispatches = int(snapshot_extra_dispatches_text)
except ValueError:
    print("invalid NG_MSLUG_SNAPSHOT_SCANLINE/NG_MSLUG_SNAPSHOT_EXTRA_DISPATCHES", file=sys.stderr)
    raise SystemExit(2)
if snapshot_scanline is not None and not (0 <= snapshot_scanline < 264):
    print("NG_MSLUG_SNAPSHOT_SCANLINE must be 0..263", file=sys.stderr)
    raise SystemExit(2)
if snapshot_extra_dispatches < 0:
    print("NG_MSLUG_SNAPSHOT_EXTRA_DISPATCHES must be non-negative", file=sys.stderr)
    raise SystemExit(2)

summary_re = re.compile(
    r"smoke summary: dispatches=(?P<dispatches>\d+) "
    r"cart=(?P<cart>\d+) bios=(?P<bios>\d+) "
    r"unique=(?P<unique>\d+) hot_overflow=(?P<hot_overflow>\d+) "
    r"last=\$(?P<last>[0-9A-Fa-f]+) "
    r"last_cart=\$(?P<last_cart>[0-9A-Fa-f]+) "
    r"last_bios=\$(?P<last_bios>[0-9A-Fa-f]+) "
    r"pc=\$(?P<pc>[0-9A-Fa-f]+) "
    r"sr=\$(?P<sr>[0-9A-Fa-f]+) sp=\$(?P<sp>[0-9A-Fa-f]+) "
    r"polls=(?P<polls>\d+) watchdog=(?P<watchdog>\d+) "
    r"vblank=(?P<vblank>\d+) frame=(?P<frame>\d+) timer_irq=(?P<timer_irq>\d+) "
    r"irqack=(?P<irqack>\d+) irq_pending=\$(?P<irq_pending>[0-9A-Fa-f]+) "
    r"last_irq_pc=\$(?P<last_irq_pc>[0-9A-Fa-f]+) "
    r"last_irq_level=(?P<last_irq_level>\d+) "
    r"last_irq_vector=(?P<last_irq_vector>\d+) "
    r"scanline=(?P<scanline>\d+) "
    r"lspc=\$(?P<lspc>[0-9A-Fa-f]+) "
    r"vram_addr=\$(?P<vram_addr>[0-9A-Fa-f]+) "
    r"vram_mod=\$(?P<vram_mod>[0-9A-Fa-f]+) "
    r"mslug_sync=\$(?P<mslug_sync>[0-9A-Fa-f]+) "
    r"mslug_counters=\$(?P<mslug_counters>[0-9A-Fa-f]+) "
    r"mslug_vblank=\$(?P<mslug_vblank>[0-9A-Fa-f]+) "
    r"mslug_bios_flags=\$(?P<mslug_bios_flags>[0-9A-Fa-f]+) "
    r"sound=\$(?P<sound>[0-9A-Fa-f]+) "
    r"port=\$(?P<port>[0-9A-Fa-f]+) wram_nonzero=(?P<wram_nonzero>\d+) "
    r"wram_sum=\$(?P<wram_sum>[0-9A-Fa-f]+) "
    r"palette_nonzero=(?P<palette_nonzero>\d+) palette_sum=\$(?P<palette_sum>[0-9A-Fa-f]+) "
    r"palette_writes=(?P<palette_writes>\d+) "
    r"palette_nonzero_writes=(?P<palette_nonzero_writes>\d+) "
    r"palette_last_addr=\$(?P<palette_last_addr>[0-9A-Fa-f]+) "
    r"palette_last_value=\$(?P<palette_last_value>[0-9A-Fa-f]+) "
    r"palette_last_bank=(?P<palette_last_bank>\d+) "
    r"palette_peak_nonzero=(?P<palette_peak_nonzero>\d+) "
    r"palette_peak_sum=\$(?P<palette_peak_sum>[0-9A-Fa-f]+) "
    r"vram_nonzero=(?P<vram_nonzero>\d+) "
    r"vram_sum=\$(?P<vram_sum>[0-9A-Fa-f]+) recent_loop=(?P<recent_loop>\d+)"
)
allowed_read8_miss_re = re.compile(
    r"^ng68k_read8 miss at \$(?:FFFFFF|F3000[0-9A-F])$"
)

summaries = []
for budget in budgets:
    print(f"=== headless smoke budget {budget} ===")
    cmd = [
        harness,
        "--max-dispatches", str(budget),
        "--scanline-poll-interval", scanline_poll_interval,
        "--bios", bios_path,
    ]
    if snapshot_dir and budget == budgets[-1]:
        cmd.extend(["--snapshot-dir", snapshot_dir])
        if snapshot_scanline is not None:
            cmd.extend([
                "--snapshot-scanline", str(snapshot_scanline),
                "--snapshot-extra-dispatches", str(snapshot_extra_dispatches),
            ])
    cmd.append(neo_path)
    proc = subprocess.run(
        cmd,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    print(proc.stdout, end="")
    if proc.returncode != 0:
        print(f"headless smoke failed at budget {budget}: exit {proc.returncode}", file=sys.stderr)
        raise SystemExit(proc.returncode)
    for line in proc.stdout.splitlines():
        if "miss at $" not in line:
            continue
        if line.startswith("ng68k_read8 miss at $") and allowed_read8_miss_re.match(line):
            continue
        print(f"headless smoke hit dispatch/bus miss at budget {budget}: {line}", file=sys.stderr)
        raise SystemExit(1)
    summary_match = summary_re.search(proc.stdout)
    if not summary_match:
        print(f"missing smoke summary at budget {budget}", file=sys.stderr)
        raise SystemExit(1)
    summary = {
        k: int(v, 16) if k in {"last", "last_cart", "last_bios", "pc", "sr", "sp", "lspc", "vram_addr", "vram_mod", "mslug_sync", "mslug_counters", "mslug_vblank", "mslug_bios_flags", "sound", "port", "irq_pending", "last_irq_pc", "wram_sum", "palette_sum", "palette_last_addr", "palette_last_value", "palette_peak_sum", "vram_sum"} else int(v)
        for k, v in summary_match.groupdict().items()
    }
    settle_for_snapshot = bool(
        snapshot_dir and budget == budgets[-1] and snapshot_scanline is not None
    )
    if settle_for_snapshot:
        if not (budget <= summary["dispatches"] <= budget + snapshot_extra_dispatches):
            print(
                f"budget {budget} with snapshot settling ended at "
                f"{summary['dispatches']} dispatches",
                file=sys.stderr,
            )
            raise SystemExit(1)
        if summary["scanline"] != snapshot_scanline:
            print(
                f"snapshot settling ended at scanline {summary['scanline']}, "
                f"expected {snapshot_scanline}",
                file=sys.stderr,
            )
            raise SystemExit(1)
    elif summary["dispatches"] != budget:
        print(f"budget {budget} ended at {summary['dispatches']} dispatches", file=sys.stderr)
        raise SystemExit(1)
    if f"after {budget} dispatches" not in proc.stdout:
        print(f"budget {budget} did not stop through the dispatch-budget guard", file=sys.stderr)
        raise SystemExit(1)
    summaries.append(summary)

final = summaries[-1]
if (final["polls"] == 0 or final["watchdog"] == 0 or final["vblank"] == 0 or
    final["frame"] == 0 or final["irqack"] == 0 or final["wram_nonzero"] == 0):
    print("headless smoke did not show enough runtime progress in final summary", file=sys.stderr)
    raise SystemExit(1)
if final["cart"] == 0 or final["bios"] == 0:
    print("headless smoke did not dispatch through both cart and BIOS code", file=sys.stderr)
    raise SystemExit(1)
if final["unique"] == 0 or final["hot_overflow"] != 0:
    print("headless smoke dispatch coverage telemetry failed", file=sys.stderr)
    raise SystemExit(1)
if final["dispatches"] >= 500000 and final["vram_nonzero"] == 0:
    print("late headless smoke did not reach VRAM writes", file=sys.stderr)
    raise SystemExit(1)
if final["recent_loop"] != 0:
    print(f"final budget stopped inside a recent dispatch loop period {final['recent_loop']}", file=sys.stderr)
    raise SystemExit(1)

max_recent_loop = max(summary["recent_loop"] for summary in summaries)

if len(summaries) > 1:
    if (summaries[-1]["polls"] <= summaries[0]["polls"] or
        summaries[-1]["watchdog"] <= summaries[0]["watchdog"] or
        summaries[-1]["vblank"] <= summaries[0]["vblank"] or
        summaries[-1]["frame"] <= summaries[0]["frame"] or
        summaries[-1]["cart"] <= summaries[0]["cart"] or
        summaries[-1]["bios"] <= summaries[0]["bios"] or
        summaries[-1]["irqack"] <= summaries[0]["irqack"]):
        print("headless smoke counters did not grow across budgets", file=sys.stderr)
        raise SystemExit(1)

print(
    "progress oracle: ok "
    f"budgets={','.join(str(b) for b in budgets)} "
    f"final_pc=${final['pc']:06X} cart={final['cart']} bios={final['bios']} "
    f"last_cart=${final['last_cart']:06X} last_bios=${final['last_bios']:06X} "
    f"unique={final['unique']} "
    f"polls={final['polls']} vblank={final['vblank']} frame={final['frame']} "
    f"scanline={final['scanline']} lspc=${final['lspc']:04X} "
    f"vram_addr=${final['vram_addr']:04X} vram_mod=${final['vram_mod']:04X} "
    f"irqack={final['irqack']} "
    f"last_irq=${final['last_irq_pc']:06X}:L{final['last_irq_level']}/V{final['last_irq_vector']} "
    f"mslug_sync=${final['mslug_sync']:014X} "
    f"mslug_counters=${final['mslug_counters']:012X} "
    f"mslug_vblank=${final['mslug_vblank']:04X} "
    f"mslug_bios_flags=${final['mslug_bios_flags']:04X} "
    f"watchdog={final['watchdog']} wram_nonzero={final['wram_nonzero']} "
    f"wram_sum=${final['wram_sum']:08X} palette_nonzero={final['palette_nonzero']} "
    f"palette_sum=${final['palette_sum']:08X} palette_writes={final['palette_writes']} "
    f"palette_nonzero_writes={final['palette_nonzero_writes']} "
    f"palette_last=${final['palette_last_addr']:06X}:${final['palette_last_value']:04X}:bank{final['palette_last_bank']} "
    f"palette_peak_nonzero={final['palette_peak_nonzero']} "
    f"palette_peak_sum=${final['palette_peak_sum']:08X} "
    f"vram_nonzero={final['vram_nonzero']} "
    f"vram_sum=${final['vram_sum']:08X} "
    f"final_recent_loop={final['recent_loop']} max_recent_loop={max_recent_loop}"
)
PY
