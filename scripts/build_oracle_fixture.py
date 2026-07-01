#!/usr/bin/env python3
"""Build the Phase 0.5 discovery oracle fixture.

The fixture has two outputs:

* a deterministic P-ROM / .neo image generated directly by this script so tests
  do not require a cross-toolchain;
* optional ELF-derived symbol and relocation truth when m68k-elf tools are
  available, checked against the deterministic oracle metadata.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

ROM_SIZE = 0x0A00
FIXED_BASE = 0x000000
FIXED_SIZE = 0x000800
BANK_WINDOW_BASE = 0x200000
BANK_WINDOW_SIZE = 0x000100
BANK0_OFFSET = 0x000800
BANK1_OFFSET = 0x000900

ADDR = {
    "oracle_entry": 0x000200,
    "oracle_direct_func": 0x000220,
    "oracle_object_state_callback": 0x000240,
    "oracle_spawn_callback": 0x000260,
    "oracle_spawn_helper": 0x000280,
    "oracle_state_table_callback": 0x0002A0,
    "oracle_chained_state_callback": 0x0002C0,
    "oracle_tagged_record_callback": 0x0002E0,
    "oracle_state_table": 0x000300,
    "oracle_state_table_next": 0x000310,
    "oracle_tagged_records": 0x000320,
    "oracle_fixed_records": 0x000340,
    "oracle_fixed_record_callback": 0x000380,
    "oracle_object_vector_callback_a": 0x0003A0,
    "oracle_object_vector_callback_b": 0x0003C0,
    "oracle_wrong_tag_data": 0x0003E0,
    "oracle_routine_table_entry_a": 0x000400,
    "oracle_routine_table_entry_b": 0x000410,
    "oracle_routine_table_shared_tail": 0x000430,
    "oracle_object_vector": 0x000440,
    "oracle_vector_data": 0x000460,
    "oracle_bank0_records": 0x200000,
    "oracle_bank0_callback": 0x200020,
    "oracle_bank1_records": 0x200000,
    "oracle_bank1_callback": 0x200020,
}

FUNCTION_SIZES = {
    "oracle_entry": 0x1E,
    "oracle_direct_func": 0x04,
    "oracle_object_state_callback": 0x04,
    "oracle_spawn_callback": 0x04,
    "oracle_spawn_helper": 0x02,
    "oracle_state_table_callback": 0x04,
    "oracle_chained_state_callback": 0x04,
    "oracle_tagged_record_callback": 0x04,
    "oracle_fixed_record_callback": 0x04,
    "oracle_object_vector_callback_a": 0x04,
    "oracle_object_vector_callback_b": 0x04,
    "oracle_routine_table_entry_a": 0x0A,
    "oracle_routine_table_entry_b": 0x08,
    "oracle_routine_table_shared_tail": 0x04,
    "oracle_bank0_callback": 0x04,
    "oracle_bank1_callback": 0x04,
}

SECTION_BASE = {
    ".vectors": 0x000000,
    ".cart_header": 0x000100,
    ".text.entry": 0x000200,
    ".text.direct": 0x000220,
    ".text.object": 0x000240,
    ".text.spawn": 0x000260,
    ".text.spawn_helper": 0x000280,
    ".text.state_a": 0x0002A0,
    ".text.state_b": 0x0002C0,
    ".text.tagged": 0x0002E0,
    ".data.state_table": 0x000300,
    ".data.tagged": 0x000320,
    ".data.fixed": 0x000340,
    ".text.fixed": 0x000380,
    ".text.vector_a": 0x0003A0,
    ".text.vector_b": 0x0003C0,
    ".data.negative_wrong_tag": 0x0003E0,
    ".text.routine_a": 0x000400,
    ".text.routine_b": 0x000410,
    ".text.routine_tail": 0x000430,
    ".data.vector": 0x000440,
    ".data.negative_vector": 0x000460,
    ".data.bank0_records": 0x200000,
    ".text.bank0": 0x200020,
    ".data.bank1_records": 0x210000,
    ".text.bank1": 0x210020,
}

RELOC_NAMES = {
    (".vectors", 0x04): "vector_entry",
    (".cart_header", 0x24): "cart_header_entry",
    (".text.entry", 0x02): "direct_jsr",
    (".text.entry", 0x08): "object_state_install",
    (".text.entry", 0x12): "spawn_install",
    (".text.entry", 0x18): "spawn_helper_call",
    (".text.routine_a", 0x04): "routine_table_call",
    (".text.routine_b", 0x06): "routine_table_branch",
    (".data.state_table", 0x00): "state_table_entry",
    (".data.state_table", 0x04): "state_table_chain_pointer",
    (".data.state_table", 0x10): "state_table_chain_entry",
    (".data.tagged", 0x02): "tagged_record_entry",
    (".data.tagged", 0x12): "wrong_tag_data_pointer",
    (".data.fixed", 0x04): "fixed_record_entry",
    (".data.vector", 0x00): "object_vector_a",
    (".data.vector", 0x04): "object_vector_b",
    (".data.vector", 0x08): "object_vector_data_pointer",
    (".data.bank0_records", 0x00): "bank0_record_entry",
    (".data.bank1_records", 0x00): "bank1_record_entry",
}

TRACE_PCS = [
    0x000200,
    0x000206,
    0x00020C,
    0x000210,
    0x000216,
    0x00021C,
    0x000220,
    0x000222,
    0x000240,
    0x000242,
    0x000260,
    0x000262,
    0x000280,
    0x0002A0,
    0x0002C0,
    0x0002E0,
    0x000380,
    0x0003A0,
    0x0003C0,
    0x000400,
    0x000402,
    0x000408,
    0x000410,
    0x000412,
    0x000414,
    0x000430,
    0x000432,
]


@dataclass(frozen=True, order=True)
class FunctionTruth:
    bank: str
    start: int
    end: int
    name: str


@dataclass(frozen=True, order=True)
class RelocTruth:
    name: str
    site_bank: str
    site: int
    target_bank: str
    target: int
    kind: str


def be16(buf: bytearray, addr: int, value: int) -> None:
    buf[addr] = (value >> 8) & 0xFF
    buf[addr + 1] = value & 0xFF


def be32(buf: bytearray, addr: int, value: int) -> None:
    be16(buf, addr, value >> 16)
    be16(buf, addr + 2, value)


def jsr_abs(buf: bytearray, addr: int, target: int) -> None:
    be16(buf, addr, 0x4EB9)
    be32(buf, addr + 2, target)


def lea_abs_a1(buf: bytearray, addr: int, target: int) -> None:
    be16(buf, addr, 0x43F9)
    be32(buf, addr + 2, target)


def bra_w(buf: bytearray, addr: int, target: int) -> None:
    be16(buf, addr, 0x6000)
    be16(buf, addr + 2, (target - (addr + 2)) & 0xFFFF)


def nop_rts(buf: bytearray, addr: int) -> None:
    be16(buf, addr, 0x4E71)
    be16(buf, addr + 2, 0x4E75)


def build_p_rom() -> bytes:
    rom = bytearray([0xFF] * ROM_SIZE)
    be32(rom, 0x000000, 0x0010F300)
    be32(rom, 0x000004, ADDR["oracle_entry"])
    rom[0x100:0x107] = b"NEO-GEO"
    rom[0x107] = 0
    be16(rom, 0x000122, 0x4EF9)
    be32(rom, 0x000124, ADDR["oracle_entry"])

    entry = ADDR["oracle_entry"]
    jsr_abs(rom, entry, ADDR["oracle_direct_func"])
    lea_abs_a1(rom, entry + 0x06, ADDR["oracle_object_state_callback"])
    be16(rom, entry + 0x0C, 0x2D49)
    be16(rom, entry + 0x0E, 0x0070)
    lea_abs_a1(rom, entry + 0x10, ADDR["oracle_spawn_callback"])
    jsr_abs(rom, entry + 0x16, ADDR["oracle_spawn_helper"])
    be16(rom, entry + 0x1C, 0x4E75)

    for name in [
        "oracle_direct_func",
        "oracle_object_state_callback",
        "oracle_spawn_callback",
        "oracle_state_table_callback",
        "oracle_chained_state_callback",
        "oracle_tagged_record_callback",
        "oracle_fixed_record_callback",
        "oracle_object_vector_callback_a",
        "oracle_object_vector_callback_b",
    ]:
        nop_rts(rom, ADDR[name])
    be16(rom, ADDR["oracle_spawn_helper"], 0x4E75)
    be16(rom, ADDR["oracle_wrong_tag_data"], 0x4E75)
    be16(rom, ADDR["oracle_vector_data"], 0x4E75)

    st = ADDR["oracle_state_table"]
    be32(rom, st, ADDR["oracle_state_table_callback"])
    be32(rom, st + 0x04, ADDR["oracle_state_table_next"])
    be32(rom, st + 0x08, 0xFFFFFFFF)
    be32(rom, st + 0x0C, 0xDEAD0000)
    be32(rom, ADDR["oracle_state_table_next"], ADDR["oracle_chained_state_callback"])
    be32(rom, ADDR["oracle_state_table_next"] + 0x04, 0xDEAD0000)

    tagged = ADDR["oracle_tagged_records"]
    be16(rom, tagged, 0x0800)
    be32(rom, tagged + 0x02, ADDR["oracle_tagged_record_callback"])
    be16(rom, tagged + 0x10, 0x0700)
    be32(rom, tagged + 0x12, ADDR["oracle_wrong_tag_data"])

    fixed = ADDR["oracle_fixed_records"]
    be32(rom, fixed + 0x04, ADDR["oracle_fixed_record_callback"])
    be32(rom, fixed + 0x14, 0xFFFFFFFF)

    routine_a = ADDR["oracle_routine_table_entry_a"]
    be16(rom, routine_a, 0x4E71)
    jsr_abs(rom, routine_a + 0x02, ADDR["oracle_direct_func"])
    be16(rom, routine_a + 0x08, 0x4E75)
    routine_b = ADDR["oracle_routine_table_entry_b"]
    be16(rom, routine_b, 0x4E71)
    be16(rom, routine_b + 0x02, 0x4E71)
    bra_w(rom, routine_b + 0x04, ADDR["oracle_routine_table_shared_tail"])
    be16(rom, ADDR["oracle_routine_table_shared_tail"], 0x4E71)
    be16(rom, ADDR["oracle_routine_table_shared_tail"] + 0x02, 0x4E75)

    vec = ADDR["oracle_object_vector"]
    be32(rom, vec, ADDR["oracle_object_vector_callback_a"])
    be32(rom, vec + 0x04, ADDR["oracle_object_vector_callback_b"])
    be32(rom, vec + 0x08, ADDR["oracle_vector_data"])
    be32(rom, vec + 0x0C, 0xFFFFFFFF)

    be32(rom, BANK0_OFFSET, ADDR["oracle_bank0_callback"])
    nop_rts(rom, BANK0_OFFSET + 0x20)
    be32(rom, BANK1_OFFSET, ADDR["oracle_bank1_callback"])
    nop_rts(rom, BANK1_OFFSET + 0x20)
    return bytes(rom)


def byteswap_words(data: bytes) -> bytes:
    out = bytearray(data)
    for i in range(0, len(out) - 1, 2):
        out[i], out[i + 1] = out[i + 1], out[i]
    return bytes(out)


def write_le32(buf: bytearray, offset: int, value: int) -> None:
    buf[offset:offset + 4] = value.to_bytes(4, "little")


def write_neo(path: Path, p_rom: bytes) -> None:
    header = bytearray(0x1000)
    header[0:4] = b"NEO\x01"
    write_le32(header, 0x04, len(p_rom))
    write_le32(header, 0x08, 0)
    write_le32(header, 0x0C, 0)
    write_le32(header, 0x10, 0)
    write_le32(header, 0x14, 0)
    write_le32(header, 0x18, 0)
    path.write_bytes(bytes(header) + byteswap_words(p_rom))


def fallback_functions() -> list[FunctionTruth]:
    rows = []
    for name, size in FUNCTION_SIZES.items():
        bank = "none"
        if name == "oracle_bank0_callback":
            bank = "0"
        elif name == "oracle_bank1_callback":
            bank = "1"
        rows.append(FunctionTruth(bank, ADDR[name], ADDR[name] + size, name))
    return sorted(rows)


def fallback_relocs() -> list[RelocTruth]:
    def fixed(name: str, site: int, target: int, kind: str) -> RelocTruth:
        return RelocTruth(name, "none", site, "none", target, kind)

    return sorted([
        fixed("vector_entry", 0x000004, ADDR["oracle_entry"], "code"),
        fixed("cart_header_entry", 0x000124, ADDR["oracle_entry"], "code"),
        fixed("direct_jsr", ADDR["oracle_entry"] + 0x02, ADDR["oracle_direct_func"], "code"),
        fixed("object_state_install", ADDR["oracle_entry"] + 0x08, ADDR["oracle_object_state_callback"], "code"),
        fixed("spawn_install", ADDR["oracle_entry"] + 0x12, ADDR["oracle_spawn_callback"], "code"),
        fixed("spawn_helper_call", ADDR["oracle_entry"] + 0x18, ADDR["oracle_spawn_helper"], "code"),
        fixed("routine_table_call", ADDR["oracle_routine_table_entry_a"] + 0x04, ADDR["oracle_direct_func"], "code"),
        fixed("routine_table_branch", ADDR["oracle_routine_table_entry_b"] + 0x06, ADDR["oracle_routine_table_shared_tail"], "code"),
        fixed("state_table_entry", ADDR["oracle_state_table"], ADDR["oracle_state_table_callback"], "code"),
        fixed("state_table_chain_pointer", ADDR["oracle_state_table"] + 0x04, ADDR["oracle_state_table_next"], "data"),
        fixed("state_table_chain_entry", ADDR["oracle_state_table_next"], ADDR["oracle_chained_state_callback"], "code"),
        fixed("tagged_record_entry", ADDR["oracle_tagged_records"] + 0x02, ADDR["oracle_tagged_record_callback"], "code"),
        fixed("wrong_tag_data_pointer", ADDR["oracle_tagged_records"] + 0x12, ADDR["oracle_wrong_tag_data"], "data"),
        fixed("fixed_record_entry", ADDR["oracle_fixed_records"] + 0x04, ADDR["oracle_fixed_record_callback"], "code"),
        fixed("object_vector_a", ADDR["oracle_object_vector"], ADDR["oracle_object_vector_callback_a"], "code"),
        fixed("object_vector_b", ADDR["oracle_object_vector"] + 0x04, ADDR["oracle_object_vector_callback_b"], "code"),
        fixed("object_vector_data_pointer", ADDR["oracle_object_vector"] + 0x08, ADDR["oracle_vector_data"], "data"),
        RelocTruth("bank0_record_entry", "0", ADDR["oracle_bank0_records"], "0", ADDR["oracle_bank0_callback"], "code"),
        RelocTruth("bank1_record_entry", "1", ADDR["oracle_bank1_records"], "1", ADDR["oracle_bank1_callback"], "code"),
    ])


def bank_and_addr(value: int) -> tuple[str, int]:
    if 0x210000 <= value < 0x210100:
        return "1", 0x200000 + (value - 0x210000)
    if 0x200000 <= value < 0x200100:
        return "0", value
    return "none", value


def tool(prefix: str, name: str) -> str | None:
    return shutil.which(prefix + name)


def run(cmd: list[str]) -> str:
    return subprocess.run(cmd, check=True, text=True, capture_output=True).stdout


def build_elf(root: Path, out_dir: Path, prefix: str) -> tuple[Path, Path] | None:
    as_tool = tool(prefix, "as")
    ld_tool = tool(prefix, "ld")
    if not as_tool or not ld_tool:
        return None
    obj = out_dir / "oracle.o"
    elf = out_dir / "oracle.elf"
    subprocess.run([as_tool, "-o", str(obj), str(root / "games" / "oracle" / "oracle.S")], check=True)
    subprocess.run([ld_tool, "-T", str(root / "games" / "oracle" / "oracle.ld"),
                    "-o", str(elf), str(obj)], check=True)
    return obj, elf


def extract_functions(elf: Path, prefix: str) -> list[FunctionTruth]:
    nm_tool = tool(prefix, "nm")
    if not nm_tool:
        raise RuntimeError("nm not found")
    rows: list[FunctionTruth] = []
    for line in run([nm_tool, "-S", "--defined-only", "--numeric-sort", str(elf)]).splitlines():
        parts = line.split()
        if len(parts) != 4:
            continue
        value_s, size_s, kind, name = parts
        if kind not in {"T", "t"} or not name.startswith("oracle_"):
            continue
        value = int(value_s, 16)
        size = int(size_s, 16)
        bank, addr = bank_and_addr(value)
        rows.append(FunctionTruth(bank, addr, addr + size, name))
    return sorted(rows)


def extract_relocs(obj: Path, prefix: str) -> list[RelocTruth]:
    readelf_tool = tool(prefix, "readelf")
    if not readelf_tool:
        raise RuntimeError("readelf not found")
    rows: list[RelocTruth] = []
    current_section: str | None = None
    for line in run([readelf_tool, "--wide", "-r", str(obj)]).splitlines():
        line = line.strip()
        if line.startswith("Relocation section '"):
            current_section = line.split("'", 2)[1]
            if current_section.startswith(".rela"):
                current_section = current_section[5:]
            elif current_section.startswith(".rel"):
                current_section = current_section[4:]
            continue
        if not current_section or not line or line.startswith("Offset"):
            continue
        parts = line.split()
        if len(parts) < 5 or not all(c in "0123456789abcdefABCDEF" for c in parts[0]):
            continue
        offset = int(parts[0], 16)
        sym = parts[4]
        addend = 0
        if len(parts) >= 7 and parts[5] in {"+", "-"}:
            addend = int(parts[6], 16)
            if parts[5] == "-":
                addend = -addend
        if current_section not in SECTION_BASE or sym not in SECTION_BASE:
            continue
        name = RELOC_NAMES.get((current_section, offset))
        if not name:
            raise RuntimeError(f"unexpected oracle reloc: {current_section}+0x{offset:X} -> {sym}")
        site_bank, site = bank_and_addr(SECTION_BASE[current_section] + offset)
        target_bank, target = bank_and_addr(SECTION_BASE[sym] + addend)
        kind = "code" if sym.startswith(".text") else "data"
        rows.append(RelocTruth(name, site_bank, site, target_bank, target, kind))
    return sorted(rows)


def write_functions(path: Path, rows: list[FunctionTruth]) -> None:
    with path.open("w") as out:
        out.write("# bank start end name\n")
        for row in rows:
            out.write(f"{row.bank} 0x{row.start:06X} 0x{row.end:06X} {row.name}\n")


def write_relocs(path: Path, rows: list[RelocTruth]) -> None:
    with path.open("w") as out:
        out.write("# name site_bank site target_bank target kind\n")
        for row in rows:
            out.write(
                f"{row.name} {row.site_bank} 0x{row.site:06X} "
                f"{row.target_bank} 0x{row.target:06X} {row.kind}\n"
            )


def write_trace(path: Path) -> None:
    with path.open("w") as out:
        out.write("# Synthetic MAME-style trace for the oracle's exercised fixed-bank path.\n")
        for pc in TRACE_PCS:
            out.write(f":maincpu: {pc:06X}: oracle\n")


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build the Phase 0.5 oracle fixture")
    parser.add_argument("--out-dir", type=Path, required=True)
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--cross-prefix", default="m68k-elf-")
    parser.add_argument("--require-toolchain", action="store_true")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    root = args.repo_root.resolve()
    out_dir = args.out_dir.resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    p_rom = build_p_rom()
    (out_dir / "oracle.p.bin").write_bytes(p_rom)
    write_neo(out_dir / "oracle.neo", p_rom)
    write_trace(out_dir / "oracle_trace.log")

    functions = fallback_functions()
    relocs = fallback_relocs()

    built = build_elf(root, out_dir, args.cross_prefix)
    if built is None:
        if args.require_toolchain:
            print("m68k-elf toolchain not found", file=sys.stderr)
            return 1
        print("m68k-elf toolchain not found, using deterministic oracle metadata", file=sys.stderr)
    else:
        obj, elf = built
        elf_functions = extract_functions(elf, args.cross_prefix)
        elf_relocs = extract_relocs(obj, args.cross_prefix)
        if elf_functions != functions:
            print("ELF function truth differs from deterministic oracle metadata", file=sys.stderr)
            print("ELF:", elf_functions, file=sys.stderr)
            print("expected:", functions, file=sys.stderr)
            return 1
        if elf_relocs != relocs:
            print("ELF relocation truth differs from deterministic oracle metadata", file=sys.stderr)
            print("ELF:", elf_relocs, file=sys.stderr)
            print("expected:", relocs, file=sys.stderr)
            return 1
        print(f"ELF truth validated: {elf}")

    write_functions(out_dir / "oracle_functions.txt", functions)
    write_relocs(out_dir / "oracle_relocs.txt", relocs)
    print(f"wrote oracle fixture to {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
