#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
NEO_PATH="${1:-$HOME/Documents/Games/Mister/NEOGEO/mslug.neo}"
BIOS_PATH="${2:-$HOME/Documents/Games/Mister/NEOGEO/bios/sp-s2.sp1}"
DISPATCH_BUDGETS="${NG_MSLUG_PROGRESS_BUDGETS:-${NG_MSLUG_DISPATCH_BUDGET:-10000 50000 100000 500000}}"

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

python3 - "$DISPATCH_BUDGETS" "$BUILD_DIR/mslug_bios_smoke_harness" "$BIOS_PATH" "$NEO_PATH" <<'PY'
import re
import subprocess
import sys

budget_text, harness, bios_path, neo_path = sys.argv[1:]
try:
    budgets = [int(part) for part in budget_text.split() if part]
except ValueError:
    print(f"invalid NG_MSLUG_PROGRESS_BUDGETS/NG_MSLUG_DISPATCH_BUDGET: {budget_text!r}", file=sys.stderr)
    raise SystemExit(2)
if not budgets or any(b <= 0 for b in budgets):
    print("at least one positive dispatch budget is required", file=sys.stderr)
    raise SystemExit(2)

summary_re = re.compile(
    r"smoke summary: dispatches=(?P<dispatches>\d+) "
    r"cart=(?P<cart>\d+) bios=(?P<bios>\d+) "
    r"unique=(?P<unique>\d+) hot_overflow=(?P<hot_overflow>\d+) "
    r"last=\$(?P<last>[0-9A-Fa-f]+) pc=\$(?P<pc>[0-9A-Fa-f]+) "
    r"sr=\$(?P<sr>[0-9A-Fa-f]+) sp=\$(?P<sp>[0-9A-Fa-f]+) "
    r"polls=(?P<polls>\d+) watchdog=(?P<watchdog>\d+) "
    r"vblank=(?P<vblank>\d+) frame=(?P<frame>\d+) timer_irq=(?P<timer_irq>\d+) "
    r"irqack=(?P<irqack>\d+) irq_pending=\$(?P<irq_pending>[0-9A-Fa-f]+) "
    r"scanline=(?P<scanline>\d+) sound=\$(?P<sound>[0-9A-Fa-f]+) "
    r"port=\$(?P<port>[0-9A-Fa-f]+) wram_nonzero=(?P<wram_nonzero>\d+) "
    r"wram_sum=\$(?P<wram_sum>[0-9A-Fa-f]+) vram_nonzero=(?P<vram_nonzero>\d+) "
    r"vram_sum=\$(?P<vram_sum>[0-9A-Fa-f]+) recent_loop=(?P<recent_loop>\d+)"
)

summaries = []
for budget in budgets:
    print(f"=== headless smoke budget {budget} ===")
    proc = subprocess.run(
        [harness, "--max-dispatches", str(budget), "--bios", bios_path, neo_path],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    print(proc.stdout, end="")
    if proc.returncode != 0:
        print(f"headless smoke failed at budget {budget}: exit {proc.returncode}", file=sys.stderr)
        raise SystemExit(proc.returncode)
    if "miss at $" in proc.stdout:
        print(f"headless smoke hit dispatch/bus miss at budget {budget}", file=sys.stderr)
        raise SystemExit(1)
    summary_match = summary_re.search(proc.stdout)
    if not summary_match:
        print(f"missing smoke summary at budget {budget}", file=sys.stderr)
        raise SystemExit(1)
    summary = {
        k: int(v, 16) if k in {"last", "pc", "sr", "sp", "sound", "port", "irq_pending", "wram_sum", "vram_sum"} else int(v)
        for k, v in summary_match.groupdict().items()
    }
    if summary["dispatches"] != budget:
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
    f"unique={final['unique']} "
    f"polls={final['polls']} vblank={final['vblank']} frame={final['frame']} "
    f"scanline={final['scanline']} irqack={final['irqack']} "
    f"watchdog={final['watchdog']} wram_nonzero={final['wram_nonzero']} "
    f"wram_sum=${final['wram_sum']:08X} vram_nonzero={final['vram_nonzero']} "
    f"vram_sum=${final['vram_sum']:08X} "
    f"final_recent_loop={final['recent_loop']} max_recent_loop={max_recent_loop}"
)
PY
