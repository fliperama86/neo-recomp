#!/usr/bin/env python3
import subprocess
import sys
from pathlib import Path

WORK_RAM_BYTES = 0x10000
PALETTE_RAM_BYTES = 0x4000
VRAM_BYTES = 0x20000


def write_pattern(path: Path, size: int, mul: int) -> None:
    path.write_bytes(bytes((i * mul) & 0xFF for i in range(size)))


def read_ppm_header(path: Path) -> tuple[int, int]:
    with path.open("rb") as f:
        magic = f.readline().strip()
        dims = f.readline().strip().split()
        maxv = f.readline().strip()
    assert magic == b"P6", (path, magic)
    assert maxv == b"255", (path, maxv)
    return int(dims[0]), int(dims[1])


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: test_snapshot_debug.py <source-dir> <work-dir>", file=sys.stderr)
        return 2
    source_dir = Path(sys.argv[1])
    work_dir = Path(sys.argv[2]) / "snapshot_debug_test"
    snapshot_dir = work_dir / "snapshot"
    out_dir = work_dir / "out"
    snapshot_dir.mkdir(parents=True, exist_ok=True)

    write_pattern(snapshot_dir / "work_ram.bin", WORK_RAM_BYTES, 3)
    write_pattern(snapshot_dir / "palette_ram.bin", PALETTE_RAM_BYTES, 5)

    vram = bytearray(VRAM_BYTES)
    for word in (0x0001, 0x0123, 0xBEEF, 0x7C00):
        offset = (word & 0xFFFF) * 2 % VRAM_BYTES
        vram[offset] = word >> 8
        vram[offset + 1] = word & 0xFF
    (snapshot_dir / "vram_be.bin").write_bytes(vram)
    (snapshot_dir / "summary.txt").write_text("dispatches=4\n")

    subprocess.run(
        [
            sys.executable,
            str(source_dir / "tools" / "render_snapshot_debug.py"),
            str(snapshot_dir),
            "--out-dir",
            str(out_dir),
        ],
        check=True,
    )

    assert read_ppm_header(out_dir / "work_ram_bytes.ppm") == (256, 256)
    assert read_ppm_header(out_dir / "vram_words.ppm") == (256, 256)
    assert read_ppm_header(out_dir / "vram_nonzero.ppm") == (256, 256)
    assert read_ppm_header(out_dir / "palette_debug.ppm") == (512, 256)
    report = (out_dir / "report.txt").read_text()
    assert "work_ram bytes=65536" in report
    assert "vram_be bytes=131072" in report
    assert (out_dir / "snapshot_summary.txt").read_text() == "dispatches=4\n"
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
