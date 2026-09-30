#!/usr/bin/env python3
"""TASK-1174 host harness: which OpenTherm data-ids does the firmware suppress
as reserved once a v4.x device is seen?

    python test/host/test_ot_reserved_range.py            # working tree
    python test/host/test_ot_reserved_range.py --rev HEAD # any git revision

The code under test is sliced out of src/OTGW-firmware/OTGW-Core.ino by anchor
(declaration line up to the matching closing brace), never copied, so a change
to the firmware is picked up on the next run and an old revision can be tested
for the old-vs-fix comparison. Compiler: MSVC from Visual Studio 2022 Build
Tools, located with vswhere like test/host/build_and_run.ps1.
Exit code: 0 PASS, 1 FAIL (mismatch against OT spec v4.2), 2 harness error.
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
GEN = HERE / "generated"
SOURCE = "src/OTGW-firmware/OTGW-Core.ino"

ANCHORS = [
    r"^enum OTSpecCompatMode\b",
    r"^static OTSpecCompatMode gOTSpecCompatMode\b",
    r"^static bool isLegacyPreV42CompatibilityId\(",
    r"^static bool useV4xReservedIdRules\(",
    r"^static bool isMsgIdReservedInActiveProfile\(",
]


def read_source(rev, path=None):
    if path is not None:
        return Path(path).read_text(encoding="utf-8", errors="replace")
    if rev is None:
        return (REPO / SOURCE).read_text(encoding="utf-8", errors="replace")
    return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{SOURCE}"],
                          capture_output=True, text=True, encoding="utf-8",
                          errors="replace", check=True).stdout


def slice_by_anchor(lines, pattern):
    """Return the declaration starting at the line matching `pattern`, up to the
    matching closing brace (or the terminating ';' for a one-line statement)."""
    rx = re.compile(pattern)
    for start, line in enumerate(lines):
        if rx.search(line):
            break
    else:
        raise SystemExit(f"anchor not found: {pattern}")
    depth, seen_brace, out = 0, False, []
    for line in lines[start:]:
        out.append(line)
        depth += line.count("{") - line.count("}")
        seen_brace = seen_brace or "{" in line
        if seen_brace and depth == 0:
            return "\n".join(out)
        if not seen_brace and line.rstrip().endswith(";"):
            return "\n".join(out)
    raise SystemExit(f"unterminated declaration for: {pattern}")


def find_vcvars():
    vswhere = Path(r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe")
    if not vswhere.exists():
        raise SystemExit("vswhere.exe not found: Visual Studio Build Tools required")
    root = subprocess.run([str(vswhere), "-latest", "-products", "*", "-property", "installationPath"],
                          capture_output=True, text=True, check=True).stdout.strip()
    vcvars = Path(root) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
    if not vcvars.exists():
        raise SystemExit(f"vcvars64.bat not found under {root}")
    return vcvars


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--rev", help="git revision to slice from (default: working tree)")
    ap.add_argument("--file", help="slice from this OTGW-Core.ino instead (e.g. the 1.x worktree)")
    args = ap.parse_args()

    lines = read_source(args.rev, args.file).splitlines()
    parts = [slice_by_anchor(lines, a) for a in ANCHORS]
    GEN.mkdir(exist_ok=True)
    origin = args.file or (f"git {args.rev}" if args.rev else "working tree")
    (GEN / "ot_reserved_range.inc").write_text(
        f"// GENERATED from {SOURCE} ({origin}) by test_ot_reserved_range.py. Do not edit.\n"
        + "\n\n".join(parts) + "\n", encoding="utf-8")
    print(f"== sliced {len(parts)} declarations from {SOURCE} ({origin})")

    exe = GEN / "test_ot_reserved_range.exe"
    if exe.exists():
        exe.unlink()
    bat = GEN / "_compile_ot_reserved_range.bat"
    gen_fwd = str(GEN).replace("\\", "/")
    bat.write_text("@echo off\r\n"
                   f"call \"{find_vcvars()}\" >nul 2>nul\r\n"
                   f"cl /nologo /EHsc /W3 /std:c++17 /D_CRT_SECURE_NO_WARNINGS "
                   f"/Fo:\"{gen_fwd}/\" /Fe:\"{exe}\" \"{HERE / 'test_ot_reserved_range.cpp'}\" /I\"{HERE}\"\r\n",
                   encoding="ascii")
    c = subprocess.run(["cmd.exe", "/c", str(bat)], capture_output=True, text=True, errors="replace")
    if c.returncode != 0 or not exe.exists():
        print(c.stdout[-3000:], c.stderr[-2000:])
        print("COMPILATION FAILED")
        return 2
    r = subprocess.run([str(exe)], capture_output=True, text=True)
    print(r.stdout, end="")
    return r.returncode


if __name__ == "__main__":
    sys.exit(main())
