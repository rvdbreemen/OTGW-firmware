#!/usr/bin/env python3
"""Generate a sha256sum-compatible SHA256SUMS.txt for OTGW-firmware release artefacts.

Intended workflow: after building the release artefacts (firmware-esp32.bin,
firmware-nodemcuv2.bin, littlefs-esp32.bin, littlefs-nodemcuv2.bin, ELF files,
etc.), the project owner runs this once over the directory holding the
artefacts before uploading them to a GitHub release. The generated
SHA256SUMS.txt is uploaded as a release artefact alongside the binaries so
that downloaders can verify integrity with `sha256sum -c SHA256SUMS.txt`
(Linux/macOS) or `Get-FileHash -Algorithm SHA256` (Windows PowerShell).

Stdlib only (hashlib, pathlib, argparse). Hashes are computed streaming in
8 KB chunks so the script never loads a whole image into memory.

Usage:
    python tools/generate_release_sha256sums.py --input-dir <dir> [--output SHA256SUMS.txt]
"""
from __future__ import annotations

import argparse
import hashlib
import sys
from pathlib import Path

ARTEFACT_GLOBS = ("*.bin", "*.zip", "*.tar.gz", "*.elf")
CHUNK_SIZE = 8 * 1024  # 8 KB streaming chunks


def sha256_streaming(path: Path) -> str:
    """Return hex SHA-256 of *path*, read in CHUNK_SIZE pieces."""
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(CHUNK_SIZE), b""):
            h.update(chunk)
    return h.hexdigest()


def collect_artefacts(input_dir: Path, output_name: str) -> list[Path]:
    """Non-recursive glob of release artefacts, with the output file excluded."""
    seen: set[Path] = set()
    for pattern in ARTEFACT_GLOBS:
        for p in input_dir.glob(pattern):
            if p.is_file() and p.name != output_name:
                seen.add(p.resolve())
    return sorted(seen, key=lambda p: p.name)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--input-dir", required=True, type=Path,
                        help="Directory containing release artefacts (.bin/.zip/.tar.gz/.elf)")
    parser.add_argument("--output", default="SHA256SUMS.txt",
                        help="Output filename inside --input-dir (default: SHA256SUMS.txt)")
    args = parser.parse_args(argv)

    input_dir = args.input_dir.resolve()
    if not input_dir.exists():
        print(f"error: --input-dir {input_dir} does not exist", file=sys.stderr)
        return 2
    if not input_dir.is_dir():
        print(f"error: --input-dir {input_dir} is not a directory", file=sys.stderr)
        return 2

    artefacts = collect_artefacts(input_dir, args.output)
    if not artefacts:
        print(f"warning: no artefacts matching {ARTEFACT_GLOBS} in {input_dir}", file=sys.stderr)
        return 1

    output_path = input_dir / args.output
    with output_path.open("w", encoding="utf-8", newline="\n") as f:
        for path in artefacts:
            digest = sha256_streaming(path)
            f.write(f"{digest}  {path.name}\n")

    # Human-readable summary table.
    name_width = max(len(p.name) for p in artefacts)
    print(f"{'file'.ljust(name_width)}  {'size (MB)':>9}  sha256")
    print(f"{'-' * name_width}  {'-' * 9}  {'-' * 16}")
    for path in artefacts:
        size_mb = path.stat().st_size / (1024 * 1024)
        digest = sha256_streaming(path)
        print(f"{path.name.ljust(name_width)}  {size_mb:9.2f}  {digest[:16]}...")
    print(f"\nwrote {output_path} ({len(artefacts)} artefact{'s' if len(artefacts) != 1 else ''})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
