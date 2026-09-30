#!/usr/bin/env python3
"""TASK-1185 host harness: does a frame that is no boiler evidence (an answer the
gateway made itself, or a line the /otgw_simulation.log replay injects) still count
as boiler evidence?

    python test/host/test_local_frame_consumers.py                       # working tree only
    python test/host/test_local_frame_consumers.py --old-rev de6b13ebe   # OLD vs FIX, with mutants

OLD is meant to be de6b13ebe, the tree before TASK-1185: the case classification
below (DEFECT and CONTROL) describes the whole task against it.

The frame chain is the one test_boiler_unsupported_origin.py compiles from the
real sources (bridgeFrameToParser(), the PIC task's enqueueOTFrame() call, the
replay's dispatchOTGWInputLine(), the frame queue, processOT(),
evaluateOTBusLiveness()); this harness reuses its slice lists and adds the SAT
gate: otRealBoilerSeenRecently() where the revision has it, isPICEnabled(),
isOTDirectEnabled(), otDirectBoilerPresent(), satBoilerHardwarePresent() and
satNotifyBoilerFrameSeen(). The driver is built once per board model (combo,
classic, OTGW32), because the SAT gate reads HAS_PIC and HAS_DIRECT_OT.

Each case dumps five consumer parts, A (acknowledged bitmaps), U (unsupported
bitmaps), L (OT log suffix), N (SAT boiler-detected flag) and H (SAT availability
gate), plus the thermostat bitmaps, the boiler liveness flag (boiler_connected)
and the queue trace. With --old-rev the run passes only when:
  1. FIX passes every case of every model, and each model's case list is exact.
  2. OLD fails every defect case, and its dump differs from FIX in exactly the
     parts that case's frames touch (DEFECT below) and nowhere else.
  3. OLD passes every control case, differing from FIX in exactly its listed
     parts (CONTROL below; mostly none).
  4. OLD and FIX see the same frames in the same order: the queue traces match
     once FIX's replay source tag (3) reads as OLD's PIC tag (0), and the other
     bitmaps and the boiler liveness flag are identical.
  5. Each mutant (one fix undone in the FIX slices, all three models built) fails
     exactly its cases.
A compile or slicing failure never counts as OLD reproducing the defect.
Exit code: 0 PASS, 1 FAIL, 2 harness error.
"""
import argparse
import re
import subprocess
import sys

from test_boiler_unsupported_origin import CORE, CORE_SLICES, OTD, OTD_SLICES, TYPES, cut
from test_ot_reserved_range import GEN, HERE, find_vcvars
from test_pic_banner_dispatch import read

FW_H = "src/OTGW-firmware/OTGW-firmware.h"
HW_H = "src/OTGW-firmware/Hardwaretypes.h"
SAT = "src/OTGW-firmware/SATcontrol.ino"

# (file, start anchor, end anchor or None[, optional]). An optional slice is left
# out when the revision does not have it (otRealBoilerSeenRecently() is new).
HW_SLICES = [(HW_H, r"^enum OTGWHardwareMode\b", None)]
OTD_EXTRA = [(OTD, r"^static bool\s+otBoilerCacheValid\[128\];", r"^static bool\s+otBoilerCacheValid\[128\];")]
GATE_SLICES = [
    (CORE, r"^bool otRealBoilerSeenRecently\(\)", None, True),
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
# Defect cases per model, against the pre-TASK-1185 tree, and the dump parts their
# frames change: A acked bitmaps, U unsupported bitmaps, L log suffix, N SAT edge
# flag, H SAT gate.
DEFECT = {
    "combo": {"A1": "A", "A2": "A", "A3": "ANH", "A4": "ANH", "A5": "A",
              "L1": "L", "L2": "L", "L3": "LNH", "N1": "LNH", "N2": "ANH", "H1": "ANH", "H2": "H",
              "R1": "UNH", "R2": "AUNH", "R3": "ANH", "R4": "ANH", "R5": "U", "R6": "UNH", "R7": "ANH"},
    "classic": {"R8": "ANH"},
    "otgw32": {},
}
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
PARTS = ["A", "U", "L", "N", "H", "other", "trace"]

# One fix undone at a time, in the FIX slices: (id, what, file, [(old, new)], cases it must fail).
MUTANTS = [
    ("MA", "acknowledged read counts non-evidence frames again", "core.inc",
     [("if (boilerEvidence && (boilerAckedRead[idx] & mask) == 0) {",
       "if ((boilerAckedRead[idx] & mask) == 0) {")], ["A1", "A3", "A5", "R3"]),
    ("MB", "acknowledged write counts non-evidence frames again", "core.inc",
     [("if (boilerEvidence && (boilerAckedWrite[idx] & mask) == 0) {",
       "if ((boilerAckedWrite[idx] & mask) == 0) {")], ["A2", "A4", "R4"]),
    ("ML", "the log gives a local type-7 the boiler suffix again", "core.inc",
     [("if (OTdata.bLocalAnswer) {", "if (false) {")], ["L1", "L2", "L3"]),
    ("MN", "a loopback or replayed B is a real boiler frame again", "core.inc",
     [("if (!localAnswer && !replayed) {", "{")], ["N1", "N2", "R6", "R7", "R8"]),
    ("MH", "the SAT gate reads the PIC signal in OT-Direct mode again", "gate.inc",
     [("if (!isOTDirectEnabled() && otRealBoilerSeenRecently()) return true;",
       "if (otRealBoilerSeenRecently()) return true;")], ["H2"]),
    ("MG", "the SAT gate reads bBoilerState, which a replayed B sets, again", "gate.inc",
     [("if (!isOTDirectEnabled() && otRealBoilerSeenRecently()) return true;",
       "if (!isOTDirectEnabled() && state.otBus.bBoilerState) return true;")], ["R7", "R8"]),
    ("MR", "the replay tags its lines as live PIC lines again", "core.inc",
     [("enqueueOTFrame(buf, len, false, OTFRAME_SRC_REPLAY);",
       "enqueueOTFrame(buf, len, false, OTFRAME_SRC_PIC);")], ["R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8"]),
    ("ME", "a replayed frame is boiler evidence for the bitmaps again", "core.inc",
     [("const bool boilerEvidence = !OTdata.bLocalAnswer && !OTdata.bReplayed;",
       "const bool boilerEvidence = !OTdata.bLocalAnswer;")], ["R1", "R2", "R3", "R4", "R5"]),
]


def export(rev, out_dir):
    out_dir.mkdir(parents=True, exist_ok=True)
    origin = f"git {rev}" if rev else "working tree"
    sources = {}
    for name, slices in OUTPUTS:
        parts = [f"// GENERATED by test_local_frame_consumers.py from the {origin}. Do not edit."]
        for entry in slices:
            path, start_rx, end_rx = entry[:3]
            optional = len(entry) > 3 and entry[3]
            if path not in sources:
                sources[path] = read(path, rev).splitlines()
            if optional and not any(re.search(start_rx, l) for l in sources[path]):
                parts.append(f"// ---- {path}: not in this revision ({start_rx}) ----")
                continue
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
        rx = re.compile(r"^(\S+) A=(\S+) U=(\S+) L=(\S+) N=(\S+) H=(\S+) other=(\S+) trace=(\S*)$")
        for line in dump_file.read_text(encoding="utf-8").splitlines():
            m = rx.match(line)
            if m:
                dump[m.group(1)] = dict(zip(PARTS, m.groups()[1:]))
    return r.returncode, cases, dump, r.stdout


def expected_cases(model):
    return sorted(list(DEFECT[model]) + list(CONTROL[model]))


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
        # The one allowed trace difference: OLD tags a replayed line as a PIC line
        # (source 0), FIX as OTFRAME_SRC_REPLAY (3). Same frames, same order.
        same_trace = o["trace"] == f["trace"].replace(":3", ":0")
        return "".join(p for p in "AULNH" if o[p] != f[p]), o["other"] == f["other"], same_trace

    def matches(model, case, want_old, parts):
        d = diff_parts(model, case)
        return old[model][1].get(case) == want_old and d is not None and d[0] == parts and d[1] and d[2], d

    bad_defect, bad_control = [], []
    for model, cs in DEFECT.items():
        for case, parts in cs.items():
            ok, d = matches(model, case, "FAIL", parts)
            if not ok:
                bad_defect.append(f"{model}:{case}({d[0] if d else '?'})")
    for model, cs in CONTROL.items():
        for case, parts in cs.items():
            ok, d = matches(model, case, "pass", parts)
            if not ok:
                bad_control.append(f"{model}:{case}({d[0] if d else '?'})")
    want_rc = {m: (1 if DEFECT[m] else 0) for m in MODELS}
    checks += [
        ("OLD exits 1 on the builds with defect cases and 0 on the others",
         all(old[m][0] == want_rc[m] for m in MODELS)),
        (f"defect cases fail on OLD and differ from FIX in exactly their parts, trace and other state identical"
         f" (not matching: {', '.join(bad_defect) or 'none'})", not bad_defect),
        (f"control cases pass on OLD, differing from FIX in exactly their listed parts, trace and other state"
         f" identical (not matching: {', '.join(bad_control) or 'none'})", not bad_control),
    ]

    for mid, what, name, reps, want_fail in MUTANTS:
        failed = []
        for model in MODELS:
            d = GEN / "local_frame_consumers" / f"{model}-{mid}"
            export(None, d)
            text = (d / name).read_text(encoding="utf-8")
            for a, b in reps:
                if text.count(a) != 1:
                    print(f"MUTANT {mid}: pattern must match exactly once, got {text.count(a)}: {a}")
                    return 2
                text = text.replace(a, b)
            (d / name).write_text(text, encoding="utf-8")
            _, cases, _, _ = build_and_run(f"mutant {mid} {model}", d, model)
            failed += [c for c, v in cases.items() if v == "FAIL"]
        failed = sorted(failed)
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
