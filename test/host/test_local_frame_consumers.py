#!/usr/bin/env python3
"""TASK-1185 host harness: does a frame the gateway made itself still count as
boiler evidence outside the two unsupported bitmaps?

    python test/host/test_local_frame_consumers.py                  # working tree only
    python test/host/test_local_frame_consumers.py --old-rev HEAD   # OLD vs FIX, with mutants

The frame chain is the one test_boiler_unsupported_origin.py compiles from the
real sources (bridgeFrameToParser() or dispatchOTGWInputLine(), the frame queue,
processOT(), evaluateOTBusLiveness()); this harness reuses its slice lists and
adds the SAT gate: isPICEnabled(), isOTDirectEnabled(), otDirectBoilerPresent(),
satBoilerHardwarePresent() and satNotifyBoilerFrameSeen(). The driver is built
once per board model (combo, classic, OTGW32), because the SAT gate reads
HAS_PIC and HAS_DIRECT_OT.

Each case dumps four consumer parts, A (acknowledged bitmaps), L (OT log
suffix), N (SAT boiler-detected flag) and H (SAT availability gate), plus every
other bitmap, the boiler liveness flag and the queue trace. With --old-rev the
run passes only when:
  1. FIX passes every case of every model, and each model's case list is exact.
  2. OLD fails every defect case, and its dump differs from FIX in exactly the
     parts that case's frames touch (DEFECT below) and nowhere else.
  3. OLD passes every control case with a dump byte-identical to FIX.
  4. OLD and FIX queue traces are identical: both sides saw the same frames.
  5. Each mutant (one fix undone in the FIX slices) fails exactly its cases.
A compile or slicing failure never counts as OLD reproducing the defect.
Exit code: 0 PASS, 1 FAIL, 2 harness error.
"""
import argparse
import re
import subprocess
import sys

from test_boiler_unsupported_origin import CORE_SLICES, OTD, OTD_SLICES, TYPES, cut
from test_ot_reserved_range import GEN, HERE, find_vcvars
from test_pic_banner_dispatch import read

FW_H = "src/OTGW-firmware/OTGW-firmware.h"
HW_H = "src/OTGW-firmware/Hardwaretypes.h"
SAT = "src/OTGW-firmware/SATcontrol.ino"

HW_SLICES = [(HW_H, r"^enum OTGWHardwareMode\b", None)]
OTD_EXTRA = [(OTD, r"^static bool\s+otBoilerCacheValid\[128\];", r"^static bool\s+otBoilerCacheValid\[128\];")]
GATE_SLICES = [
    (FW_H, r"^inline bool isPICEnabled\(\) \{", None),
    (FW_H, r"^inline bool isOTDirectEnabled\(\) \{", None),
    (OTD, r"^bool otDirectBoilerPresent\(\) \{", None),
    (SAT, r"^static bool satDebugForceBoilerPresent = false;", r"^static bool satDebugForceBoilerPresent = false;"),
    (SAT, r"^bool satBoilerHardwarePresent\(\)", None),
    (SAT, r"^void satNotifyBoilerFrameSeen\(\)", None),
]
OUTPUTS = [("types.inc", TYPES), ("hw.inc", HW_SLICES), ("core.inc", CORE_SLICES),
           ("otd.inc", OTD_SLICES + OTD_EXTRA), ("gate.inc", GATE_SLICES)]

MODELS = {"combo": 1, "classic": 2, "otgw32": 3}
# Defect cases (combo build) and the dump parts their frames change: A acked
# bitmaps, L log suffix, N SAT edge flag, H SAT gate.
DEFECT = {"A1": "A", "A2": "A", "A3": "ANH", "A4": "ANH", "A5": "A",
          "L1": "L", "L2": "L", "L3": "LNH", "N1": "LNH", "N2": "ANH", "H1": "ANH", "H2": "H"}
# Control cases per model: their own check holds on both sides. The value lists
# the parts that still differ: O1 runs real loopback frames on an OTGW32, so the
# acknowledged bitmap (A) and the SAT edge flag (N) change there as on the combo,
# while the gate it checks (H) does not.
CONTROL = {
    "combo": {c: "" for c in ["AC1", "AC2", "AC3", "AC4", "AC5", "AC6", "LC1", "LC2", "LC3",
                              "NC1", "NC2", "NC3", "HC1", "HC2", "HC3", "HC4"]},
    "classic": {"K1": "", "K2": "", "K3": ""},
    "otgw32": {"O1": "AN", "O2": "", "O3": ""},
}
PARTS = ["A", "L", "N", "H", "other", "trace"]

# One fix undone at a time, in the FIX slices: (id, what, file, [(old, new)], cases it must fail).
MUTANTS = [
    ("MA", "acknowledged read counts a local frame again", "core.inc",
     [("if (!OTdata.bLocalAnswer && (boilerAckedRead[idx] & mask) == 0) {",
       "if ((boilerAckedRead[idx] & mask) == 0) {")], ["A1", "A3", "A5"]),
    ("MB", "acknowledged write counts a local frame again", "core.inc",
     [("if (!OTdata.bLocalAnswer && (boilerAckedWrite[idx] & mask) == 0) {",
       "if ((boilerAckedWrite[idx] & mask) == 0) {")], ["A2", "A4"]),
    ("ML", "the log gives a local type-7 the boiler suffix again", "core.inc",
     [("if (OTdata.bLocalAnswer) {", "if (false) {")], ["L1", "L2", "L3"]),
    ("MN", "the SAT edge hook takes a loopback B again", "core.inc",
     [("if (!localAnswer) satNotifyBoilerFrameSeen();", "satNotifyBoilerFrameSeen();")], ["N1", "N2"]),
    ("MH", "the SAT gate reads bBoilerState in OT-Direct mode again", "gate.inc",
     [("if (!isOTDirectEnabled() && state.otBus.bBoilerState) return true;",
       "if (state.otBus.bBoilerState) return true;")], ["H1", "H2"]),
]


def export(rev, out_dir):
    out_dir.mkdir(parents=True, exist_ok=True)
    origin = f"git {rev}" if rev else "working tree"
    sources = {}
    for name, slices in OUTPUTS:
        parts = [f"// GENERATED by test_local_frame_consumers.py from the {origin}. Do not edit."]
        for path, start_rx, end_rx in slices:
            if path not in sources:
                sources[path] = read(path, rev).splitlines()
            text, first, last = cut(sources[path], start_rx, end_rx)
            parts.append(f"// ---- {path}:{first}-{last} ----\n{text}")
        (out_dir / name).write_text("\n".join(parts) + "\n", encoding="utf-8")


def build_and_run(label, out_dir, model):
    exe = out_dir / "test_local_frame_consumers.exe"
    if exe.exists():
        exe.unlink()
    bat = out_dir / "_compile.bat"
    fwd = str(out_dir).replace("\\", "/")
    bat.write_text("@echo off\r\n"
                   f"call \"{find_vcvars()}\" >nul 2>nul\r\n"
                   f"cl /nologo /EHsc /W3 /std:c++17 /Od /utf-8 /D_CRT_SECURE_NO_WARNINGS /DMODEL={MODELS[model]} "
                   f"/Fo:\"{fwd}/\" /Fe:\"{exe}\" \"{HERE / 'test_local_frame_consumers.cpp'}\" "
                   f"/I\"{out_dir}\"\r\n", encoding="ascii")
    c = subprocess.run(["cmd.exe", "/c", str(bat)], capture_output=True, text=True, errors="replace")
    if c.returncode != 0 or not exe.exists():
        print(c.stdout[-4000:], c.stderr[-2000:])
        print(f"COMPILATION FAILED ({label})")
        sys.exit(2)
    r = subprocess.run([str(exe), str(out_dir)], capture_output=True, text=True)
    cases = dict(re.findall(r"^CASE (\S+) (pass|FAIL)\b", r.stdout, re.M))
    dump = {}
    dump_file = out_dir / "cases.txt"
    if dump_file.exists():
        rx = re.compile(r"^(\S+) A=(\S+) L=(\S+) N=(\S+) H=(\S+) other=(\S+) trace=(\S*)$")
        for line in dump_file.read_text(encoding="utf-8").splitlines():
            m = rx.match(line)
            if m:
                dump[m.group(1)] = dict(zip(PARTS, m.groups()[1:]))
    return r.returncode, cases, dump, r.stdout


def expected_cases(model):
    return sorted((list(DEFECT) if model == "combo" else []) + list(CONTROL[model]))


def run_side(label, rev, tag):
    """Slice once per model, build and run; returns {model: (rc, cases, dump, stdout)}."""
    out = {}
    for model in MODELS:
        d = GEN / "local_frame_consumers" / f"{model}-{tag}"
        export(rev, d)
        out[model] = build_and_run(f"{label} {model}", d, model)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--old-rev", help="also build OLD from this git revision, compare, and run the mutants")
    args = ap.parse_args()

    fix = run_side("FIX (working tree)", None, "fix")
    for model, (rc, cases, _, stdout) in fix.items():
        print(f"== FIX (working tree), {model} build ==")
        print(stdout, end="")
    fix_ok = all(rc == 0 and sorted(cases) == expected_cases(m) and all(v == "pass" for v in cases.values())
                 for m, (rc, cases, _, _) in fix.items())
    checks = [("FIX passes every case of every model, case lists exact", fix_ok)]
    if not args.old_rev:
        ok = fix_ok
        print("RESULT:", "PASS" if ok else "FAIL")
        return 0 if ok else 1

    old = run_side(f"OLD (git {args.old_rev})", args.old_rev, "old")
    for model, (rc, cases, _, stdout) in old.items():
        print(f"== OLD (git {args.old_rev}), {model} build ==")
        print(stdout, end="")

    def diff_parts(model, case):
        o, f = old[model][2].get(case), fix[model][2].get(case)
        if o is None or f is None:
            return None
        return "".join(p for p in "ALNH" if o[p] != f[p]), o["other"] == f["other"], o["trace"] == f["trace"]

    def matches(model, case, want_old, parts):
        d = diff_parts(model, case)
        return old[model][1].get(case) == want_old and d is not None and d[0] == parts and d[1] and d[2], d

    bad_defect, bad_control = [], []
    for case, parts in DEFECT.items():
        ok, d = matches("combo", case, "FAIL", parts)
        if not ok:
            bad_defect.append(f"{case}({d[0] if d else '?'})")
    for model, cs in CONTROL.items():
        for case, parts in cs.items():
            ok, d = matches(model, case, "pass", parts)
            if not ok:
                bad_control.append(f"{model}:{case}({d[0] if d else '?'})")
    checks += [
        ("OLD combo build exits 1, classic and OTGW32 builds exit 0",
         old["combo"][0] == 1 and old["classic"][0] == 0 and old["otgw32"][0] == 0),
        (f"defect cases fail on OLD and differ from FIX in exactly their parts, trace and other bitmaps identical"
         f" (not matching: {', '.join(bad_defect) or 'none'})", not bad_defect),
        (f"control cases pass on OLD, differing from FIX in exactly their listed parts, trace and other bitmaps"
         f" identical (not matching: {', '.join(bad_control) or 'none'})", not bad_control),
    ]

    for mid, what, name, reps, want_fail in MUTANTS:
        d = GEN / "local_frame_consumers" / f"combo-{mid}"
        export(None, d)
        text = (d / name).read_text(encoding="utf-8")
        for a, b in reps:
            if text.count(a) != 1:
                print(f"MUTANT {mid}: pattern must match exactly once, got {text.count(a)}: {a}")
                return 2
            text = text.replace(a, b)
        (d / name).write_text(text, encoding="utf-8")
        rc, cases, _, _ = build_and_run(f"mutant {mid}", d, "combo")
        failed = sorted(c for c, v in cases.items() if v == "FAIL")
        print(f"== {mid}: {what} -> failed cases: {', '.join(failed) or 'none'}")
        checks.append((f"{mid} ({what}) fails exactly {','.join(want_fail)}", failed == sorted(want_fail)))

    print("== old-vs-fix verdict ==")
    for text, ok in checks:
        print(f"  {text}: {'yes' if ok else 'NO'}")
    ok = all(ok for _, ok in checks)
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
