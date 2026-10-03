#!/usr/bin/env python3
"""TASK-1195 host harness: the SAT PID derivative timer, through the real SATpid.ino.

    python test/host/test_sat_pid_derivative.py                  # working tree, plus mutants
    python test/host/test_sat_pid_derivative.py --old-rev <rev>  # plus the OLD side
    python test/host/test_sat_pid_derivative.py --report <rev>   # one revision, report only

test_sat_pid_derivative.cpp is compiled against code cut out of one revision, never
copied by hand: SATpid.ino (the whole file, with its function anchors checked),
satGetEffectiveHeatingSystem() and the zone PID step (SAT_MIN_SETPOINT, satZones[],
satZonePidExclude(), satZonePidStep()) from SATcontrol.ino, and SATtypes.h, so every
default in settings.sat and state.sat is the firmware's own. Each side gets its own directory
under test/host/generated/, and src/ is not on the include path, so one revision's
header can never leak into the other side's build.

The driver runs a stepped heat-up on the control-tick grid (0.1 C every 300 s, the
step phase swept 0..29 s, loop latency off and on) next to a transcription of pid.py
(a reference oracle, not code under test), and prints one "CHECK <id> PASS|FAIL" line
per check.

FIX side (working tree or --rev): every check must pass.
OLD side (--old-rev): the pid.py rules the fix keeps (C1, C2, C2b, C3, C5a, C6, C7) and
the harness self-check H1 must pass; the defect checks must fail: the derivative timer
(A2_reach, A2_dt, A2_oracle), the derivative on the device path through the TASK-894
room EMA and on a fast sensor (E1, R1), and the deadband boundary at an error of 0.1
(B1, B1d, B2). C4, C5b and C6b are reported for OLD, not judged.
Mutants of the FIX source must each fail their named case: without that, a pass of the
case proves nothing. Every substitution must match exactly once and change the text.

Exit code: 0 PASS, 1 FAIL, 2 harness error. A slice or compile failure is a harness
error, never a reproduction.
"""
import argparse
import re
import subprocess
import sys

from test_ot_reserved_range import GEN, HERE, REPO, find_vcvars, slice_by_anchor
from test_pic_banner_dispatch import slice_range

FW = "src/OTGW-firmware/"
PID_INO, CONTROL_INO, TYPES_H = FW + "SATpid.ino", FW + "SATcontrol.ino", FW + "SATtypes.h"
PID_ANCHORS = [r"^static void _pidUpdateDerivative\(", r"^float satPidUpdate\(", r"^void satPidReset\("]
HSYS_ANCHOR = r"^static uint8_t satGetEffectiveHeatingSystem\("
ZONE_ANCHORS = [r"^static const float SAT_MIN_SETPOINT\b", r"^static SATZoneState satZones\[",
                r"^static inline float satZonePidExclude\(", r"^static float satZonePidStep\("]
# The EMA block in satControlLoop(): from its comment through the bare block that follows.
EMA_START, EMA_BLOCK = r"^\s*// TASK-894 \(George\): EMA low-pass on the RAW room sensor", r"^\s*\{\s*$"

KEEP = ["C1", "C2", "C2b", "C3", "C5a", "C6", "C7", "H1"]   # pid.py rules + self-check: pass on both sides
# The TASK-1195 defects: fail on OLD, pass on FIX.
DEFECT = ["A2_reach", "A2_dt", "A2_oracle", "E1", "R1", "B1", "B1d", "B2"]

ROUNDED = "return roundf((target - room) * 1000.0f) / 1000.0f;"

# (id, what, [(old text, new text), ...], case that must fail)
MUTANTS = [
    ("MF", "deadband freeze removed", [("if (fabsf(error) <= deadband) {", "if (false) {")], "C1"),
    ("MC", "raw cap removed", [("if (fabsf(rawDeriv) >= SAT_PID_DERIVATIVE_CAP) {", "if (false) {")], "C2"),
    ("MA", "alpha fixed at 0.5", [("float alpha = deltaTime / (SAT_PID_UPDATE_INTERVAL + deltaTime);",
                                   "float alpha = 0.5f;")], "C3"),
    ("MB", "derivative reference is the last temperature seen (pid.py's literal last_temperature)",
     [("float tempDelta = roomTemp - _pid_derivRefTemp;", "float tempDelta = roomTemp - _pid_lastRoomTemp;")], "E1"),
    ("MR", "error rounding removed (primary PID)", [(ROUNDED, "return target - room;")], "B1"),
    ("MRz", "error rounding removed (zone PID)", [(ROUNDED, "return target - room;")], "B2"),
]


def read(rev, path):
    if rev is None:
        return (REPO / path).read_text(encoding="utf-8", errors="replace")
    return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{path}"], capture_output=True,
                          text=True, encoding="utf-8", errors="replace", check=True).stdout


def once(lines, pattern):
    hits = [i for i, l in enumerate(lines) if re.search(pattern, l)]
    if len(hits) != 1:
        raise SystemExit(f"anchor must match once, got {len(hits)}: {pattern}")


def prepare(side, rev, mutations=()):
    """Write the side's slices and SATtypes.h into generated/<side>/; return that directory."""
    origin = f"git {rev}" if rev else "working tree"
    pid = read(rev, PID_INO)
    pid_lines = pid.splitlines()
    for a in PID_ANCHORS:
        once(pid_lines, a)
    for old, new in mutations:
        if pid.count(old) != 1:
            raise SystemExit(f"mutant text must match once, got {pid.count(old)}: {old!r}")
        pid = pid.replace(old, new)
    ctl_lines = read(rev, CONTROL_INO).splitlines()
    for a in [HSYS_ANCHOR] + ZONE_ANCHORS:
        once(ctl_lines, a)
    hsys = slice_by_anchor(ctl_lines, HSYS_ANCHOR)
    zone = "\n\n".join(slice_by_anchor(ctl_lines, a) for a in ZONE_ANCHORS)
    ema = slice_range(ctl_lines, EMA_START, EMA_BLOCK)

    d = GEN / f"sat_pid_{side}"
    d.mkdir(parents=True, exist_ok=True)
    head = f"// GENERATED from {{}} ({origin}) by test_sat_pid_derivative.py. Do not edit.\n"
    (d / "sat_pid_slice.inc").write_text(head.format(PID_INO) + pid + "\n", encoding="utf-8")
    (d / "sat_hsys_slice.inc").write_text(head.format(CONTROL_INO) + hsys + "\n", encoding="utf-8")
    (d / "sat_zone_slice.inc").write_text(head.format(CONTROL_INO) + zone + "\n", encoding="utf-8")
    (d / "sat_ema_slice.inc").write_text(head.format(CONTROL_INO) + ema + "\n", encoding="utf-8")
    (d / "SATtypes.h").write_text(read(rev, TYPES_H), encoding="utf-8")
    return d


def build_and_run(side, d):
    """Compile the driver against directory d. Return (exit code, stdout, checks) or None on a harness error."""
    exe = d / "test_sat_pid_derivative.exe"
    if exe.exists():
        exe.unlink()
    bat = d / "_compile.bat"
    fwd = str(d).replace("\\", "/")
    bat.write_text("@echo off\r\n"
                   f"call \"{find_vcvars()}\" >nul 2>nul\r\n"
                   f"cl /nologo /EHsc /W3 /std:c++17 /D_CRT_SECURE_NO_WARNINGS "
                   f"/Fo:\"{fwd}/\" /Fe:\"{exe}\" \"{HERE / 'test_sat_pid_derivative.cpp'}\" "
                   f"/I\"{d}\" /I\"{HERE / 'sat_pid_shim'}\"\r\n", encoding="ascii")
    c = subprocess.run(["cmd.exe", "/c", str(bat)], capture_output=True, text=True, errors="replace")
    if c.returncode != 0 or not exe.exists():
        print(c.stdout[-3000:], c.stderr[-2000:])
        print(f"COMPILATION FAILED ({side})")
        return None
    r = subprocess.run([str(exe)], capture_output=True, text=True, errors="replace")
    checks = dict(re.findall(r"^CHECK (\S+) (PASS|FAIL)", r.stdout, re.M))
    return r.returncode, r.stdout, checks


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--rev", help="FIX side: git revision to test (default: working tree)")
    ap.add_argument("--old-rev", help="OLD side: git revision from before the fix")
    ap.add_argument("--report", metavar="REV", help="build one revision ('worktree' for the working tree), print, judge nothing")
    args = ap.parse_args()

    if args.report:
        rev = None if args.report == "worktree" else args.report
        res = build_and_run("report", prepare("report", rev))
        if res is None:
            return 2
        print(res[1], end="")
        return 0

    verdict = []
    if args.old_rev:
        print(f"===== OLD: {args.old_rev}")
        res = build_and_run("old", prepare("old", args.old_rev))
        if res is None:
            return 2
        _, out, checks = res
        print(out, end="")
        for cid in KEEP + DEFECT:
            if cid not in checks:
                print(f"harness error: OLD printed no CHECK {cid}")
                return 2
        bad = [c for c in KEEP if checks[c] != "PASS"] + [c for c in DEFECT if checks[c] != "FAIL"]
        verdict.append(("OLD keeps the pid.py rules and shows the defect", not bad, bad))

    print(f"===== FIX: {args.rev or 'working tree'}")
    res = build_and_run("fix", prepare("fix", args.rev))
    if res is None:
        return 2
    code, out, checks = res
    print(out, end="")
    if not checks:
        print("harness error: FIX printed no CHECK lines")
        return 2
    failed = [c for c, v in checks.items() if v != "PASS"]
    verdict.append(("FIX passes every check", code == 0 and not failed, failed))

    print("===== mutants of the FIX source")
    for mid, what, subs, case in MUTANTS:
        res = build_and_run(f"mut_{mid}", prepare(f"mut_{mid}", args.rev, subs))
        if res is None:
            return 2
        got = res[2].get(case, "missing")
        print(f"   {mid} ({what}): {case} {got}")
        verdict.append((f"mutant {mid} fails {case}", got == "FAIL", [] if got == "FAIL" else [case]))

    print("===== verdict")
    for name, ok, detail in verdict:
        print(f"   {'PASS' if ok else 'FAIL'}  {name}" + (f"  {detail}" if detail else ""))
    return 0 if all(ok for _, ok, _ in verdict) else 1


if __name__ == "__main__":
    sys.exit(main())
