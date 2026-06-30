#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
NEO_PATH="${1:-${NG_MSLUG_NEO:-$HOME/Documents/Games/Mister/NEOGEO/mslug.neo}}"
GOLDEN_DIR="${NG_MSLUG_DISCOVERY_GOLDEN_DIR:-$ROOT/tests/golden}"
OUT_DIR="${NG_MSLUG_DISCOVERY_CHECK_DIR:-$BUILD_DIR/discovery_golden_check}"

if [[ ! -f "$NEO_PATH" ]]; then
  echo "mslug .neo not found: $NEO_PATH" >&2
  echo "usage: $0 [path/to/mslug.neo]" >&2
  exit 2
fi

if [[ ! -f "$GOLDEN_DIR/mslug_discovery.txt" ]]; then
  echo "golden discovery set missing: $GOLDEN_DIR/mslug_discovery.txt" >&2
  exit 2
fi

mkdir -p "$OUT_DIR"

cmake -S "$ROOT" -B "$BUILD_DIR" >/dev/null
cmake --build "$BUILD_DIR" --target neo-recomp -j4

"$BUILD_DIR/neo-recomp" \
  --game "$ROOT/games/mslug.toml" \
  --neo "$NEO_PATH" \
  --emit-discovery-set "$OUT_DIR/mslug_discovery.current.txt" \
  --emit-dispatch-audit "$OUT_DIR/mslug_dispatch_audit.current.txt" \
  >"$OUT_DIR/neo-recomp.log"

args=(
  --golden "$GOLDEN_DIR/mslug_discovery.txt"
  --current "$OUT_DIR/mslug_discovery.current.txt"
)
if [[ -f "$GOLDEN_DIR/mslug_dispatch_audit.txt" ]]; then
  args+=(
    --audit-golden "$GOLDEN_DIR/mslug_dispatch_audit.txt"
    --audit-current "$OUT_DIR/mslug_dispatch_audit.current.txt"
  )
fi

python3 "$ROOT/scripts/check_discovery_superset.py" "${args[@]}"
