#!/usr/bin/env python3
"""Render simple debug images from a neo-recomp headless snapshot.

This is intentionally *not* a Neo Geo renderer. It converts CPU-visible snapshot
buffers into deterministic, dependency-free PPM images so we can inspect whether
headless execution produced plausible nonzero state before building accurate
fixed/sprite/video output.
"""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Iterable

WORK_RAM_BYTES = 0x10000
PALETTE_RAM_BYTES = 0x4000
VRAM_BYTES = 0x20000
VRAM_WORDS = 0x10000


def read_exact(path: Path, expected_size: int) -> bytes:
    data = path.read_bytes()
    if len(data) != expected_size:
        raise SystemExit(
            f"{path} has {len(data)} bytes; expected {expected_size}"
        )
    return data


def checksum_bytes(data: bytes) -> int:
    return sum(data) & 0xFFFFFFFF


def checksum_words_be(data: bytes) -> int:
    total = 0
    for i in range(0, len(data), 2):
        total += (data[i] << 8) | data[i + 1]
    return total & 0xFFFFFFFF


def count_nonzero_bytes(data: bytes) -> int:
    return sum(1 for b in data if b)


def count_nonzero_words_be(data: bytes) -> int:
    return sum(
        1 for i in range(0, len(data), 2) if ((data[i] << 8) | data[i + 1]) != 0
    )


def scale5(v: int) -> int:
    return (v & 0x1F) * 255 // 31


def neogeo_palette_word_to_rgb(word: int) -> tuple[int, int, int]:
    """Approximate Neo Geo palette decode for debug swatches.

    Neo Geo palette words store four high bits per channel plus separate low
    bits. Bit 15 is treated here as a simple dark/normal modifier for visibility;
    this is diagnostic output, not an authoritative video implementation.
    """

    r5 = ((word >> 7) & 0x1E) | ((word >> 14) & 0x01)
    g5 = ((word >> 3) & 0x1E) | ((word >> 13) & 0x01)
    b5 = ((word << 1) & 0x1E) | ((word >> 12) & 0x01)
    r, g, b = scale5(r5), scale5(g5), scale5(b5)
    if word & 0x8000:
        r = (r * 2) // 3
        g = (g * 2) // 3
        b = (b * 2) // 3
    return r, g, b


def raw_word_to_rgb(word: int) -> tuple[int, int, int]:
    if word == 0:
        return 0, 0, 0
    # Hash-like raw view: makes tilemap/control-word structure visible without
    # pretending to render Neo Geo video semantics.
    r = ((word >> 10) & 0x1F) * 255 // 31
    g = ((word >> 5) & 0x1F) * 255 // 31
    b = (word & 0x1F) * 255 // 31
    return r, g, b


def write_ppm(path: Path, width: int, height: int, pixels: Iterable[tuple[int, int, int]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as f:
        f.write(f"P6\n{width} {height}\n255\n".encode("ascii"))
        buf = bytearray()
        for r, g, b in pixels:
            buf.extend((r & 0xFF, g & 0xFF, b & 0xFF))
        expected = width * height * 3
        if len(buf) != expected:
            raise SystemExit(f"internal image size mismatch for {path}: {len(buf)} != {expected}")
        f.write(buf)


def render_work_ram(data: bytes, path: Path) -> None:
    pixels = ((b, b, b) for b in data)
    write_ppm(path, 256, 256, pixels)


def render_vram_words(data: bytes, path: Path) -> None:
    pixels = (
        raw_word_to_rgb((data[i] << 8) | data[i + 1])
        for i in range(0, len(data), 2)
    )
    write_ppm(path, 256, 256, pixels)


def render_vram_nonzero(data: bytes, path: Path) -> None:
    def pixels() -> Iterable[tuple[int, int, int]]:
        for i in range(0, len(data), 2):
            word = (data[i] << 8) | data[i + 1]
            if word == 0:
                yield 0, 0, 0
            else:
                # White-ish with low byte tint to show clusters.
                yield 128 + ((word >> 8) & 0x7F), 128 + ((word >> 4) & 0x7F), 128 + (word & 0x7F)

    write_ppm(path, 256, 256, pixels())


def render_palette(data: bytes, path: Path, swatch: int = 4, cols: int = 128) -> None:
    words = [(data[i] << 8) | data[i + 1] for i in range(0, len(data), 2)]
    rows = (len(words) + cols - 1) // cols
    width = cols * swatch
    height = rows * swatch

    def pixels() -> Iterable[tuple[int, int, int]]:
        for y in range(height):
            row = y // swatch
            for x in range(width):
                col = x // swatch
                idx = row * cols + col
                yield neogeo_palette_word_to_rgb(words[idx]) if idx < len(words) else (0, 0, 0)

    write_ppm(path, width, height, pixels())


def copy_summary(snapshot_dir: Path, out_dir: Path) -> None:
    src = snapshot_dir / "summary.txt"
    if src.exists():
        (out_dir / "snapshot_summary.txt").write_text(src.read_text())


def write_report(out_dir: Path, work_ram: bytes, palette_ram: bytes, vram: bytes) -> None:
    lines = [
        "neo-recomp snapshot debug report",
        "",
        f"work_ram bytes={len(work_ram)} nonzero={count_nonzero_bytes(work_ram)} checksum=${checksum_bytes(work_ram):08X}",
        f"palette_ram bytes={len(palette_ram)} nonzero={count_nonzero_bytes(palette_ram)} checksum=${checksum_bytes(palette_ram):08X}",
        f"vram_be bytes={len(vram)} nonzero_words={count_nonzero_words_be(vram)} checksum=${checksum_words_be(vram):08X}",
        "",
        "Images are diagnostic PPMs, not accurate Neo Geo rendering:",
        "- work_ram_bytes.ppm: 256x256 grayscale work RAM bytes",
        "- vram_words.ppm: 256x256 raw VRAM-word color hash",
        "- vram_nonzero.ppm: 256x256 nonzero VRAM mask/tint",
        "- palette_debug.ppm: approximate palette swatches",
        "",
    ]
    (out_dir / "report.txt").write_text("\n".join(lines))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("snapshot_dir", type=Path, help="directory containing work_ram.bin, palette_ram.bin, vram_be.bin")
    parser.add_argument("--out-dir", type=Path, help="output directory (default: <snapshot_dir>/debug_images)")
    args = parser.parse_args()

    snapshot_dir = args.snapshot_dir
    out_dir = args.out_dir or (snapshot_dir / "debug_images")
    out_dir.mkdir(parents=True, exist_ok=True)

    work_ram = read_exact(snapshot_dir / "work_ram.bin", WORK_RAM_BYTES)
    palette_ram = read_exact(snapshot_dir / "palette_ram.bin", PALETTE_RAM_BYTES)
    vram = read_exact(snapshot_dir / "vram_be.bin", VRAM_BYTES)

    render_work_ram(work_ram, out_dir / "work_ram_bytes.ppm")
    render_vram_words(vram, out_dir / "vram_words.ppm")
    render_vram_nonzero(vram, out_dir / "vram_nonzero.ppm")
    render_palette(palette_ram, out_dir / "palette_debug.ppm")
    write_report(out_dir, work_ram, palette_ram, vram)
    copy_summary(snapshot_dir, out_dir)

    print(f"wrote snapshot debug images to {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
