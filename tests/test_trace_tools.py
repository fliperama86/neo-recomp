#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path


def run(cmd: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, check=True, text=True, capture_output=True)


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: test_trace_tools.py REPO_ROOT BUILD_DIR", file=sys.stderr)
        return 2

    root = Path(sys.argv[1])
    out_dir = Path(sys.argv[2]) / "trace_tool_tests"
    out_dir.mkdir(parents=True, exist_ok=True)

    discovery = out_dir / "discovery.txt"
    discovery.write_text("0x000010\n0x000020\n")

    trace = out_dir / "trace.log"
    trace.write_text(
        "PC=000010\n"
        ":maincpu: 000020: 4e75 rts\n"
        "000030: 4e75 rts\n"
    )
    suggestions = out_dir / "trace_suggestions.toml"
    residual = out_dir / "trace_residual.toml"
    result = run([
        sys.executable,
        str(root / "scripts" / "trace_pc_residual.py"),
        str(trace),
        "--discovery-set",
        str(discovery),
        "--output",
        str(suggestions),
        "--residual-output",
        str(residual),
    ])
    assert "missing=1" in result.stderr
    assert "suggestion_count = 1" in suggestions.read_text()
    assert "0x000030" in suggestions.read_text()
    assert "0x000030" in residual.read_text()

    miss_log = out_dir / "miss.jsonl"
    miss_log.write_text(
        json.dumps({"kind": "dispatch_miss", "addr": "0x000020", "pc": "0x10"}) + "\n" +
        json.dumps({"kind": "dispatch_miss", "addr": "0x000040", "pc": "0x12"}) + "\n"
    )
    miss_suggestions = out_dir / "miss_suggestions.toml"
    miss_residual = out_dir / "miss_residual.toml"
    result = run([
        sys.executable,
        str(root / "scripts" / "runtime_miss_suggestions.py"),
        str(miss_log),
        "--discovery-set",
        str(discovery),
        "--output",
        str(miss_suggestions),
        "--residual-output",
        str(miss_residual),
    ])
    text = miss_suggestions.read_text()
    assert "covered = true" in text
    assert "covered = false" in text
    assert "0x000040" in miss_residual.read_text()
    assert "0x000020" not in miss_residual.read_text()
    assert "uncovered=1 covered=1" in result.stderr

    debug_script = out_dir / "capture.mame"
    result = run([
        sys.executable,
        str(root / "scripts" / "mame_trace_capture.py"),
        "--software",
        "mslug",
        "--seconds",
        "0",
        "--trace-output",
        str(out_dir / "capture_trace.log"),
        "--debugscript-output",
        str(debug_script),
        "--dry-run",
    ])
    script_text = debug_script.read_text()
    assert "trace " in script_text
    assert ",maincpu,noloops" in script_text
    assert script_text.rstrip().endswith("go")
    assert "command:" in result.stdout

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
