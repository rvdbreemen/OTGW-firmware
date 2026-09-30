#!/usr/bin/env python3
"""TASK-1180 host harness: does processOT()'s banner recovery keep the combo
board mode of an S3 Mini Pro?

    python test/host/test_banner_board_mode.py            # working tree
    python test/host/test_banner_board_mode.py --rev HEAD # any git revision

Slices the REAL recovery block ('if (!state.pic.bAvailable) {' up to its
matching brace) that follows the OTGW_BANNER test in processOT(), from
src/OTGW-firmware/OTGW-Core.ino, and compiles it with MSVC into
test_banner_board_mode.cpp. Exit code: 0 PASS, 1 FAIL, 2 harness error.
"""
import argparse
import re
import subprocess
import sys

from test_ot_reserved_range import GEN, HERE, REPO, SOURCE, find_vcvars, read_source


def slice_recovery_block(lines):
    start = next((i for i, l in enumerate(lines) if "strstr(buf, OTGW_BANNER)" in l), None)
    if start is None:
        raise SystemExit("anchor not found: strstr(buf, OTGW_BANNER)")
    rx = re.compile(r"^\s*if \(!state\.pic\.bAvailable\) \{")
    begin = next((i for i in range(start, len(lines)) if rx.search(lines[i])), None)
    if begin is None:
        raise SystemExit("recovery block not found after the OTGW_BANNER test")
    depth, out = 0, []
    for line in lines[begin:]:
        out.append(line)
        depth += line.count("{") - line.count("}")
        if depth == 0:
            return "\n".join(out)
    raise SystemExit("unterminated recovery block")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--rev", help="git revision to slice from (default: working tree)")
    args = ap.parse_args()
    origin = f"git {args.rev}" if args.rev else "working tree"
    block = slice_recovery_block(read_source(args.rev).splitlines())
    GEN.mkdir(exist_ok=True)
    (GEN / "banner_board_mode.inc").write_text(
        f"// GENERATED from {SOURCE} ({origin}) by test_banner_board_mode.py. Do not edit.\n" + block + "\n",
        encoding="utf-8")
    print(f"== sliced the processOT() banner recovery block ({origin})")

    exe = GEN / "test_banner_board_mode.exe"
    if exe.exists():
        exe.unlink()
    bat = GEN / "_compile_banner_board_mode.bat"
    gen_fwd = str(GEN).replace("\\", "/")
    bat.write_text("@echo off\r\n"
                   f"call \"{find_vcvars()}\" >nul 2>nul\r\n"
                   f"cl /nologo /EHsc /W3 /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS "
                   f"/Fo:\"{gen_fwd}/\" /Fe:\"{exe}\" \"{HERE / 'test_banner_board_mode.cpp'}\" /I\"{HERE}\"\r\n",
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
