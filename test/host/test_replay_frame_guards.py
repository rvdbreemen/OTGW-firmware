#!/usr/bin/env python3
"""TASK-1649 / TASK-1650 host harness: replayed MsgID 27 must not stamp the outside temperature;
a banner on OT-Direct must not call satNotifyPicRestart().

    python test/host/test_replay_frame_guards.py            # working tree
    python test/host/test_replay_frame_guards.py --rev HEAD # any git revision (old code)

Slices the REAL blocks from processOT() in src/OTGW-firmware/OTGW-Core.ino and compiles them with MSVC.
Exit code: 0 PASS, 1 FAIL, 2 harness error.
"""
import argparse
import re
import subprocess
import sys

from test_ot_reserved_range import GEN, HERE, SOURCE, find_vcvars, read_source


def slice_block(lines, anchor_rx, brace_from_anchor_line=True):
    start = next((i for i, l in enumerate(lines) if re.search(anchor_rx, l)), None)
    if start is None:
        raise SystemExit(f"anchor not found: {anchor_rx}")
    depth, out, opened = 0, [], False
    for line in lines[start:]:
        out.append(line)
        depth += line.count("{") - line.count("}")
        if "{" in line:
            opened = True
        if opened and depth == 0:
            return "\n".join(out)
    raise SystemExit("unterminated block")


def slice_banner(lines):
    start = next((i for i, l in enumerate(lines) if "strstr(buf, OTGW_BANNER)" in l), None)
    if start is None:
        raise SystemExit("anchor not found: strstr(buf, OTGW_BANNER)")
    end = next(i for i in range(start, len(lines)) if re.search(r"^\s*if \(!state\.pic\.bAvailable\b", lines[i]))
    return "\n".join(lines[start + 1:end])


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--rev")
    args = ap.parse_args()
    origin = f"git {args.rev}" if args.rev else "working tree"
    lines = read_source(args.rev).splitlines()
    GEN.mkdir(exist_ok=True)
    hdr = f"// GENERATED from {SOURCE} ({origin}) by test_replay_frame_guards.py. Do not edit.\n"
    (GEN / "replay_guard_outside27.inc").write_text(hdr + slice_block(lines, r"^\s*if \(OTdata\.id == 27 &&") + "\n", encoding="utf-8")
    (GEN / "replay_guard_banner.inc").write_text(hdr + slice_banner(lines) + "\n", encoding="utf-8")
    print(f"== sliced the processOT() MsgID 27 stamp and banner statements ({origin})")
    exe = GEN / "test_replay_frame_guards.exe"
    if exe.exists():
        exe.unlink()
    bat = GEN / "_compile_replay_frame_guards.bat"
    gen_fwd = str(GEN).replace("\\", "/")
    bat.write_text("@echo off\r\n"
                   f"call \"{find_vcvars()}\" >nul 2>nul\r\n"
                   f"cl /nologo /EHsc /W3 /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS "
                   f"/Fo:\"{gen_fwd}/\" /Fe:\"{exe}\" \"{HERE / 'test_replay_frame_guards.cpp'}\" /I\"{HERE}\"\r\n",
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
