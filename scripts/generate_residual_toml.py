#!/usr/bin/env python3
"""Generate a TOML residual seed file from discovery set differences."""

from __future__ import annotations

import argparse
from pathlib import Path


def read_set(path: Path) -> set[int]:
    values: set[int] = set()
    for lineno, raw in enumerate(path.read_text().splitlines(), 1):
        line = raw.split('#', 1)[0].strip()
        if not line:
            continue
        try:
            values.add(int(line, 0) & 0x00FFFFFF)
        except ValueError as exc:
            raise SystemExit(f"{path}:{lineno}: invalid address: {raw!r}") from exc
    return values


def write_toml(path: Path, missing: list[int], source: str) -> None:
    with path.open('w') as out:
        out.write('# Auto-generated residual discovery seeds.\n')
        out.write('# Review before committing; prefer structural descriptors when possible.\n')
        out.write(f'# Source: {source}\n\n')
        out.write('[functions]\n')
        out.write('extra = [\n')
        for addr in missing:
            out.write(f'  0x{addr:06X},\n')
        out.write(']\n')


def main() -> int:
    parser = argparse.ArgumentParser(
        description='Generate [functions].extra residual TOML from discovery set differences.',
    )
    parser.add_argument('--required', required=True, type=Path,
                        help='Required/golden discovery set, one address per line')
    parser.add_argument('--covered', required=True, type=Path,
                        help='Discovery set already covered by descriptors, one address per line')
    parser.add_argument('--output', required=True, type=Path,
                        help='Output TOML residual file')
    args = parser.parse_args()

    required = read_set(args.required)
    covered = read_set(args.covered)
    missing = sorted(required - covered)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    write_toml(args.output, missing, f'{args.required} minus {args.covered}')
    print(f'residual TOML: required={len(required)} covered={len(covered)} residual={len(missing)} output={args.output}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
