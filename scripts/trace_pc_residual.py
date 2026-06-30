#!/usr/bin/env python3
"""Import execution PC traces and diff them against a discovery set."""

from __future__ import annotations

import argparse
import re
import sys
from collections import Counter
from pathlib import Path


ADDR_RE = re.compile(r"(?:0x|\$)?([0-9A-Fa-f]+)")
PC_PATTERNS = [
    re.compile(r"\b[Pp][Cc]\s*=\s*(?:0x|\$)?([0-9A-Fa-f]{1,8})\b"),
    re.compile(r"^\s*(?:[A-Za-z0-9_./-]+:)?\s*(?:0x|\$)?([0-9A-Fa-f]{6})\s*:"),
]


def parse_addr(text: str) -> int:
    match = ADDR_RE.fullmatch(text.strip())
    if not match:
        raise argparse.ArgumentTypeError(f"invalid address: {text!r}")
    return int(match.group(1), 16) & 0x00FFFFFF


def parse_range(text: str) -> tuple[int, int]:
    if "-" not in text:
        raise argparse.ArgumentTypeError("range must be START-END")
    start_text, end_text = text.split("-", 1)
    start = parse_addr(start_text)
    end = parse_addr(end_text)
    if end <= start:
        raise argparse.ArgumentTypeError("range end must be greater than start")
    return start, end


def read_addr_set(path: Path) -> set[int]:
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


def pc_in_ranges(pc: int, ranges: list[tuple[int, int]]) -> bool:
    if not ranges:
        return True
    return any(start <= pc < end for start, end in ranges)


def extract_pc(line: str) -> int | None:
    for pattern in PC_PATTERNS:
        match = pattern.search(line)
        if match:
            return int(match.group(1), 16) & 0x00FFFFFF
    return None


def read_trace(paths: list[Path], ranges: list[tuple[int, int]]) -> Counter[int]:
    pcs: Counter[int] = Counter()
    for path in paths:
        for raw in path.read_text(errors="replace").splitlines():
            pc = extract_pc(raw)
            if pc is not None and pc_in_ranges(pc, ranges):
                pcs[pc] += 1
    return pcs


def write_residual(path: Path, missing: list[int], source: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w") as out:
        out.write("# Auto-generated residual seeds from an execution PC trace.\n")
        out.write("# Review first; prefer structural descriptors when possible.\n")
        out.write(f"# Source: {source}\n\n")
        out.write("[functions]\n")
        out.write("extra = [\n")
        for addr in missing:
            out.write(f"  0x{addr:06X},\n")
        out.write("]\n")


def write_suggestions(out, pcs: Counter[int], discovery: set[int]) -> None:
    missing = sorted(pc for pc in pcs if pc not in discovery)
    out.write("# Machine-readable execution-trace discovery suggestions.\n")
    out.write("# These are diagnostics, not auto-applied config.\n")
    out.write(f"trace_pc_count = {len(pcs)}\n")
    out.write(f"suggestion_count = {len(missing)}\n\n")
    for pc in missing:
        out.write("[[suggestion]]\n")
        out.write('kind = "trace_pc_missing"\n')
        out.write(f"addr = 0x{pc:06X}\n")
        out.write(f"count = {pcs[pc]}\n")
        out.write('action = "prefer a structural descriptor; otherwise add to generated residual seeds"\n\n')


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Diff execution trace PCs against a neo-recomp discovery set.",
    )
    parser.add_argument("traces", nargs="+", type=Path,
                        help="Trace text files. Supports PC=xxxx and MAME-style address prefixes.")
    parser.add_argument("--discovery-set", required=True, type=Path,
                        help="Static discovery set, one address per line")
    parser.add_argument("--range", dest="ranges", action="append", type=parse_range,
                        default=[], help="Optional inclusive-exclusive PC range START-END")
    parser.add_argument("--output", type=Path,
                        help="Suggestion TOML output, defaults to stdout")
    parser.add_argument("--residual-output", type=Path,
                        help="Optional [functions].extra TOML for trace PCs missing from discovery")
    args = parser.parse_args()

    discovery = read_addr_set(args.discovery_set)
    pcs = read_trace(args.traces, args.ranges)
    missing = sorted(pc for pc in pcs if pc not in discovery)

    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open("w") as out:
            write_suggestions(out, pcs, discovery)
    else:
        write_suggestions(sys.stdout, pcs, discovery)

    if args.residual_output:
        write_residual(args.residual_output, missing, ", ".join(str(p) for p in args.traces))

    print(
        f"trace PCs: total={sum(pcs.values())} unique={len(pcs)} "
        f"discovered={len(pcs) - len(missing)} missing={len(missing)}",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
