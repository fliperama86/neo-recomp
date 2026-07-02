#!/usr/bin/env python3
"""Compare neo-recomp discovery against symbols from a built ngdevkit example.

This is a source-backed oracle for real ngdevkit programs. It does not require
ngdevkit during normal test runs, but when a built example is available it can:

* extract m68k function symbols from rom.elf with m68k-neogeo-elf-nm;
* generate CPU-order P-ROM binaries from ELF files with objcopy;
* run neo-recomp discovery; and
* report which symbolized function entries were or were not discovered.
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence

DEFAULT_FIXED_BASE = 0x000000
DEFAULT_FIXED_SIZE = 0x100000
DEFAULT_BANK_WINDOW_BASE = 0x200000
DEFAULT_BANK_WINDOW_SIZE = 0x100000
DEFAULT_TOOL_PREFIX = "m68k-neogeo-elf-"
DEFAULT_EXCLUDE_RE = [
    r"^__",
    r"^_edata$",
    r"^_end$",
    r"^_bss",
    r"^_heap",
    r"^_stack",
    r"^_rom",
    r"^\.L",
    r"^L[0-9]+$",
]
TEXT_SYMBOL_TYPES = set("TtWw")
NM_WITH_SIZE_RE = re.compile(
    r"^\s*(?P<addr>[0-9A-Fa-f]+)\s+"
    r"(?P<size>[0-9A-Fa-f]+)\s+"
    r"(?P<kind>[A-Za-z?])\s+"
    r"(?P<name>\S+)\s*$"
)
NM_NO_SIZE_RE = re.compile(
    r"^\s*(?P<addr>[0-9A-Fa-f]+)\s+"
    r"(?P<kind>[A-Za-z?])\s+"
    r"(?P<name>\S+)\s*$"
)
READELF_LINE_RE = re.compile(r"^\s*(?P<num>\d+):\s+")
READELF_SECTION_RE = re.compile(
    r"^\s*\[\s*(?P<num>\d+)\]\s+"
    r"(?P<name>\S*)\s+\S+\s+"
    r"(?P<addr>[0-9A-Fa-f]+)\s+"
    r"(?P<off>[0-9A-Fa-f]+)\s+"
    r"(?P<size>[0-9A-Fa-f]+)\s+"
    r"\S+\s+(?P<flags>[A-Z]+)\s+"
)
DISCOVERY_BANK_RE = re.compile(r"^bank:(?P<bank>[^\s]+)\s+(?P<addr>\S+)\s*$")
MAKE_ASSIGN_RE = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)\s*(\?=|:=|=)\s*(.*?)\s*$")
MAKE_VAR_RE = re.compile(r"\$\(([^)]+)\)")


@dataclass(frozen=True, order=True)
class ProgramMap:
    fixed_base: int = DEFAULT_FIXED_BASE
    fixed_size: int = DEFAULT_FIXED_SIZE
    bank_window_base: int = DEFAULT_BANK_WINDOW_BASE
    bank_window_size: int = DEFAULT_BANK_WINDOW_SIZE


@dataclass(frozen=True, order=True)
class SymbolKey:
    bank: str
    addr: int


@dataclass(frozen=True, order=True)
class Symbol:
    key: SymbolKey
    size: int
    kind: str
    name: str
    source: str

    @property
    def end(self) -> int:
        if self.size > 0:
            return self.key.addr + self.size
        return self.key.addr + 2


@dataclass(frozen=True, order=True)
class DiscoveryRow:
    bank: str
    addr: int


@dataclass(frozen=True)
class PromLayout:
    build_dir: str = "build"
    prom_size: int = DEFAULT_FIXED_SIZE
    has_prom2: bool = False
    prom2_size: int = 0


@dataclass(frozen=True)
class CompareResult:
    symbol_count: int
    discovery_count: int
    discovered_symbols: tuple[Symbol, ...]
    missing_symbols: tuple[Symbol, ...]
    unattributed_rows: tuple[DiscoveryRow, ...]


class OracleError(RuntimeError):
    pass


def parse_int(text: str) -> int:
    value = text.strip()
    if value.startswith("$((") and value.endswith("))"):
        value = value[3:-2]
    value = value.replace("$", "0x")
    return int(value, 0)


def parse_addr(text: str) -> int:
    value = text.strip()
    if value.startswith("$"):
        value = "0x" + value[1:]
    return int(value, 0) & 0x00FFFFFF


def strip_make_comment(line: str) -> str:
    escaped = False
    for index, char in enumerate(line):
        if char == "\\" and not escaped:
            escaped = True
            continue
        if char == "#" and not escaped:
            return line[:index]
        escaped = False
    return line


def read_make_assignments(paths: Iterable[Path]) -> dict[str, str]:
    values: dict[str, str] = {}
    for path in paths:
        if not path.exists():
            continue
        for raw in path.read_text(errors="replace").splitlines():
            line = strip_make_comment(raw).strip()
            if not line:
                continue
            match = MAKE_ASSIGN_RE.match(line)
            if not match:
                continue
            key, op, value = match.group(1), match.group(2), match.group(3).strip()
            if op == "?=" and key in values:
                continue
            values[key] = expand_make_vars(value, values)
    return values


def expand_make_vars(value: str, values: dict[str, str]) -> str:
    def replace(match: re.Match[str]) -> str:
        return values.get(match.group(1), match.group(0))

    previous = None
    current = value
    for _ in range(8):
        if current == previous:
            break
        previous = current
        current = MAKE_VAR_RE.sub(replace, current)
    return current


def parse_prom_layout(example_dir: Path) -> PromLayout:
    values = read_make_assignments([example_dir / "Makefile", example_dir / "rom.mk"])
    build_dir = values.get("BUILDDIR", "build")
    prom_size = parse_int(values.get("PROMSIZE", str(DEFAULT_FIXED_SIZE)))
    has_prom2 = "PROM2" in values or "PROM2SIZE" in values
    prom2_size = parse_int(values.get("PROM2SIZE", str(prom_size))) if has_prom2 else 0
    return PromLayout(build_dir=build_dir, prom_size=prom_size,
                      has_prom2=has_prom2, prom2_size=prom2_size)


def bank_for_addr(addr: int,
                  elf_bank: int | None,
                  program_map: ProgramMap,
                  bank_count: int | None = None) -> str | None:
    addr &= 0x00FFFFFF
    fixed_end = program_map.fixed_base + program_map.fixed_size
    bank_end = program_map.bank_window_base + program_map.bank_window_size
    if program_map.fixed_base <= addr < fixed_end:
        return "none"
    if program_map.bank_window_base <= addr < bank_end:
        if bank_count is None:
            return str(elf_bank if elf_bank is not None else 0)
        if bank_count > 1:
            return str(elf_bank if elf_bank is not None else 0)
        return "none"
    return None


def default_exclude_patterns() -> list[re.Pattern[str]]:
    return [re.compile(pattern) for pattern in DEFAULT_EXCLUDE_RE]


def symbol_name_excluded(name: str, patterns: Sequence[re.Pattern[str]]) -> bool:
    return any(pattern.search(name) for pattern in patterns)


def parse_nm_output(text: str,
                    source: str = "<nm>",
                    elf_bank: int | None = None,
                    program_map: ProgramMap = ProgramMap(),
                    exclude_patterns: Sequence[re.Pattern[str]] | None = None,
                    bank_count: int | None = None) -> list[Symbol]:
    patterns = default_exclude_patterns() if exclude_patterns is None else exclude_patterns
    symbols: list[Symbol] = []
    for lineno, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if not line or line.endswith(":"):
            continue
        match = NM_WITH_SIZE_RE.match(line)
        size = 0
        if match:
            size = int(match.group("size"), 16)
        else:
            match = NM_NO_SIZE_RE.match(line)
        if not match:
            continue
        kind = match.group("kind")
        name = match.group("name")
        if kind not in TEXT_SYMBOL_TYPES or symbol_name_excluded(name, patterns):
            continue
        addr = int(match.group("addr"), 16) & 0x00FFFFFF
        if addr & 1:
            continue
        bank = bank_for_addr(addr, elf_bank, program_map, bank_count)
        if bank is None:
            continue
        symbols.append(Symbol(SymbolKey(bank, addr), size, kind, name,
                              f"{source}:{lineno}"))
    return infer_zero_size_extents(dedup_symbols(symbols))


def parse_readelf_symbols(text: str,
                          source: str = "<readelf>",
                          elf_bank: int | None = None,
                          program_map: ProgramMap = ProgramMap(),
                          exclude_patterns: Sequence[re.Pattern[str]] | None = None,
                          bank_count: int | None = None) -> list[Symbol]:
    patterns = default_exclude_patterns() if exclude_patterns is None else exclude_patterns
    symbols: list[Symbol] = []
    for lineno, raw in enumerate(text.splitlines(), 1):
        if not READELF_LINE_RE.match(raw):
            continue
        parts = raw.split(None, 7)
        if len(parts) < 8:
            continue
        _, value_text, size_text, sym_type, _, _, ndx, name = parts
        if sym_type != "FUNC" or ndx in {"UND", "ABS"}:
            continue
        name = name.strip()
        if not name or symbol_name_excluded(name, patterns):
            continue
        addr = int(value_text, 16) & 0x00FFFFFF
        if addr & 1:
            continue
        bank = bank_for_addr(addr, elf_bank, program_map, bank_count)
        if bank is None:
            continue
        symbols.append(Symbol(SymbolKey(bank, addr), int(size_text, 0), "FUNC",
                              name, f"{source}:{lineno}"))
    return infer_zero_size_extents(dedup_symbols(symbols))


def parse_readelf_exec_sections(text: str,
                                source: str = "<readelf>",
                                elf_bank: int | None = None,
                                program_map: ProgramMap = ProgramMap(),
                                bank_count: int | None = None) -> list[Symbol]:
    sections: list[Symbol] = []
    for lineno, raw in enumerate(text.splitlines(), 1):
        match = READELF_SECTION_RE.match(raw)
        if not match:
            continue
        flags = match.group("flags")
        if "A" not in flags or "X" not in flags:
            continue
        addr = int(match.group("addr"), 16) & 0x00FFFFFF
        size = int(match.group("size"), 16)
        if size == 0:
            continue
        bank = bank_for_addr(addr, elf_bank, program_map, bank_count)
        if bank is None:
            continue
        name = match.group("name") or f"section_{match.group('num')}"
        sections.append(Symbol(SymbolKey(bank, addr), size, "SECTION",
                               f"<section:{name}>", f"{source}:{lineno}"))
    return sorted(sections, key=lambda s: (s.key.bank, s.key.addr, s.name))


def dedup_symbols(symbols: Iterable[Symbol]) -> list[Symbol]:
    by_identity: dict[tuple[SymbolKey, str], Symbol] = {}
    for sym in symbols:
        previous = by_identity.get((sym.key, sym.name))
        if previous is None or (previous.size == 0 and sym.size > 0):
            by_identity[(sym.key, sym.name)] = sym
    return sorted(by_identity.values(), key=lambda s: (s.key.bank, s.key.addr, s.name))


def infer_zero_size_extents(symbols: Sequence[Symbol]) -> list[Symbol]:
    by_bank: dict[str, list[Symbol]] = {}
    for sym in symbols:
        by_bank.setdefault(sym.key.bank, []).append(sym)
    inferred: list[Symbol] = []
    for bank_symbols in by_bank.values():
        ordered = sorted(bank_symbols, key=lambda s: (s.key.addr, s.name))
        for index, sym in enumerate(ordered):
            if sym.size != 0:
                inferred.append(sym)
                continue
            next_addr = 0
            for later in ordered[index + 1:]:
                if later.key.addr > sym.key.addr:
                    next_addr = later.key.addr
                    break
            size = max(2, next_addr - sym.key.addr) if next_addr else 2
            inferred.append(Symbol(sym.key, size, sym.kind, sym.name, sym.source))
    return sorted(inferred, key=lambda s: (s.key.bank, s.key.addr, s.name))


def parse_discovery_text(text: str) -> set[DiscoveryRow]:
    rows: set[DiscoveryRow] = set()
    for raw in text.splitlines():
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        match = DISCOVERY_BANK_RE.match(line)
        if match:
            rows.add(DiscoveryRow(match.group("bank"), parse_addr(match.group("addr"))))
        else:
            rows.add(DiscoveryRow("none", parse_addr(line)))
    return rows


def parse_discovery_file(path: Path) -> set[DiscoveryRow]:
    return parse_discovery_text(path.read_text(errors="replace"))


def row_in_symbol_extent(row: DiscoveryRow, symbols: Sequence[Symbol]) -> bool:
    for sym in symbols:
        if sym.key.bank == row.bank and sym.key.addr <= row.addr < sym.end:
            return True
    return False


def compare_symbols_to_discovery(symbols: Sequence[Symbol],
                                 discovery: set[DiscoveryRow],
                                 extent_symbols: Sequence[Symbol] | None = None) -> CompareResult:
    extents = symbols if extent_symbols is None else extent_symbols
    discovery_keys = {SymbolKey(row.bank, row.addr) for row in discovery}
    discovered = tuple(sym for sym in symbols if sym.key in discovery_keys)
    missing = tuple(sym for sym in symbols if sym.key not in discovery_keys)
    unattributed = tuple(sorted(
        (row for row in discovery if not row_in_symbol_extent(row, extents)),
        key=lambda row: (row.bank, row.addr),
    ))
    return CompareResult(
        symbol_count=len(symbols),
        discovery_count=len(discovery),
        discovered_symbols=discovered,
        missing_symbols=missing,
        unattributed_rows=unattributed,
    )


def format_key(bank: str, addr: int) -> str:
    if bank == "none":
        return f"0x{addr:06X}"
    return f"bank:{bank} 0x{addr:06X}"


def format_symbol(sym: Symbol) -> str:
    bank = "none" if sym.key.bank == "none" else f"bank:{sym.key.bank}"
    return f"{bank} 0x{sym.key.addr:06X}..0x{sym.end:06X} {sym.name} ({sym.source})"


def write_symbol_truth(path: Path, symbols: Sequence[Symbol]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w") as out:
        for sym in symbols:
            out.write(f"{sym.key.bank} 0x{sym.key.addr:06X} 0x{sym.end:06X} {sym.name}\n")


def format_report(example: str,
                  symbols: Sequence[Symbol],
                  result: CompareResult,
                  list_limit: int = 40) -> str:
    lines = [
        f"ngdevkit symbol oracle: {example}",
        (
            f"symbols={result.symbol_count} "
            f"discovered_symbols={len(result.discovered_symbols)} "
            f"missing={len(result.missing_symbols)} "
            f"discovery_rows={result.discovery_count} "
            f"unattributed_rows={len(result.unattributed_rows)}"
        ),
    ]
    if result.missing_symbols:
        lines.append("missing symbols:")
        for sym in result.missing_symbols[:list_limit]:
            lines.append(f"  {format_symbol(sym)}")
        remaining = len(result.missing_symbols) - list_limit
        if remaining > 0:
            lines.append(f"  ... {remaining} more")
    if result.unattributed_rows:
        lines.append("unattributed discovery rows:")
        for row in result.unattributed_rows[:list_limit]:
            lines.append(f"  {format_key(row.bank, row.addr)}")
        remaining = len(result.unattributed_rows) - list_limit
        if remaining > 0:
            lines.append(f"  ... {remaining} more")
    if symbols:
        fixed = sum(1 for sym in symbols if sym.key.bank == "none")
        banked = len(symbols) - fixed
        lines.append(f"symbol split: fixed={fixed} banked={banked}")
    return "\n".join(lines) + "\n"


def find_tool(tool_name: str, tool_prefix: str, tools_dir: Path | None) -> str | None:
    candidate_name = f"{tool_prefix}{tool_name}"
    if tools_dir is not None:
        candidate = tools_dir / candidate_name
        if candidate.exists():
            return str(candidate)
        candidate = tools_dir / "bin" / candidate_name
        if candidate.exists():
            return str(candidate)
    return shutil.which(candidate_name)


def run_checked(cmd: Sequence[str], cwd: Path | None = None) -> subprocess.CompletedProcess[str]:
    try:
        return subprocess.run(cmd, cwd=cwd, check=True, text=True,
                              capture_output=True)
    except subprocess.CalledProcessError as exc:
        stderr = exc.stderr.strip() if exc.stderr else ""
        stdout = exc.stdout.strip() if exc.stdout else ""
        details = "\n".join(part for part in [stdout, stderr] if part)
        raise OracleError(f"command failed: {' '.join(cmd)}\n{details}") from exc


def choose_make() -> str | None:
    return shutil.which("gmake") or shutil.which("make")


def build_example(example_dir: Path) -> None:
    make = choose_make()
    if make is None:
        raise OracleError("make/gmake not found")
    run_checked([make], cwd=example_dir)


def discover_elfs(example_dir: Path, build_dir: str, explicit: Sequence[Path]) -> list[Path]:
    if explicit:
        return [path.resolve() for path in explicit]
    build_path = example_dir / build_dir
    rom_elf = build_path / "rom.elf"
    if rom_elf.exists():
        return [rom_elf.resolve()]
    elfs = sorted(build_path.glob("rom*.elf"))
    return [path.resolve() for path in elfs]


def bank_id_for_elf(path: Path, index: int, total: int) -> int | None:
    if total <= 1:
        return 0
    match = re.search(r"rom(\d+)\.elf$", path.name)
    if match:
        return int(match.group(1))
    return index


def collect_symbols(nm: str | None,
                    readelf: str | None,
                    elfs: Sequence[Path],
                    program_map: ProgramMap,
                    exclude_patterns: Sequence[re.Pattern[str]],
                    symbol_source: str,
                    bank_count: int | None) -> list[Symbol]:
    all_symbols: list[Symbol] = []
    total = len(elfs)
    for index, elf in enumerate(elfs):
        bank_id = bank_id_for_elf(elf, index, total)
        use_readelf = symbol_source == "readelf" or (
            symbol_source == "auto" and readelf is not None
        )
        if use_readelf:
            if readelf is None:
                raise OracleError("readelf symbol source requested, but readelf was not found")
            result = run_checked([readelf, "-sW", str(elf)])
            parsed = parse_readelf_symbols(result.stdout, str(elf), bank_id,
                                           program_map, exclude_patterns,
                                           bank_count)
            all_symbols.extend(parsed)
            continue
        if nm is None:
            raise OracleError("nm symbol source requested, but nm was not found")
        result = run_checked([nm, "-S", "--defined-only", "--numeric-sort", str(elf)])
        all_symbols.extend(parse_nm_output(result.stdout, str(elf), bank_id,
                                           program_map, exclude_patterns,
                                           bank_count))
    return infer_zero_size_extents(dedup_symbols(all_symbols))


def collect_nm_extent_symbols(nm: str,
                              elfs: Sequence[Path],
                              program_map: ProgramMap,
                              exclude_patterns: Sequence[re.Pattern[str]],
                              bank_count: int | None) -> list[Symbol]:
    all_symbols: list[Symbol] = []
    total = len(elfs)
    for index, elf in enumerate(elfs):
        bank_id = bank_id_for_elf(elf, index, total)
        result = run_checked([nm, "-S", "--defined-only", "--numeric-sort", str(elf)])
        all_symbols.extend(parse_nm_output(result.stdout, str(elf), bank_id,
                                           program_map, exclude_patterns,
                                           bank_count))
    return infer_zero_size_extents(dedup_symbols(all_symbols))


def collect_readelf_exec_sections(readelf: str,
                                  elfs: Sequence[Path],
                                  program_map: ProgramMap,
                                  bank_count: int | None) -> list[Symbol]:
    sections: list[Symbol] = []
    total = len(elfs)
    for index, elf in enumerate(elfs):
        bank_id = bank_id_for_elf(elf, index, total)
        result = run_checked([readelf, "-SW", str(elf)])
        sections.extend(parse_readelf_exec_sections(result.stdout, str(elf),
                                                    bank_id, program_map,
                                                    bank_count))
    return sorted(sections, key=lambda s: (s.key.bank, s.key.addr, s.name))


def pad_file(path: Path, size: int) -> None:
    data = path.read_bytes()
    if len(data) > size:
        raise OracleError(f"{path} is {len(data)} bytes, larger than expected {size}")
    if len(data) < size:
        path.write_bytes(data + bytes([0xFF]) * (size - len(data)))


def normalize_bank_blob(path: Path, size: int) -> None:
    data = path.read_bytes()
    if len(data) == size:
        return
    prefixed_size = DEFAULT_BANK_WINDOW_BASE + size
    if len(data) == prefixed_size:
        path.write_bytes(data[DEFAULT_BANK_WINDOW_BASE:])
        return
    if len(data) < size:
        path.write_bytes(data + bytes([0xFF]) * (size - len(data)))
        return
    raise OracleError(f"{path} is {len(data)} bytes, cannot normalize to {size}")


def objcopy_fixed_prom(objcopy: str, elf: Path, out: Path, prom_size: int) -> None:
    out.parent.mkdir(parents=True, exist_ok=True)
    run_checked([
        objcopy,
        "-O", "binary",
        "-S",
        "-R", ".text2",
        "--gap-fill", "0xff",
        "--pad-to", str(prom_size),
        str(elf),
        str(out),
    ])
    pad_file(out, prom_size)


def objcopy_bank_prom(objcopy: str, elf: Path, out: Path, bank_size: int) -> None:
    out.parent.mkdir(parents=True, exist_ok=True)
    run_checked([
        objcopy,
        "-O", "binary",
        "-j", ".text2",
        "--gap-fill", "0xff",
        "--pad-to", str(DEFAULT_BANK_WINDOW_BASE + bank_size),
        str(elf),
        str(out),
    ])
    normalize_bank_blob(out, bank_size)


def generate_proms(objcopy: str,
                   elfs: Sequence[Path],
                   layout: PromLayout,
                   out_dir: Path) -> tuple[Path, Path | None]:
    if not elfs:
        raise OracleError("no ELF files to convert")
    p1 = out_dir / "ngdevkit_p1_cpu.bin"
    objcopy_fixed_prom(objcopy, elfs[0], p1, layout.prom_size)
    p2: Path | None = None
    if layout.has_prom2:
        bank_paths: list[Path] = []
        for index, elf in enumerate(elfs):
            bank_path = out_dir / f"ngdevkit_bank{index}_cpu.bin"
            objcopy_bank_prom(objcopy, elf, bank_path, DEFAULT_BANK_WINDOW_SIZE)
            bank_paths.append(bank_path)
        data = b"".join(path.read_bytes() for path in bank_paths)
        if len(data) > layout.prom2_size:
            raise OracleError(
                f"generated PROM2 bank data is {len(data)} bytes, larger than {layout.prom2_size}"
            )
        data += bytes([0xFF]) * (layout.prom2_size - len(data))
        p2 = out_dir / "ngdevkit_p2_cpu.bin"
        p2.write_bytes(data)
    return p1, p2


def run_discovery(neo_recomp: Path,
                  game: Path,
                  p1: Path,
                  p2: Path | None,
                  discovery_out: Path) -> None:
    cmd = [str(neo_recomp), "--game", str(game), "--p1", str(p1)]
    if p2 is not None:
        cmd.extend(["--p2", str(p2)])
    cmd.extend(["--emit-discovery-set", str(discovery_out)])
    run_checked(cmd)


def compile_excludes(patterns: Sequence[str]) -> list[re.Pattern[str]]:
    return [re.compile(pattern) for pattern in patterns]


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Compare neo-recomp discovery with ngdevkit ELF symbols."
    )
    parser.add_argument("--example-dir", type=Path,
                        help="Built ngdevkit example directory, e.g. references/ngdevkit-examples/01-helloworld")
    parser.add_argument("--repo-root", type=Path, default=Path.cwd(),
                        help="neo-recomp repo root, defaults to cwd")
    parser.add_argument("--neo-recomp", type=Path,
                        help="neo-recomp binary, defaults to REPO_ROOT/build/neo-recomp")
    parser.add_argument("--game", type=Path,
                        help="game TOML, defaults to games/ngdevkit_example.toml")
    parser.add_argument("--out-dir", type=Path,
                        help="output directory, defaults to build/ngdevkit_symbol_oracle/EXAMPLE")
    parser.add_argument("--elf", action="append", type=Path, default=[],
                        help="ELF file to inspect. Can be repeated. Defaults to build/rom*.elf")
    parser.add_argument("--discovery-set", type=Path,
                        help="Existing discovery set to compare instead of running neo-recomp")
    parser.add_argument("--nm-output", type=Path,
                        help="Existing nm output to parse instead of invoking m68k-neogeo-elf-nm")
    parser.add_argument("--symbol-source", choices=["auto", "readelf", "nm"],
                        default="auto",
                        help="ELF symbol source. auto prefers readelf FUNC symbols and falls back to nm")
    parser.add_argument("--build", action="store_true",
                        help="Run make/gmake in the example before extracting symbols")
    parser.add_argument("--no-run", action="store_true",
                        help="Only extract symbols and generate PROMs, do not run neo-recomp")
    parser.add_argument("--tools-dir", type=Path,
                        help="Directory containing m68k-neogeo-elf tools, or its bin subdirectory")
    parser.add_argument("--tool-prefix", default=DEFAULT_TOOL_PREFIX,
                        help="Cross-tool prefix, default m68k-neogeo-elf-")
    parser.add_argument("--exclude-regex", action="append", default=list(DEFAULT_EXCLUDE_RE),
                        help="Function symbol names to ignore. Can be repeated")
    parser.add_argument("--symbols-output", type=Path,
                        help="Write parsed symbol truth here")
    parser.add_argument("--report-output", type=Path,
                        help="Write summary report here")
    parser.add_argument("--list-limit", type=int, default=40,
                        help="Maximum missing/unattributed rows printed in the report")
    parser.add_argument("--fail-on-missing", action="store_true",
                        help="Return nonzero if any symbol entry is missing from discovery")
    parser.add_argument("--fail-on-extra", action="store_true",
                        help="Return nonzero if discovery rows fall outside symbol extents")
    args = parser.parse_args(argv)

    repo_root = args.repo_root.resolve()
    example_dir = args.example_dir.resolve() if args.example_dir else None
    neo_recomp = args.neo_recomp or (repo_root / "build" / "neo-recomp")
    game = args.game or (repo_root / "games" / "ngdevkit_example.toml")
    example_name = example_dir.name if example_dir else "nm-output"
    out_dir = args.out_dir or (repo_root / "build" / "ngdevkit_symbol_oracle" / example_name)
    out_dir.mkdir(parents=True, exist_ok=True)
    excludes = compile_excludes(args.exclude_regex)
    program_map = ProgramMap()

    try:
        layout = parse_prom_layout(example_dir) if example_dir else PromLayout()
        if args.build:
            if example_dir is None:
                raise OracleError("--build requires --example-dir")
            build_example(example_dir)

        if args.nm_output:
            symbols = parse_nm_output(args.nm_output.read_text(errors="replace"),
                                      str(args.nm_output), 0, program_map, excludes)
            extent_symbols = symbols
            elfs: list[Path] = []
        else:
            if example_dir is None:
                raise OracleError("--example-dir or --nm-output is required")
            elfs = discover_elfs(example_dir, layout.build_dir, args.elf)
            if not elfs:
                raise OracleError(
                    f"no ELF files found under {example_dir / layout.build_dir}; build the example first"
                )
            nm = find_tool("nm", args.tool_prefix, args.tools_dir)
            readelf = find_tool("readelf", args.tool_prefix, args.tools_dir)
            symbol_bank_count = len(elfs) if layout.has_prom2 else 0
            if args.symbol_source == "readelf" and readelf is None:
                raise OracleError(
                    f"{args.tool_prefix}readelf not found. Install ngdevkit or pass --tools-dir/--tool-prefix"
                )
            if args.symbol_source == "nm" and nm is None:
                raise OracleError(
                    f"{args.tool_prefix}nm not found. Install ngdevkit or pass --tools-dir/--tool-prefix"
                )
            if args.symbol_source == "auto" and readelf is None and nm is None:
                raise OracleError(
                    f"{args.tool_prefix}readelf/nm not found. Install ngdevkit or pass --tools-dir/--tool-prefix"
                )
            symbols = collect_symbols(nm, readelf, elfs, program_map, excludes,
                                      args.symbol_source, symbol_bank_count)
            extent_symbols = symbols
            if readelf is not None:
                extent_symbols = collect_readelf_exec_sections(readelf, elfs,
                                                               program_map,
                                                               symbol_bank_count)
            elif args.symbol_source != "nm" and nm is not None:
                extent_symbols = collect_nm_extent_symbols(nm, elfs, program_map,
                                                           excludes,
                                                           symbol_bank_count)

        symbols_output = args.symbols_output or (out_dir / "ngdevkit_symbols.txt")
        write_symbol_truth(symbols_output, symbols)

        discovery: set[DiscoveryRow] | None = None
        if args.discovery_set:
            discovery = parse_discovery_file(args.discovery_set)
        elif not args.no_run:
            if not neo_recomp.exists():
                raise OracleError(f"neo-recomp not found: {neo_recomp}")
            if not game.exists():
                raise OracleError(f"game config not found: {game}")
            objcopy = find_tool("objcopy", args.tool_prefix, args.tools_dir)
            if objcopy is None:
                raise OracleError(
                    f"{args.tool_prefix}objcopy not found. Install ngdevkit or pass --tools-dir/--tool-prefix"
                )
            if not elfs:
                raise OracleError("cannot generate PROMs from --nm-output only; pass --discovery-set or --no-run")
            p1, p2 = generate_proms(objcopy, elfs, layout, out_dir)
            discovery_path = out_dir / "ngdevkit_discovery.txt"
            run_discovery(neo_recomp, game, p1, p2, discovery_path)
            discovery = parse_discovery_file(discovery_path)

        if discovery is None:
            print(f"symbols written: {symbols_output} (symbols={len(symbols)})")
            return 0

        result = compare_symbols_to_discovery(symbols, discovery, extent_symbols)
        report = format_report(example_name, symbols, result, args.list_limit)
        if args.report_output:
            args.report_output.parent.mkdir(parents=True, exist_ok=True)
            args.report_output.write_text(report)
        sys.stdout.write(report)

        if args.fail_on_missing and result.missing_symbols:
            return 1
        if args.fail_on_extra and result.unattributed_rows:
            return 1
        return 0
    except OracleError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
