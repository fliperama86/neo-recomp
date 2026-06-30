#!/usr/bin/env python3
"""Convert runtime dispatch miss logs into machine-readable suggestions."""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


LEGACY_MISS_RE = re.compile(r"dispatch miss at \$([0-9A-Fa-f]{1,6})")
ADDR_RE = re.compile(r"(?:0x|\$)?([0-9A-Fa-f]+)")


@dataclass
class RuntimeMiss:
    addr: int
    count: int = 0
    first_source: str = ""
    first_context: dict[str, Any] = field(default_factory=dict)


def parse_hexish(value: Any) -> int | None:
    if value is None:
        return None
    if isinstance(value, int):
        return value & 0x00FFFFFF
    if isinstance(value, str):
        try:
            return int(value, 0) & 0x00FFFFFF
        except ValueError:
            return None
    return None


def read_discovery_set(path: Path | None) -> set[int]:
    if path is None:
        return set()
    values: set[int] = set()
    for lineno, raw in enumerate(path.read_text().splitlines(), 1):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        match = ADDR_RE.fullmatch(line)
        if not match:
            raise SystemExit(f"{path}:{lineno}: expected one address, got {raw!r}")
        values.add(int(match.group(1), 16) & 0x00FFFFFF)
    return values


def load_misses(paths: list[Path]) -> dict[int, RuntimeMiss]:
    misses: dict[int, RuntimeMiss] = {}
    for path in paths:
        for lineno, raw in enumerate(path.read_text().splitlines(), 1):
            line = raw.strip()
            if not line:
                continue

            context: dict[str, Any] = {}
            addr: int | None = None
            if line.startswith("{"):
                try:
                    context = json.loads(line)
                except json.JSONDecodeError as exc:
                    raise SystemExit(f"{path}:{lineno}: invalid JSONL: {exc}") from exc
                if context.get("kind") != "dispatch_miss":
                    continue
                addr = parse_hexish(context.get("addr"))
            else:
                match = LEGACY_MISS_RE.search(line)
                if match:
                    addr = int(match.group(1), 16) & 0x00FFFFFF
                    context = {"kind": "dispatch_miss", "addr": f"0x{addr:06X}"}

            if addr is None:
                continue

            miss = misses.setdefault(
                addr,
                RuntimeMiss(addr=addr, first_source=f"{path}:{lineno}"),
            )
            miss.count += 1
            if not miss.first_context:
                miss.first_context = context
    return misses


def toml_string(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def write_residual(path: Path, addrs: list[int], source: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w") as out:
        out.write("# Auto-generated residual seeds from runtime dispatch misses.\n")
        out.write("# Review first; prefer structural descriptors when possible.\n")
        out.write(f"# Source: {source}\n\n")
        out.write("[functions]\n")
        out.write("extra = [\n")
        for addr in addrs:
            out.write(f"  0x{addr:06X},\n")
        out.write("]\n")


def write_suggestions(
    out,
    misses: dict[int, RuntimeMiss],
    discovery: set[int],
    sources: list[Path],
) -> tuple[int, int]:
    uncovered = 0
    covered = 0
    out.write("# Machine-readable runtime dispatch miss suggestions.\n")
    out.write("# These are diagnostics, not auto-applied config.\n")
    out.write(f"source_count = {len(sources)}\n")
    out.write(f"suggestion_count = {len(misses)}\n")
    out.write("\n")

    for addr in sorted(misses):
        miss = misses[addr]
        is_covered = addr in discovery if discovery else False
        if is_covered:
            covered += 1
        else:
            uncovered += 1

        context = miss.first_context
        pc = parse_hexish(context.get("pc"))
        sr = parse_hexish(context.get("sr"))
        obj = context.get("object")
        if not isinstance(obj, dict):
            obj = {}

        out.write("[[suggestion]]\n")
        out.write('kind = "runtime_dispatch_miss"\n')
        out.write(f"addr = 0x{addr:06X}\n")
        out.write(f"count = {miss.count}\n")
        out.write(f"covered = {'true' if is_covered else 'false'}\n")
        out.write(f"first_source = {toml_string(miss.first_source)}\n")
        if pc is not None:
            out.write(f"first_pc = 0x{pc:06X}\n")
        if sr is not None:
            out.write(f"first_sr = 0x{sr:04X}\n")
        for key in ("a6", "state", "parent", "record", "aux70"):
            value = parse_hexish(obj.get(key))
            if value is not None:
                out.write(f"object_{key} = 0x{value:06X}\n")
        if is_covered:
            action = "already in discovery; inspect generated dispatch binding or stale runtime log"
        else:
            action = "prefer a structural descriptor; otherwise add to generated residual seeds"
        out.write(f"action = {toml_string(action)}\n\n")

    return uncovered, covered


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Convert NG_NEO_DISPATCH_MISS_LOG JSONL into TOML suggestions.",
    )
    parser.add_argument("logs", nargs="+", type=Path,
                        help="Runtime miss log(s), JSONL from NG_NEO_DISPATCH_MISS_LOG")
    parser.add_argument("--discovery-set", type=Path,
                        help="Optional discovery set used to mark already-covered misses")
    parser.add_argument("--output", type=Path,
                        help="Suggestion TOML output, defaults to stdout")
    parser.add_argument("--residual-output", type=Path,
                        help="Optional [functions].extra TOML for uncovered miss addresses")
    args = parser.parse_args()

    misses = load_misses(args.logs)
    discovery = read_discovery_set(args.discovery_set)

    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open("w") as out:
            uncovered, covered = write_suggestions(out, misses, discovery, args.logs)
    else:
        uncovered, covered = write_suggestions(sys.stdout, misses, discovery, args.logs)

    if args.residual_output:
        residual = sorted(
            addr for addr in misses
            if not discovery or addr not in discovery
        )
        write_residual(args.residual_output, residual, ", ".join(str(p) for p in args.logs))

    print(
        f"runtime miss suggestions: suggestions={len(misses)} "
        f"uncovered={uncovered} covered={covered}",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
