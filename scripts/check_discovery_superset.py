#!/usr/bin/env python3
import argparse
import re
import sys
from pathlib import Path


ADDR_RE = re.compile(r"(?:0x|\$)?([0-9A-Fa-f]+)")
BANK_ADDR_RE = re.compile(r"bank:(\d+)\s+(?:0x|\$)?([0-9A-Fa-f]+)")
COUNT_RE = re.compile(r"([A-Za-z_]+)=(\d+)")


def load_addresses(path: Path) -> set[tuple[int | None, int]]:
    addrs: set[tuple[int | None, int]] = set()
    for lineno, line in enumerate(path.read_text().splitlines(), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        bank_match = BANK_ADDR_RE.fullmatch(line)
        if bank_match:
            addrs.add(
                (int(bank_match.group(1)), int(bank_match.group(2), 16) & 0xFFFFFF)
            )
            continue
        match = ADDR_RE.fullmatch(line)
        if not match:
            raise ValueError(f"{path}:{lineno}: expected one address, got {line!r}")
        addrs.add((None, int(match.group(1), 16) & 0xFFFFFF))
    return addrs


def load_audit_counts(path: Path) -> dict[str, int]:
    for line in path.read_text().splitlines():
        if not line.startswith("dispatch audit:"):
            continue
        counts = {key: int(value) for key, value in COUNT_RE.findall(line)}
        if "truncated" in line:
            counts["truncated"] = 1
        return counts
    raise ValueError(f"{path}: missing dispatch audit summary line")


def format_addr(addr: tuple[int | None, int]) -> str:
    bank, value = addr
    if bank is not None:
        return f"bank:{bank} 0x{value:06X}"
    return f"0x{value:06X}"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Assert a current discovery set is a superset of a golden set."
    )
    parser.add_argument("--golden", required=True, type=Path)
    parser.add_argument("--current", required=True, type=Path)
    parser.add_argument("--audit-golden", type=Path)
    parser.add_argument("--audit-current", type=Path)
    args = parser.parse_args()

    golden = load_addresses(args.golden)
    current = load_addresses(args.current)
    missing = sorted(golden - current)
    if missing:
        print(
            f"discovery set regression: dropped {len(missing)} of {len(golden)} golden addresses",
            file=sys.stderr,
        )
        for addr in missing[:64]:
            print(f"  {format_addr(addr)}", file=sys.stderr)
        if len(missing) > 64:
            print(f"  ... {len(missing) - 64} more", file=sys.stderr)
        return 1

    print(
        "discovery set superset: "
        f"golden={len(golden)} current={len(current)} additions={len(current - golden)}"
    )

    if bool(args.audit_golden) != bool(args.audit_current):
        print(
            "--audit-golden and --audit-current must be provided together",
            file=sys.stderr,
        )
        return 2

    if args.audit_golden and args.audit_current:
        golden_counts = load_audit_counts(args.audit_golden)
        current_counts = load_audit_counts(args.audit_current)
        if current_counts.get("truncated", 0):
            print("current dispatch audit is truncated", file=sys.stderr)
            return 1

        gap_keys = ("missing_direct", "computed", "table_missing")
        worse: list[str] = []
        for key in gap_keys:
            if current_counts.get(key, 0) > golden_counts.get(key, 0):
                worse.append(
                    f"{key}: golden={golden_counts.get(key, 0)} "
                    f"current={current_counts.get(key, 0)}"
                )
        if worse:
            print("dispatch audit gap regression:", file=sys.stderr)
            for line in worse:
                print(f"  {line}", file=sys.stderr)
            return 1

        gap_text = " ".join(
            f"{key}={current_counts.get(key, 0)}/{golden_counts.get(key, 0)}"
            for key in gap_keys
        )
        print(f"dispatch audit gaps not worse: {gap_text}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
