#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Row:
    bank: str
    addr: int


@dataclass(frozen=True)
class Extent:
    bank: str
    start: int
    end: int
    name: str


@dataclass(frozen=True)
class Reloc:
    name: str
    site_bank: str
    site: int
    target_bank: str
    target: int
    kind: str


def run(cmd: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, check=True, text=True, capture_output=True)


def parse_hex(text: str) -> int:
    return int(text, 16) & 0x00FFFFFF


def parse_discovery(path: Path) -> set[Row]:
    rows: set[Row] = set()
    for raw in path.read_text().splitlines():
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        if line.startswith("bank:"):
            bank_text, addr_text = line.split(None, 1)
            rows.add(Row(bank_text.split(":", 1)[1], parse_hex(addr_text)))
        else:
            rows.add(Row("none", parse_hex(line)))
    return rows


def parse_functions(path: Path) -> list[Extent]:
    rows: list[Extent] = []
    for raw in path.read_text().splitlines():
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        bank, start, end, name = line.split(None, 3)
        rows.append(Extent(bank, parse_hex(start), parse_hex(end), name))
    return rows


def parse_relocs(path: Path) -> list[Reloc]:
    rows: list[Reloc] = []
    for raw in path.read_text().splitlines():
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        name, site_bank, site, target_bank, target, kind = line.split()
        rows.append(Reloc(name, site_bank, parse_hex(site), target_bank,
                          parse_hex(target), kind))
    return rows


def in_extent(row: Row, extent: Extent) -> bool:
    return row.bank == extent.bank and extent.start <= row.addr < extent.end


def write_fixed_discovery(discovery: set[Row], path: Path) -> None:
    with path.open("w") as out:
        for row in sorted(discovery, key=lambda r: (r.bank, r.addr)):
            if row.bank == "none":
                out.write(f"0x{row.addr:06X}\n")


def assert_expected(generated: Path, expected: Path) -> None:
    if generated.read_text() != expected.read_text():
        raise AssertionError(f"generated oracle truth differs from {expected}")


def main() -> int:
    if len(sys.argv) != 4:
        print("usage: test_oracle_fixture.py REPO_ROOT BUILD_DIR NEO_RECOMP", file=sys.stderr)
        return 2

    root = Path(sys.argv[1])
    build_dir = Path(sys.argv[2]) / "oracle_fixture_test"
    neo_recomp = Path(sys.argv[3])
    build_dir.mkdir(parents=True, exist_ok=True)

    run([
        sys.executable,
        str(root / "scripts" / "build_oracle_fixture.py"),
        "--repo-root",
        str(root),
        "--out-dir",
        str(build_dir),
    ])

    expected_dir = root / "tests" / "oracle"
    assert_expected(build_dir / "oracle_functions.txt", expected_dir / "oracle_functions.txt")
    assert_expected(build_dir / "oracle_relocs.txt", expected_dir / "oracle_relocs.txt")
    assert_expected(build_dir / "oracle_trace.log", expected_dir / "oracle_trace.log")

    discovery_path = build_dir / "oracle_discovery.txt"
    run([
        str(neo_recomp),
        "--game",
        str(root / "games" / "oracle" / "oracle.toml"),
        "--neo",
        str(build_dir / "oracle.neo"),
        "--emit-discovery-set",
        str(discovery_path),
    ])

    discovery = parse_discovery(discovery_path)
    functions = parse_functions(build_dir / "oracle_functions.txt")
    relocs = parse_relocs(build_dir / "oracle_relocs.txt")

    for extent in functions:
        if Row(extent.bank, extent.start) not in discovery:
            raise AssertionError(f"oracle function missing: {extent.name}")

    for row in discovery:
        if not any(in_extent(row, extent) for extent in functions):
            raise AssertionError(f"oracle soundness failure: {row.bank} 0x{row.addr:06X}")

    for reloc in relocs:
        target = Row(reloc.target_bank, reloc.target)
        if reloc.kind == "code" and target not in discovery:
            raise AssertionError(f"oracle code reloc missing: {reloc.name}")
        if reloc.kind == "data" and target in discovery:
            raise AssertionError(f"oracle data reloc discovered as code: {reloc.name}")

    fixed_discovery = build_dir / "oracle_discovery_fixed.txt"
    write_fixed_discovery(discovery, fixed_discovery)
    suggestions = build_dir / "oracle_trace_suggestions.toml"
    result = run([
        sys.executable,
        str(root / "scripts" / "trace_pc_residual.py"),
        str(build_dir / "oracle_trace.log"),
        "--discovery-set",
        str(fixed_discovery),
        "--output",
        str(suggestions),
    ])
    assert "missing=0" in result.stderr
    assert "suggestion_count = 0" in suggestions.read_text()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
