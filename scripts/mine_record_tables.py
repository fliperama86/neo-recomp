#!/usr/bin/env python3
"""Mine fixed-stride callback record tables from a Neo Geo P-ROM.

The miner is intentionally oracle-aided: existing discovery entry roots are used
as anchors, then clustered records can pull in adjacent not-yet-discovered
targets inside an explicit target range. This keeps runtime misses as evidence
for a family, not as one-off seeds.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable


NEO_HEADER_SIZE = 0x1000
ADDR_RE = re.compile(r"(?:0x|\$)?([0-9A-Fa-f]+)")
DISCOVERY_BANK_RE = re.compile(
    r"bank:(?P<bank>\d+)\s+(?:0x|\$)?(?P<addr>[0-9A-Fa-f]+)$",
)


def parse_int(text: str) -> int:
    try:
        return int(text, 0)
    except ValueError:
        match = ADDR_RE.fullmatch(text.strip())
        if not match:
            raise argparse.ArgumentTypeError(f"invalid integer/address: {text!r}")
        return int(match.group(1), 16)


def parse_range(text: str) -> tuple[int, int]:
    if "-" not in text:
        raise argparse.ArgumentTypeError("range must be START-END")
    start_text, end_text = text.split("-", 1)
    start = parse_int(start_text) & 0x00FFFFFF
    end = parse_int(end_text) & 0x00FFFFFF
    if end <= start:
        raise argparse.ArgumentTypeError("range end must be greater than start")
    return start, end


def parse_int_list(text: str) -> list[int]:
    values: list[int] = []
    for raw in text.split(","):
        item = raw.strip()
        if item:
            values.append(parse_int(item))
    if not values:
        raise argparse.ArgumentTypeError("expected at least one integer")
    return values


def byteswap_words(data: bytearray) -> None:
    for i in range(0, len(data) - 1, 2):
        data[i], data[i + 1] = data[i + 1], data[i]


def load_program(args: argparse.Namespace) -> bytes:
    if args.neo:
        data = args.neo.read_bytes()
        if len(data) < NEO_HEADER_SIZE or data[:3] != b"NEO" or data[3] != 1:
            raise SystemExit(f"{args.neo} is not a supported .neo image")
        p_size = int.from_bytes(data[4:8], "little")
        if p_size <= 0 or NEO_HEADER_SIZE + p_size > len(data):
            raise SystemExit(f"{args.neo} has an invalid P-ROM size")
        prom = bytearray(data[NEO_HEADER_SIZE:NEO_HEADER_SIZE + p_size])
        byteswap_words(prom)
        return bytes(prom)

    if not args.p1:
        raise SystemExit("expected --neo or --p1")
    image = bytearray(args.p1.read_bytes())
    if args.p2:
        image.extend(args.p2.read_bytes())
    return bytes(image)


@dataclass(frozen=True)
class AddressMap:
    fixed_base: int
    fixed_size: int
    bank_window_base: int
    bank_window_size: int
    program_size: int

    @property
    def bank_count(self) -> int:
        if self.bank_window_size <= 0 or self.program_size <= self.fixed_size:
            return 0
        return (self.program_size - self.fixed_size +
                self.bank_window_size - 1) // self.bank_window_size

    def bank_size(self, bank: int) -> int:
        offset = self.fixed_size + bank * self.bank_window_size
        if offset >= self.program_size:
            return 0
        return min(self.bank_window_size, self.program_size - offset)

    def is_fixed(self, addr: int) -> bool:
        rel = addr - self.fixed_base
        return 0 <= rel < self.fixed_size and rel < self.program_size

    def is_banked(self, addr: int) -> bool:
        rel = addr - self.bank_window_base
        return 0 <= rel < self.bank_window_size

    def translate(self, addr: int, bank: int | None = None) -> int | None:
        if self.fixed_size <= 0 and self.bank_window_size <= 0:
            return addr if 0 <= addr < self.program_size else None

        if self.is_fixed(addr):
            return addr - self.fixed_base

        if self.bank_window_size > 0 and self.is_banked(addr):
            rel = addr - self.bank_window_base
            active_bank = 0 if bank is None else bank
            if active_bank < 0 or active_bank >= self.bank_count:
                return None
            if rel >= self.bank_size(active_bank):
                return None
            offset = self.fixed_size + active_bank * self.bank_window_size + rel
            return offset if offset < self.program_size else None

        return None

    def mapped(self, addr: int, bank: int | None = None) -> bool:
        return self.translate(addr, bank) is not None


@dataclass(frozen=True)
class ScanSpec:
    token_kind: str
    start: int
    end: int
    bank: int | None = None

    def toml_token(self, auto: bool = True) -> str:
        prefix = "auto:" if auto else ""
        if self.token_kind == "bank_all":
            return f"{prefix}bank:*:0x{self.start:06X}-0x{self.end:06X}"
        if self.token_kind == "bank_one":
            assert self.bank is not None
            return f"{prefix}bank:{self.bank}:0x{self.start:06X}-0x{self.end:06X}"
        return f"{prefix}0x{self.start:06X}-0x{self.end:06X}"


@dataclass
class RecordRun:
    scan: ScanSpec
    stride: int
    callback_offset: int
    start: int
    end: int
    entries: int
    known: int
    targets: list[int] = field(default_factory=list)

    @property
    def unknown_targets(self) -> list[int]:
        return []


@dataclass
class CandidateRecord:
    addr: int
    target: int
    known: bool


def read_discovery_entries(path: Path) -> tuple[set[int], set[tuple[int | None, int]]]:
    addrs: set[int] = set()
    pairs: set[tuple[int | None, int]] = set()
    for lineno, raw in enumerate(path.read_text().splitlines(), 1):
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        bank_match = DISCOVERY_BANK_RE.fullmatch(line)
        if bank_match:
            bank = int(bank_match.group("bank"), 10)
            addr = int(bank_match.group("addr"), 16) & 0x00FFFFFF
            addrs.add(addr)
            pairs.add((bank, addr))
            continue
        match = ADDR_RE.fullmatch(line)
        if not match:
            raise SystemExit(f"{path}:{lineno}: expected discovery row, got {raw!r}")
        addr = int(match.group(1), 16) & 0x00FFFFFF
        addrs.add(addr)
        pairs.add((None, addr))
    return addrs, pairs


def expand_scan(token: str, amap: AddressMap) -> list[ScanSpec]:
    token = token.strip()
    if token == "fixed":
        return [ScanSpec("range", amap.fixed_base, amap.fixed_base + amap.fixed_size)]
    if token == "bank:*":
        start = amap.bank_window_base
        end = amap.bank_window_base + amap.bank_window_size
        return [ScanSpec("bank_all", start, end)]
    if token.startswith("bank:*:"):
        start, end = parse_range(token[len("bank:*:"):])
        return [ScanSpec("bank_all", start, end)]
    if token.startswith("bank:"):
        rest = token[len("bank:"):]
        if ":" in rest:
            bank_text, range_text = rest.split(":", 1)
            start, end = parse_range(range_text)
        else:
            bank_text = rest
            start = amap.bank_window_base
            end = amap.bank_window_base + amap.bank_window_size
        return [ScanSpec("bank_one", start, end, int(bank_text, 0))]
    start, end = parse_range(token)
    return [ScanSpec("range", start, end)]


def read32(program: bytes, amap: AddressMap, addr: int, bank: int | None) -> int | None:
    offset = amap.translate(addr, bank)
    if offset is None or offset + 4 > len(program):
        return None
    return int.from_bytes(program[offset:offset + 4], "big")


def in_ranges(value: int, ranges: list[tuple[int, int]]) -> bool:
    return not ranges or any(start <= value < end for start, end in ranges)


def target_candidate(
    value: int | None,
    amap: AddressMap,
    known_addrs: set[int],
    target_ranges: list[tuple[int, int]],
    sentinel: int,
    allow_unknown: bool,
) -> tuple[bool, bool]:
    if value is None or value == sentinel or (value & 1) != 0:
        return False, False
    value &= 0x00FFFFFF
    if not in_ranges(value, target_ranges):
        return False, False
    known = value in known_addrs
    if known:
        return True, True
    if not allow_unknown:
        return False, False
    if not target_ranges:
        return False, False
    if not amap.mapped(value):
        return False, False
    return True, False


def scan_one_bank(
    program: bytes,
    amap: AddressMap,
    scan: ScanSpec,
    bank: int | None,
    strides: Iterable[int],
    callback_offsets: Iterable[int],
    known_addrs: set[int],
    target_ranges: list[tuple[int, int]],
    min_entries: int,
    min_known: int,
    max_unknown_ratio: float,
    sentinel: int,
    allow_unknown: bool,
) -> list[RecordRun]:
    runs: list[RecordRun] = []
    for stride in strides:
        if stride <= 0:
            continue
        phase_step = 2 if stride % 2 == 0 else 1
        for callback_offset in callback_offsets:
            if callback_offset < 0:
                continue
            if callback_offset + 4 > stride:
                continue
            for phase in range(0, stride, phase_step):
                records: list[CandidateRecord] = []
                addr = scan.start + phase
                while addr + callback_offset + 4 <= scan.end:
                    value = read32(program, amap, addr + callback_offset, bank)
                    ok, known = target_candidate(
                        value,
                        amap,
                        known_addrs,
                        target_ranges,
                        sentinel,
                        allow_unknown,
                    )
                    if ok and value is not None:
                        records.append(
                            CandidateRecord(addr, value & 0x00FFFFFF, known),
                        )
                    else:
                        flush_run(records, scan, stride, callback_offset,
                                  min_entries, min_known, max_unknown_ratio,
                                  runs)
                        records = []
                    addr += stride
                flush_run(records, scan, stride, callback_offset,
                          min_entries, min_known, max_unknown_ratio, runs)
    return runs


def flush_run(
    records: list[CandidateRecord],
    scan: ScanSpec,
    stride: int,
    callback_offset: int,
    min_entries: int,
    min_known: int,
    max_unknown_ratio: float,
    out: list[RecordRun],
) -> None:
    if len(records) < min_entries:
        return
    known = sum(1 for record in records if record.known)
    unknown = len(records) - known
    if known < min_known:
        return
    if records and unknown / len(records) > max_unknown_ratio:
        return
    out.append(
        RecordRun(
            scan=scan,
            stride=stride,
            callback_offset=callback_offset,
            start=records[0].addr,
            end=records[-1].addr + stride,
            entries=len(records),
            known=known,
            targets=[record.target for record in records],
        ),
    )


def mine(
    program: bytes,
    amap: AddressMap,
    scans: list[ScanSpec],
    strides: list[int],
    callback_offsets: list[int],
    known_addrs: set[int],
    target_ranges: list[tuple[int, int]],
    min_entries: int,
    min_known: int,
    max_unknown_ratio: float,
    sentinel: int,
    allow_unknown: bool,
) -> list[RecordRun]:
    all_runs: list[RecordRun] = []
    for scan in scans:
        if scan.token_kind == "bank_all":
            for bank in range(amap.bank_count):
                if amap.bank_size(bank) == 0:
                    continue
                all_runs.extend(
                    scan_one_bank(program, amap, scan, bank, strides,
                                  callback_offsets, known_addrs, target_ranges,
                                  min_entries, min_known, max_unknown_ratio,
                                  sentinel, allow_unknown),
                )
        elif scan.token_kind == "bank_one":
            all_runs.extend(
                scan_one_bank(program, amap, scan, scan.bank, strides,
                              callback_offsets, known_addrs, target_ranges,
                              min_entries, min_known, max_unknown_ratio,
                              sentinel, allow_unknown),
            )
        else:
            all_runs.extend(
                scan_one_bank(program, amap, scan, None, strides,
                              callback_offsets, known_addrs, target_ranges,
                              min_entries, min_known, max_unknown_ratio,
                              sentinel, allow_unknown),
            )
    return all_runs


def group_runs(runs: list[RecordRun]) -> dict[tuple[int, int], list[RecordRun]]:
    grouped: dict[tuple[int, int], list[RecordRun]] = {}
    for run in runs:
        grouped.setdefault((run.stride, run.callback_offset), []).append(run)
    for group in grouped.values():
        group.sort(key=lambda run: (run.scan.token_kind, run.start, run.end))
    return dict(sorted(grouped.items()))


def target_toml(ranges: list[tuple[int, int]]) -> str | None:
    if len(ranges) == 1:
        start, end = ranges[0]
        return f'"0x{start:06X}-0x{end:06X}"'
    return None


def write_toml(
    out,
    runs: list[RecordRun],
    target_ranges: list[tuple[int, int]],
    name_prefix: str,
    min_entries: int,
) -> None:
    out.write("# Auto-generated by scripts/mine_record_tables.py.\n")
    out.write("# Review before committing. Prefer precise scan ranges.\n\n")
    grouped = group_runs(runs)
    for index, ((stride, callback_offset), group) in enumerate(grouped.items(), 1):
        out.write("[[record_format]]\n")
        out.write(f'name = "{name_prefix}_s{stride:02X}_o{callback_offset:02X}_{index}"\n')
        out.write(f"stride = 0x{stride:X}\n")
        out.write(f"callback_offsets = [0x{callback_offset:X}]\n")
        out.write(f"cluster_min_entries = {min_entries}\n")
        target = target_toml(target_ranges)
        if target is not None:
            out.write(f"target = {target}\n")
        out.write("scan = [\n")
        seen_tokens: set[str] = set()
        for run in group:
            precise = ScanSpec(run.scan.token_kind, run.start, run.end,
                               run.scan.bank)
            token = precise.toml_token(auto=True)
            if token in seen_tokens:
                continue
            seen_tokens.add(token)
            unknown_targets = sorted(set(run.targets))
            out.write(
                f'  "{token}", # entries={run.entries} known={run.known} '
                f"targets=0x{min(unknown_targets):06X}-0x{max(unknown_targets):06X}\n",
            )
        out.write("]\n\n")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Mine clustered fixed-stride callback record tables.",
    )
    input_group = parser.add_mutually_exclusive_group(required=True)
    input_group.add_argument("--neo", type=Path, help=".neo image")
    input_group.add_argument("--p1", type=Path, help="raw P1/P-ROM image")
    parser.add_argument("--p2", type=Path, help="optional raw P2/P-ROM image")
    parser.add_argument("--discovery-entries", required=True, type=Path,
                        help="neo-recomp --emit-discovery-entries output")
    parser.add_argument("--scan", action="append", required=True,
                        help="Scan token: fixed, bank:*, bank:N, RANGE, or bank:*:RANGE")
    parser.add_argument("--stride", action="append", type=parse_int_list,
                        default=[], help="Comma-separated stride(s)")
    parser.add_argument("--callback-offset", action="append", type=parse_int_list,
                        default=[], help="Comma-separated callback offset(s)")
    parser.add_argument("--target-range", action="append", type=parse_range,
                        default=[], help="Allowed callback target range")
    parser.add_argument("--min-entries", type=int, default=3)
    parser.add_argument("--min-known", type=int, default=2)
    parser.add_argument("--max-unknown-ratio", type=float, default=0.50)
    parser.add_argument("--sentinel", type=parse_int, default=0xFFFFFFFF)
    parser.add_argument("--known-only", action="store_true",
                        help="Only accept targets already in discovery entries")
    parser.add_argument("--fixed-base", type=parse_int, default=0)
    parser.add_argument("--fixed-size", type=parse_int, default=0x100000)
    parser.add_argument("--bank-window-base", type=parse_int, default=0x200000)
    parser.add_argument("--bank-window-size", type=parse_int, default=0x100000)
    parser.add_argument("--name-prefix", default="mined_record")
    parser.add_argument("--output", type=Path, help="TOML output, defaults to stdout")
    args = parser.parse_args()

    strides = [value for values in args.stride for value in values]
    callback_offsets = [value for values in args.callback_offset for value in values]
    if not strides:
        strides = [4, 0x0A, 0x12, 0x14, 0x1E]
    if not callback_offsets:
        callback_offsets = [0, 2, 4, 6, 8]

    program = load_program(args)
    amap = AddressMap(
        fixed_base=args.fixed_base,
        fixed_size=args.fixed_size,
        bank_window_base=args.bank_window_base,
        bank_window_size=args.bank_window_size,
        program_size=len(program),
    )
    known_addrs, _known_pairs = read_discovery_entries(args.discovery_entries)
    scans = [scan for token in args.scan for scan in expand_scan(token, amap)]

    runs = mine(
        program=program,
        amap=amap,
        scans=scans,
        strides=strides,
        callback_offsets=callback_offsets,
        known_addrs=known_addrs,
        target_ranges=args.target_range,
        min_entries=args.min_entries,
        min_known=args.min_known,
        max_unknown_ratio=args.max_unknown_ratio,
        sentinel=args.sentinel & 0xFFFFFFFF,
        allow_unknown=not args.known_only,
    )

    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with args.output.open("w") as out:
            write_toml(out, runs, args.target_range, args.name_prefix,
                       args.min_entries)
    else:
        write_toml(sys.stdout, runs, args.target_range, args.name_prefix,
                   args.min_entries)

    print(
        f"record-table miner: runs={len(runs)} "
        f"entries={sum(run.entries for run in runs)} "
        f"known={sum(run.known for run in runs)}",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
