#!/usr/bin/env python3
"""TASK-1665 host harness: the diagnostic recorder core (ADR-213), compiled from the REAL header.

    python test/host/test_diag_recorder.py              # the header as it is in the working tree
    python test/host/test_diag_recorder.py --mutant     # also build a copy WITHOUT the refuse-below-5120 B guard

src/OTGW-firmware/diagRecorderStuff.h is included as is (no slicing, no copy of its logic). The mutant
copy removes the one line marked DIAG-GUARD-HEAP in the generated include directory only; it
must fail exactly the cases that depend on the start rule (F7 and F9) and pass the rest.
Compiler: MSVC through vswhere. Exit: 0 PASS, 1 FAIL, 2 error.
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

from test_ot_reserved_range import GEN, HERE, REPO, find_vcvars

HEADER = REPO / "src" / "OTGW-firmware" / "diagRecorderStuff.h"
MUTANTS = [("DIAG-GUARD-HEAP", ["F7", "F9"], "start guard"), ("DIAG-GUARD-BRIDGE", ["F15"], "bridge flag")]


def build_and_run(label, inc_dir, out_dir):
    out_dir.mkdir(parents=True, exist_ok=True)
    exe = out_dir / "test_diag_recorder.exe"
    if exe.exists():
        exe.unlink()
    bat = out_dir / "_compile.bat"
    fwd = str(out_dir).replace("\\", "/")
    bat.write_text("@echo off\r\n"
                   f"call \"{find_vcvars()}\" >nul 2>nul\r\n"
                   f"cl /nologo /EHsc /W3 /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS "
                   f"/Fo:\"{fwd}/\" /Fe:\"{exe}\" \"{HERE / 'test_diag_recorder.cpp'}\" /I\"{inc_dir}\"\r\n",
                   encoding="ascii")
    c = subprocess.run(["cmd.exe", "/c", str(bat)], capture_output=True, text=True, errors="replace")
    if c.returncode != 0 or not exe.exists():
        print(c.stdout[-4000:], c.stderr[-2000:])
        print(f"COMPILATION FAILED ({label})")
        sys.exit(2)
    r = subprocess.run([str(exe)], capture_output=True, text=True, errors="replace")
    print(f"== {label} ==")
    print(r.stdout, end="")
    cases = dict(re.findall(r"^CASE (\S+) (pass|FAIL)\b", r.stdout, re.M))
    return r.returncode, cases


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--mutant", action="store_true", help="also build the header without the start guard")
    args = ap.parse_args()
    base = GEN / "diag_recorder"
    rc, cases = build_and_run("REAL header (working tree)", HEADER.parent, base / "real")
    ok_real = rc == 0 and len(cases) >= 15 and all(v == "pass" for v in cases.values())
    if not args.mutant:
        print("RESULT:", "PASS" if ok_real else "FAIL")
        return 0 if ok_real else 1
    text = HEADER.read_text(encoding="utf-8")
    checks = [("REAL passes every case", ok_real)]
    for mark, red, what in MUTANTS:
        kept = [l for l in text.splitlines(keepends=True) if mark not in l]
        if len(kept) != len(text.splitlines(keepends=True)) - 1:
            print(f"expected exactly one {mark} line in the header")
            return 2
        mut_inc = base / ("mutant_inc_" + mark)
        mut_inc.mkdir(parents=True, exist_ok=True)
        (mut_inc / HEADER.name).write_text("".join(kept), encoding="utf-8", newline="")
        mrc, mcases = build_and_run(f"MUTANT header ({what} removed)", mut_inc, base / ("mutant_" + mark))
        checks += [
            (f"MUTANT {what}: fails " + ",".join(red), all(mcases.get(c) == "FAIL" for c in red)),
            (f"MUTANT {what}: passes the other cases", all(v == "pass" for k, v in mcases.items() if k not in red)),
            (f"MUTANT {what}: exits 1", mrc == 1),
        ]
    print("== real-vs-mutant verdict ==")
    for text_, ok in checks:
        print(f"  {text_}: {'yes' if ok else 'NO'}")
    ok = all(ok for _, ok in checks)
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
