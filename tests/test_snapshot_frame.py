#!/usr/bin/env python3
from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

WORK_RAM_BYTES = 0x10000
PALETTE_RAM_BYTES = 0x4000
VRAM_BYTES = 0x20000
FIX_BASE = 0x7000


def write_neo(path: Path) -> None:
    header = bytearray(0x1000)
    header[0:4] = b"NEO\x01"
    p = b"\x00\x00"
    s = bytearray(0x20)
    # Native fix layout: line 0 bytes are 0x10,0x18,0x00,0x08.
    # Low nibble is the first pixel in each pair for our renderer.
    s[0x10] = 0x21
    for offset, data in [
        (0x04, p),
        (0x08, bytes(s)),
        (0x0C, b""),
        (0x10, b""),
        (0x14, b""),
        (0x18, b""),
    ]:
        header[offset : offset + 4] = struct.pack("<I", len(data))
    path.write_bytes(bytes(header) + p + bytes(s))


def write_snapshot(snapshot_dir: Path) -> None:
    snapshot_dir.mkdir(parents=True, exist_ok=True)
    (snapshot_dir / "work_ram.bin").write_bytes(bytes(WORK_RAM_BYTES))

    palette = bytearray(PALETTE_RAM_BYTES)
    # Palette 1, colors 1 and 2: red and green.
    palette[0x11 * 2 : 0x11 * 2 + 2] = bytes.fromhex("4f00")
    palette[0x12 * 2 : 0x12 * 2 + 2] = bytes.fromhex("20f0")
    (snapshot_dir / "palette_ram.bin").write_bytes(bytes(palette))

    vram = bytearray(VRAM_BYTES)
    word_offset = FIX_BASE * 2
    vram[word_offset : word_offset + 2] = bytes.fromhex("1000")
    (snapshot_dir / "vram_be.bin").write_bytes(bytes(vram))


def read_ppm(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    magic, dims, maxval, pixels = data.split(b"\n", 3)
    assert magic == b"P6"
    assert maxval == b"255"
    width, height = map(int, dims.split())
    return width, height, pixels


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: test_snapshot_frame.py <neo-render-snapshot> <work-dir>", file=sys.stderr)
        return 2
    render_tool = Path(sys.argv[1])
    work_dir = Path(sys.argv[2]) / "snapshot_frame_test"
    snapshot_dir = work_dir / "snapshot"
    neo_path = work_dir / "synthetic.neo"
    out_path = work_dir / "fix.ppm"
    work_dir.mkdir(parents=True, exist_ok=True)
    write_neo(neo_path)
    write_snapshot(snapshot_dir)

    subprocess.run(
        [str(render_tool), str(snapshot_dir), str(neo_path), str(out_path)],
        check=True,
    )
    width, height, pixels = read_ppm(out_path)
    assert (width, height) == (320, 256)
    assert pixels[0:3] == bytes((255, 0, 0))
    assert pixels[3:6] == bytes((0, 255, 0))
    assert pixels[6:9] == bytes((0, 0, 0))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
