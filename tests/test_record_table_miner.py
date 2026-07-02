#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def write16(buf: bytearray, addr: int, value: int) -> None:
    buf[addr] = (value >> 8) & 0xFF
    buf[addr + 1] = value & 0xFF


def write32(buf: bytearray, addr: int, value: int) -> None:
    write16(buf, addr, value >> 16)
    write16(buf, addr + 2, value)


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: test_record_table_miner.py REPO_ROOT BUILD_DIR", file=sys.stderr)
        return 2
    repo = Path(sys.argv[1])
    build = Path(sys.argv[2])
    work = build / "record_table_miner"
    work.mkdir(parents=True, exist_ok=True)

    prom = bytearray(0x200)
    write16(prom, 0x80, 0x4E75)
    write16(prom, 0x90, 0x4E75)
    write16(prom, 0xA0, 0x4E75)
    write32(prom, 0x120 + 4, 0x00000080)
    write32(prom, 0x132 + 4, 0x00000090)
    write32(prom, 0x144 + 4, 0x000000A0)
    write32(prom, 0x180 + 4, 0x000000B0)
    prom_path = work / "synthetic.p"
    prom_path.write_bytes(prom)

    entries_path = work / "entries.txt"
    entries_path.write_text("0x000080\n0x000090\n")
    out_path = work / "mined.toml"

    script = repo / "scripts" / "mine_record_tables.py"
    result = subprocess.run(
        [
            sys.executable,
            str(script),
            "--p1",
            str(prom_path),
            "--discovery-entries",
            str(entries_path),
            "--fixed-size",
            "0x100",
            "--bank-window-size",
            "0x100",
            "--scan",
            "bank:*",
            "--stride",
            "0x12",
            "--callback-offset",
            "4",
            "--target-range",
            "0x80-0xC0",
            "--min-entries",
            "3",
            "--min-known",
            "2",
            "--output",
            str(out_path),
        ],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
    )
    assert "runs=1" in result.stderr, result.stderr

    text = out_path.read_text()
    assert "stride = 0x12" in text
    assert "callback_offsets = [0x4]" in text
    assert '"auto:bank:*:0x200020-0x200056"' in text
    assert "entries=3 known=2" in text
    assert "0x000080-0x0000A0" in text
    assert "0x200080" not in text
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
