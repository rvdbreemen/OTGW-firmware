#!/usr/bin/env python3
"""
Generate the release assets that must be attached to a GitHub release DRAFT.

Why a script and not workflow steps: this repository publishes immutable
releases. Once a release is published, GitHub refuses every asset upload
("Cannot upload asset X to an immutable release"), so a workflow that fires on
`release: published` can never attach anything. Every asset therefore has to be
on the draft before it is published, which means it has to be produced locally
alongside the build. See docs/process/RELEASE_PROCESS.md, Phase 7.

Produces, from the binaries in build/:
  SHA256SUMS                                integrity manifest, verified by the flash scripts
  RELEASE_ASSETS.md                         what each asset is, and how to report a bug
  OTGW-firmware-<version>-flash-bundle.zip  self-contained download

and prints the full asset list for `gh release create`.

Usage:
    python scripts/make_release_assets.py --version 1.7.4
    python scripts/make_release_assets.py --version 1.7.4 --out-dir dist/assets
    python scripts/make_release_assets.py --version 1.7.4 --print-gh-args
"""

import argparse
import hashlib
import shutil
import sys
import zipfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

# Copied into the bundle. Path is relative to the repository root.
BUNDLE_DOCS = [
    Path("flash_otgw.sh"),
    Path("flash_otgw.bat"),
    Path("README.md"),
    Path("docs/guides/FLASH_GUIDE.md"),
]

# Copied into the bundle under capture/, and attached as individual assets
# (the .bat ones) so a bug report can carry a real log.
CAPTURE_SCRIPTS = [
    Path("scripts/capture-mqtt-debug.bat"),
    Path("scripts/capture-usb-serial.bat"),
]
CAPTURE_EXTRA = [Path("scripts/capture-settings.example.json")]


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def find_binaries(build_dir: Path, version: str) -> tuple[Path, Path]:
    """Locate the firmware and filesystem image for this version.

    The build stamps a git hash into the filename, so the exact name is not
    known ahead of time. Matching on the version prefix keeps a stale binary
    from an earlier version out of the release.
    """
    firmware = sorted(build_dir.glob(f"OTGW-firmware-{version}+*.ino.bin"))
    filesystem = sorted(build_dir.glob(f"OTGW-firmware.{version}+*.littlefs.bin"))

    for label, matches in (("firmware (*.ino.bin)", firmware),
                           ("filesystem (*.littlefs.bin)", filesystem)):
        if not matches:
            sys.exit(f"ERROR: no {label} for version {version} in {build_dir}. "
                     f"Run the build first.")
        if len(matches) > 1:
            names = ", ".join(m.name for m in matches)
            sys.exit(f"ERROR: {len(matches)} candidates for {label}: {names}. "
                     f"Clean build/ so exactly one binary per version remains.")

    return firmware[0], filesystem[0]


def write_sha256sums(out_dir: Path, binaries: list[Path]) -> Path:
    """Write the manifest in `sha256sum -c` format.

    The flash scripts strip a leading '*' from the filename field, so both the
    text-mode ("hash  name") and binary-mode ("hash *name") spellings parse.
    """
    sums_path = out_dir / "SHA256SUMS"
    lines = [f"{sha256_of(binary)} *{binary.name}\n" for binary in binaries]
    sums_path.write_text("".join(lines), encoding="utf-8", newline="\n")
    return sums_path


def write_release_assets_md(out_dir: Path, version: str) -> Path:
    tag = f"v{version}"
    body = f"""# What is in this release ({tag})

## Just want to flash it?

Download **OTGW-firmware-{version}-flash-bundle.zip**. It contains both
binaries, both flash scripts, the flashing guide and the checksums, so it
is the only file most people need.

Flash **both** the firmware and the filesystem. Some releases change the
web interface, and flashing only the firmware leaves the old interface in
place. Your settings are preserved.

## The individual files

| Asset | What it is |
|---|---|
| `OTGW-firmware-{version}+<githash>.ino.bin` | The firmware. Flash to the ESP8266. |
| `OTGW-firmware.{version}+<githash>.littlefs.bin` | The filesystem: web interface and assets. Flash alongside the firmware. |
| `OTGW-firmware-{version}-flash-bundle.zip` | Everything below, in one download. Start here. |
| `flash_otgw.bat` | Flash helper for Windows. |
| `flash_otgw.sh` | Flash helper for Linux and macOS. |
| `SHA256SUMS` | Checksums for the two binaries. The flash scripts verify against this automatically. |
| `capture-mqtt-debug.bat` | Diagnostic capture, Windows. Run this before reporting a bug. |
| `capture-usb-serial.bat` | Diagnostic capture over USB, for a device that will not come up on WiFi. |
| `RELEASE_ASSETS.md` | This file. |

## Verifying a download

```
sha256sum -c SHA256SUMS          # Linux/macOS
certutil -hashfile <file> SHA256 # Windows, compare by eye
```

The flash scripts do this for you when they download a release themselves.

## Reporting a problem

A log makes the difference between a guess and a diagnosis. Please run a
capture and attach the single transcript file it produces:

1. Run `capture-mqtt-debug.bat` (it will ask for your gateway address and
   MQTT broker details; `capture-settings.example.json` in the bundle shows
   the format if you prefer to pre-fill them).
2. Reproduce the problem while it runs, then stop it with Q.
3. Attach the `transcript-*.txt` it wrote. It bundles the telnet log, the
   MQTT stream, the browser console and the crash log into one file.

Report on Discord in #beta-testing, or open an issue at
https://github.com/rvdbreemen/OTGW-firmware/issues

Captures may contain your WiFi SSID, MQTT username and broker address.
They do not contain your MQTT password. Review before posting publicly.
"""
    path = out_dir / "RELEASE_ASSETS.md"
    path.write_text(body, encoding="utf-8", newline="\n")
    return path


def build_bundle(out_dir: Path, version: str, binaries: list[Path],
                 sums: Path, assets_md: Path) -> Path:
    """Zip everything a user needs into one download.

    Entries keep the `OTGW-firmware-<version>-flash-bundle/` prefix so the zip
    unpacks into its own directory instead of scattering files.
    """
    bundle_name = f"OTGW-firmware-{version}-flash-bundle"
    zip_path = out_dir / f"{bundle_name}.zip"
    staging = out_dir / bundle_name

    if staging.exists():
        shutil.rmtree(staging)
    (staging / "capture").mkdir(parents=True)

    for rel in BUNDLE_DOCS:
        source = REPO_ROOT / rel
        if not source.is_file():
            sys.exit(f"ERROR: bundle input missing: {rel}")
        shutil.copy2(source, staging / source.name)

    for rel in CAPTURE_SCRIPTS + CAPTURE_EXTRA:
        source = REPO_ROOT / rel
        if not source.is_file():
            sys.exit(f"ERROR: capture script missing: {rel}")
        shutil.copy2(source, staging / "capture" / source.name)

    for binary in binaries:
        shutil.copy2(binary, staging / binary.name)
    shutil.copy2(sums, staging / sums.name)
    shutil.copy2(assets_md, staging / assets_md.name)

    if zip_path.exists():
        zip_path.unlink()
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as archive:
        for entry in sorted(staging.rglob("*")):
            archive.write(entry, entry.relative_to(out_dir))

    shutil.rmtree(staging)
    return zip_path


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate release assets for a GitHub release draft.")
    parser.add_argument("--version", required=True,
                        help="Release version without the leading v (e.g. 1.7.4)")
    parser.add_argument("--build-dir", default=str(REPO_ROOT / "build"),
                        help="Directory holding the built binaries (default: build/)")
    parser.add_argument("--out-dir", default=str(REPO_ROOT / "build" / "release-assets"),
                        help="Where to write the generated assets")
    parser.add_argument("--print-gh-args", action="store_true",
                        help="Print only the asset paths, space separated, for gh release create")
    args = parser.parse_args()

    version = args.version.lstrip("v")
    build_dir = Path(args.build_dir).resolve()
    out_dir = Path(args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    firmware, filesystem = find_binaries(build_dir, version)
    binaries = [firmware, filesystem]

    sums = write_sha256sums(out_dir, binaries)
    assets_md = write_release_assets_md(out_dir, version)
    bundle = build_bundle(out_dir, version, binaries, sums, assets_md)

    # The complete set that must be attached to the DRAFT. Nothing can be added
    # once the release is published.
    all_assets = binaries + [
        REPO_ROOT / "flash_otgw.sh",
        REPO_ROOT / "flash_otgw.bat",
        sums,
        assets_md,
        REPO_ROOT / CAPTURE_SCRIPTS[0],
        REPO_ROOT / CAPTURE_SCRIPTS[1],
        bundle,
    ]

    if args.print_gh_args:
        print(" ".join(f'"{path}"' for path in all_assets))
        return 0

    print(f"Generated release assets for v{version} in {out_dir}")
    print()
    print(sums.read_text(encoding="utf-8").rstrip())
    print()
    print(f"{len(all_assets)} assets to attach to the draft:")
    for path in all_assets:
        print(f"  {path.name}  ({path.stat().st_size} bytes)")
    print()
    print("Attach every one of them to the DRAFT. A published release is "
          "immutable and will reject any later upload.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
