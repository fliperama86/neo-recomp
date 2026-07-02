#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
import sys
from pathlib import Path


def load_module(root: Path):
    script = root / "scripts" / "ngdevkit_symbol_oracle.py"
    spec = importlib.util.spec_from_file_location("ngdevkit_symbol_oracle", script)
    if spec is None or spec.loader is None:
        raise AssertionError("could not load ngdevkit_symbol_oracle.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def test_parse_nm_filters_and_banks(mod) -> None:
    nm_text = """
00000100 0000000A T _start
0000010A 00000008 t local_func
00000112 T zero_size
00000113 00000002 T odd_ignored
00100000 00000004 T ram_ignored
00200010 00000006 T banked_func
00200020 00000002 W weak_text
00000120 00000004 D data_object
00000124 00000004 T __internal
"""
    symbols = mod.parse_nm_output(nm_text, "fake-nm", elf_bank=3)
    by_name = {sym.name: sym for sym in symbols}

    assert set(by_name) == {"_start", "local_func", "zero_size", "banked_func", "weak_text"}
    assert by_name["_start"].key.bank == "none"
    assert by_name["_start"].key.addr == 0x100
    assert by_name["_start"].end == 0x10A
    assert by_name["zero_size"].end == 0x114
    assert by_name["banked_func"].key.bank == "3"
    assert by_name["weak_text"].key.bank == "3"


def test_parse_readelf_uses_function_symbols_only(mod) -> None:
    readelf_text = """
Symbol table '.symtab' contains 8 entries:
   Num:    Value  Size Type    Bind   Vis      Ndx Name
    10: 00000100    10 FUNC    GLOBAL DEFAULT    1 main
    11: 0000010a     0 FUNC    LOCAL  DEFAULT    1 helper
    12: 00000112     4 OBJECT  GLOBAL DEFAULT    1 data_rom
    13: 00000116     0 NOTYPE  GLOBAL DEFAULT    1 label
    14: 00200020     6 FUNC    GLOBAL DEFAULT   11 banked
    14: 00200040  0x10 FUNC    GLOBAL DEFAULT   11 banked_big
    15: 00100000     4 FUNC    GLOBAL DEFAULT    4 ram_ignored
    16: 00000113     2 FUNC    GLOBAL DEFAULT    1 odd_ignored
    17: 00000118     2 FUNC    GLOBAL DEFAULT  UND undef_ignored
"""
    symbols = mod.parse_readelf_symbols(readelf_text, "fake-readelf", elf_bank=2)
    by_name = {sym.name: sym for sym in symbols}

    assert set(by_name) == {"main", "helper", "banked", "banked_big"}
    assert by_name["main"].key.bank == "none"
    assert by_name["main"].end == 0x10A
    assert by_name["helper"].end == 0x10C
    assert by_name["banked"].key.bank == "2"
    assert by_name["banked_big"].end == 0x200050


def test_readelf_bank_count_matches_discovery_bank_identity(mod) -> None:
    text = "10: 00280004    12 FUNC    GLOBAL DEFAULT   12 bank_window_func\n"

    single_bank = mod.parse_readelf_symbols(text, "fake-readelf",
                                            elf_bank=0, bank_count=1)
    multi_bank = mod.parse_readelf_symbols(text, "fake-readelf",
                                           elf_bank=0, bank_count=2)

    assert single_bank[0].key.bank == "none"
    assert multi_bank[0].key.bank == "0"


def test_parse_readelf_exec_sections(mod) -> None:
    text = """
Section Headers:
  [ 1] .text.boot        PROGBITS        00000000 002000 000438 00  AX  0   0  4
  [ 2] .rodata           PROGBITS        00000438 002438 000160 00   A  0   0  2
  [12] .text2            PROGBITS        00200000 0c8000 000254 00  AX  0   0  4
"""
    sections = mod.parse_readelf_exec_sections(text, "fake-readelf",
                                               elf_bank=1, bank_count=2)
    by_name = {sym.name: sym for sym in sections}

    assert set(by_name) == {"<section:.text.boot>", "<section:.text2>"}
    assert by_name["<section:.text.boot>"].key.bank == "none"
    assert by_name["<section:.text2>"].key.bank == "1"
    assert by_name["<section:.text2>"].end == 0x200254


def test_discovery_compare_reports_missing_and_unattributed(mod) -> None:
    symbols = mod.parse_nm_output(
        """
00000100 0000000A T _start
0000010A 00000008 T local_func
00200010 00000006 T banked_func
""",
        "fake-nm",
        elf_bank=0,
    )
    discovery = mod.parse_discovery_text(
        """
0x000100
0x000102
bank:0 0x200010
0x000900
"""
    )

    result = mod.compare_symbols_to_discovery(symbols, discovery)
    assert result.symbol_count == 3
    assert result.discovery_count == 4
    assert {sym.name for sym in result.discovered_symbols} == {"_start", "banked_func"}
    assert [sym.name for sym in result.missing_symbols] == ["local_func"]
    assert result.unattributed_rows == (mod.DiscoveryRow("none", 0x900),)

    report = mod.format_report("fixture", symbols, result, list_limit=8)
    assert "symbols=3 discovered_symbols=2 missing=1 discovery_rows=4 unattributed_rows=1" in report
    assert "local_func" in report
    assert "0x000900" in report


def test_compare_can_use_separate_soundness_extents(mod) -> None:
    required = mod.parse_readelf_symbols(
        "10: 00000200    10 FUNC    GLOBAL DEFAULT    1 main\n",
        "fake-readelf",
        elf_bank=0,
    )
    extents = mod.parse_nm_output(
        "00000100 00000120 T boot\n00000200 0000000a T main\n",
        "fake-nm",
        elf_bank=0,
    )
    discovery = mod.parse_discovery_text("0x000100\n0x000102\n0x000200\n")

    result = mod.compare_symbols_to_discovery(required, discovery, extents)
    assert result.missing_symbols == ()
    assert result.unattributed_rows == ()


def test_parse_prom_layout_ignores_commented_prom2(tmp_path: Path, mod) -> None:
    tmp_path.mkdir(parents=True, exist_ok=True)
    (tmp_path / "Makefile").write_text("BUILDDIR=out\n")
    (tmp_path / "rom.mk").write_text(
        """
BUILDDIR?=build
PROMSIZE=524288
PROM1=$(ROM)/game-p1.p1
# PROM2=$(ROM)/game-p2.p2
# PROM2SIZE=1048576
"""
    )
    layout = mod.parse_prom_layout(tmp_path)
    assert layout.build_dir == "out"
    assert layout.prom_size == 524288
    assert not layout.has_prom2
    assert layout.prom2_size == 0


def test_parse_prom_layout_active_prom2(tmp_path: Path, mod) -> None:
    tmp_path.mkdir(parents=True, exist_ok=True)
    (tmp_path / "rom.mk").write_text(
        """
BUILDDIR?=build
PROMSIZE=1048576
PROM1=$(ROM)/game-p1.p1
PROM2=$(ROM)/game-p2.p2
PROM2SIZE=2097152
"""
    )
    layout = mod.parse_prom_layout(tmp_path)
    assert layout.build_dir == "build"
    assert layout.prom_size == 1048576
    assert layout.has_prom2
    assert layout.prom2_size == 2097152


def test_normalize_bank_blob_handles_prefixed_objcopy_output(tmp_path: Path, mod) -> None:
    tmp_path.mkdir(parents=True, exist_ok=True)
    bank = bytes([0x12, 0x34, 0x56, 0x78])
    path = tmp_path / "bank.bin"
    path.write_bytes(bytes([0xFF]) * mod.DEFAULT_BANK_WINDOW_BASE + bank)
    mod.normalize_bank_blob(path, len(bank))
    assert path.read_bytes() == bank


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: test_ngdevkit_symbol_oracle.py REPO_ROOT BUILD_DIR", file=sys.stderr)
        return 2
    root = Path(sys.argv[1])
    build_dir = Path(sys.argv[2]) / "ngdevkit_symbol_oracle_test"
    build_dir.mkdir(parents=True, exist_ok=True)
    mod = load_module(root)

    test_parse_nm_filters_and_banks(mod)
    test_parse_readelf_uses_function_symbols_only(mod)
    test_readelf_bank_count_matches_discovery_bank_identity(mod)
    test_parse_readelf_exec_sections(mod)
    test_discovery_compare_reports_missing_and_unattributed(mod)
    test_compare_can_use_separate_soundness_extents(mod)
    test_parse_prom_layout_ignores_commented_prom2(build_dir / "layout_no_prom2", mod)
    test_parse_prom_layout_active_prom2(build_dir / "layout_prom2", mod)
    test_normalize_bank_blob_handles_prefixed_objcopy_output(build_dir / "normalize", mod)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
