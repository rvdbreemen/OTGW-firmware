#!/usr/bin/env python3
"""TASK-1179 host harness: does the loop-side banner consumer re-enable a PIC
that boot detection missed?

    python test/host/test_pic_banner_recovery.py            # working tree
    python test/host/test_pic_banner_recovery.py --rev HEAD # any git revision

Slices the REAL applyPICBannerInfo() from src/OTGW-firmware/OTGW-Core.ino and
the REAL isPICEnabled() from src/OTGW-firmware/OTGW-firmware.h by anchor
(declaration up to the matching brace), then compiles them with MSVC into
test_pic_banner_recovery.cpp. Exit code: 0 PASS, 1 FAIL, 2 harness error.
"""
import argparse
import subprocess
import sys
from pathlib import Path

from test_ot_reserved_range import GEN, HERE, REPO, host_cxx_build, slice_by_anchor

SLICES = [
    ("src/OTGW-firmware/OTGW-firmware.h", r"^inline bool isPICEnabled\("),
    ("src/OTGW-firmware/OTGW-Core.ino", r"^static void applyPICBannerInfo\("),
]


def read(path, rev):
    if rev is None:
        return (REPO / path).read_text(encoding="utf-8", errors="replace")
    return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{path}"], capture_output=True,
                          text=True, encoding="utf-8", errors="replace", check=True).stdout


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--rev", help="git revision to slice from (default: working tree)")
    args = ap.parse_args()
    origin = f"git {args.rev}" if args.rev else "working tree"
    parts = [slice_by_anchor(read(path, args.rev).splitlines(), anchor) for path, anchor in SLICES]
    GEN.mkdir(exist_ok=True)
    (GEN / "pic_banner_recovery.inc").write_text(
        f"// GENERATED from OTGW-firmware.h / OTGW-Core.ino ({origin}) by test_pic_banner_recovery.py. Do not edit.\n"
        + "\n\n".join(parts) + "\n", encoding="utf-8")
    print(f"== sliced isPICEnabled() and applyPICBannerInfo() ({origin})")

    exe = GEN / "test_pic_banner_recovery.exe"
    if exe.exists():
        exe.unlink()
    ok, out_ = host_cxx_build([HERE / 'test_pic_banner_recovery.cpp'], exe, includes=[HERE], workdir=GEN)
    if not ok:
        print(out_[-3000:])
        print("COMPILATION FAILED")
        return 2
    r = subprocess.run([str(exe)], capture_output=True, text=True)
    print(r.stdout, end="")
    return r.returncode


if __name__ == "__main__":
    sys.path.insert(0, str(HERE))
    sys.exit(main())
