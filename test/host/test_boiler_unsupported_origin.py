#!/usr/bin/env python3
"""TASK-1086 host harness: does an answer the gateway made itself count as
boiler evidence in the boiler-unsupported bitmaps?

    python test/host/test_boiler_unsupported_origin.py                  # working tree only
    python test/host/test_boiler_unsupported_origin.py --old-rev HEAD   # OLD vs FIX

A frame reaches the bitmaps through bridgeFrameToParser() (OTDirect.ino) or
dispatchOTGWInputLine() (the PIC path), enqueueOTFrame() and its OTFrameMsg
queue item, drainOTFrameQueue(), and processOT(), which parses the frame,
holds it back one frame to pair (T,R) and (B,A), and then updates the six
per-msgid bitmaps. This harness compiles that whole chain from the REAL
sources, sliced by anchor (TYPES, CORE_SLICES and OTD_SLICES below), and
replays the frame sequences the OTDirect producers emit and the PIC sends.
Each case names the producer and the test_override_reply.cpp case that
already proves the T/R/B/A log that producer emits. Doubles stand in for the
FreeRTOS queue and mutex, the PIC hardware, all output (MQTT, WebSocket,
telnet, OT log, port 25238, LED) and the value decoding and publishing that
processOT() runs after the bitmap block.

With --old-rev, the same slices of that revision are compiled against the same
harness. The run passes only when:
  1. FIX passes every case, and the case list is exactly DEFECT + CONTROL.
  2. OLD fails every defect case; its unsupported verdict (the unsupported
     bitmaps and their dirty flags) differs from FIX while every other bitmap
     is byte-identical: the fix changes the verdicts and nothing else.
  3. OLD passes every control case with a dump byte-identical to FIX.
  4. FIX's queue trace (prefix and source byte of each frame) is the expected
     one, and OLD's trace is the same frames with no frame tagged local, so
     both sides were fed the same sequence.
A compile or slicing failure never counts as OLD reproducing the defect.
Exit code: 0 PASS, 1 FAIL, 2 harness error.

The old-vs-fix comparison proves the TASK-1086 change on its own: run it on a
checkout of 331550de3 with --old-rev 331550de3^. On later trees TASK-1185 also
keeps gateway-made frames out of the acknowledged bitmaps, so D6, D7 and S1
differ there as well and rules 2 and 3 no longer hold against that OLD;
test_local_frame_consumers.py carries that proof. The working-tree-only run
stays a regression check.
"""
import argparse
import re
import subprocess
import sys

from test_ot_reserved_range import ANCHORS as RESERVED_ID_ANCHORS
from test_ot_reserved_range import GEN, HERE, find_vcvars, slice_by_anchor
from test_pic_banner_dispatch import read, slice_range

CORE_H = "src/OTGW-firmware/OTGW-Core.h"
CORE = "src/OTGW-firmware/OTGW-Core.ino"
OTD_TYPES = "src/OTGW-firmware/OTDirecttypes.h"
OTD = "src/OTGW-firmware/OTDirect.ino"

# (file, start anchor, end anchor or None), in source order per output file.
# Without an end anchor the slice is the construct that starts on the anchor
# line (slice_by_anchor). With one it runs through the construct that begins on
# the end line (slice_range); start == end takes that one line, for a
# declaration that carries a trailing comment.
TYPES = [
    (CORE_H, r"^#define MAX_BUFFER_READ\b", r"^#define MAX_BUFFER_READ\b"),
    (CORE_H, r"^enum OTLibMessageType\b", None),
    (CORE_H, r"^enum OTLibMessageID\b", None),
    (CORE_H, r"^\s*enum OTtype_t\b", r"^\s*const OTlookup_t OTmap\[\] PROGMEM"),
    (CORE_H, r"^#define OT_MSGID_MAX\b", r"^#define OT_MSGID_MAX\b"),
    (CORE_H, r"^void processOT\(const char \*buf, int len\b", None),
    (CORE_H, r"^enum OTFrameSource\b", r"^void drainOTFrameQueue\(\);"),
    (CORE_H, r"^struct OTStateLock \{", None),
    (CORE_H, r"^enum OTGW_response_type\b", None),
    (CORE_H, r"^struct OpenthermData_t \{", None),
]
CORE_SLICES = [
    (CORE, r"^OpenthermData_t OTdata, delayedOTdata, tmpOTdata;", None),
    (CORE, r"^PlatformQueue otFrameQueue = nullptr;", r"^static uint32_t otFrameQueueDrops\b"),
    (CORE, r"^bool enqueueOTFrame\(const char \*buf, size_t len, bool suppressOutput, uint8_t source\)", None),
    (CORE, r"^void drainOTFrameQueue\(\)", None),
] + [(CORE, a, None) for a in RESERVED_ID_ANCHORS] + [
    (CORE, r"^bool is_value_valid\(OpenthermData_t OT, OTlookup_t OTlookup\)", None),
    (CORE, r"^static void dispatchOTGWInputLine\(const char\* buf, size_t len\)", None),
    (CORE, r"^bool isvalidotmsg\(const char \*buf, int len\)", None),
    (CORE, r"^static uint8_t boilerLastMasterWasWrite\[32\]", r"^void clearBoilerUnsupportedDirty\(\)"),
    (CORE, r"^void evaluateOTBusLiveness\(OTBusLivenessTrigger trigger\)", None),
    (CORE, r"^void processOT\(const char \*buf, int len, bool suppressOutput", None),
]
OTD_SLICES = [
    (OTD_TYPES, r"^enum OTDirectMode\b", None),
    (OTD, r"^static OTDirectMode otCurrentMode\b", r"^#define IS_LOOPBACK_MODE\(\)"),
    (OTD, r"^static bool otHideReports\b", r"^static bool otHideReports\b"),
    (OTD, r"^static void bridgeFrameToParser\(char prefix, unsigned long frame\)", None),
]
OUTPUTS = [("types.inc", TYPES), ("core.inc", CORE_SLICES), ("otd.inc", OTD_SLICES)]

DEFECT = ["D1", "D2", "D3", "D4", "D5", "D6", "D7"]
CONTROL = ["G1", "G2", "G3", "G4", "G5", "G6", "G7", "S1", "P1", "P2", "P3", "P4", "P5", "P6", "P7"]

# The queue trace FIX must produce: the prefix and OTFrameSource byte of every
# frame a case emits (0 PIC, 1 OTDirect, 2 OTDirect answer the gateway made).
FIX_TRACE = {
    "D1": "T:1,A:2", "D2": "T:1,A:2", "D3": "T:1,A:2", "D4": "T:1,B:2",
    "D5": "R:1,B:2", "D6": "T:1,B:2", "D7": "R:1,B:2",
    "G1": "T:1,B:1", "G2": "T:1,B:1", "G3": "T:1,B:1", "G4": "T:1,B:1",
    "G5": "R:1,B:1", "G6": "T:1,R:1,B:1,A:2", "G7": "T:1,B:1,A:2", "S1": "T:1,A:2",
    "P1": "T:0,A:0", "P2": "T:0,B:0", "P3": "T:0,B:0", "P4": "T:0,A:0",
    "P5": "T:0,B:0,A:0", "P6": "T:0,B:0", "P7": "T:0,A:0",
}


def cut(lines, start_rx, end_rx):
    """The slice text plus its 1-based source line range, for the log."""
    text = slice_range(lines, start_rx, end_rx) if end_rx else slice_by_anchor(lines, start_rx)
    first = next(i for i, l in enumerate(lines) if re.search(start_rx, l)) + 1
    return text, first, first + text.count("\n")


def export(rev, out_dir):
    out_dir.mkdir(parents=True, exist_ok=True)
    origin = f"git {rev}" if rev else "working tree"
    sources = {}
    for name, slices in OUTPUTS:
        parts = [f"// GENERATED by test_boiler_unsupported_origin.py from the {origin}. Do not edit."]
        for path, start_rx, end_rx in slices:
            if path not in sources:
                sources[path] = read(path, rev).splitlines()
            text, first, last = cut(sources[path], start_rx, end_rx)
            print(f"  {name:10} {path.split('/')[-1]:16} lines {first:5}-{last:<5} {start_rx}")
            parts.append(f"// ---- {path}:{first}-{last} ----\n{text}")
        (out_dir / name).write_text("\n".join(parts) + "\n", encoding="utf-8")


def build_and_run(label, out_dir):
    exe = out_dir / "test_boiler_unsupported_origin.exe"
    if exe.exists():
        exe.unlink()
    bat = out_dir / "_compile.bat"
    fwd = str(out_dir).replace("\\", "/")
    bat.write_text("@echo off\r\n"
                   f"call \"{find_vcvars()}\" >nul 2>nul\r\n"
                   f"cl /nologo /EHsc /W3 /std:c++17 /Od /utf-8 /D_CRT_SECURE_NO_WARNINGS "
                   f"/Fo:\"{fwd}/\" /Fe:\"{exe}\" \"{HERE / 'test_boiler_unsupported_origin.cpp'}\" "
                   f"/I\"{out_dir}\"\r\n", encoding="ascii")
    c = subprocess.run(["cmd.exe", "/c", str(bat)], capture_output=True, text=True, errors="replace")
    if c.returncode != 0 or not exe.exists():
        print(c.stdout[-4000:], c.stderr[-2000:])
        print(f"COMPILATION FAILED ({label})")
        sys.exit(2)
    r = subprocess.run([str(exe), str(out_dir)], capture_output=True, text=True)
    print(f"== {label} ==")
    print(r.stdout, end="")
    cases = dict(re.findall(r"^CASE (\S+) (pass|FAIL)\b", r.stdout, re.M))
    dump = {}
    dump_file = out_dir / "cases.txt"
    if dump_file.exists():
        for line in dump_file.read_text(encoding="utf-8").splitlines():
            m = re.match(r"^(\S+) verdict=(\S+) other=(\S+) trace=(\S+)$", line)
            if m:
                dump[m.group(1)] = {"verdict": m.group(2), "other": m.group(3), "trace": m.group(4)}
    return r.returncode, cases, dump


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--old-rev", help="also build OLD from this git revision and compare")
    args = ap.parse_args()

    fix_dir = GEN / "boiler_unsupported_origin" / "fix"
    print("== slicing FIX (working tree) ==")
    export(None, fix_dir)
    fix_rc, fix_cases, fix_dump = build_and_run("FIX (working tree)", fix_dir)
    trace_ok = all(fix_dump.get(c, {}).get("trace") == t for c, t in FIX_TRACE.items())
    fix_ok = (fix_rc == 0 and sorted(fix_cases) == sorted(DEFECT + CONTROL)
              and all(v == "pass" for v in fix_cases.values()))
    if not args.old_rev:
        print(f"  FIX queue trace as expected: {'yes' if trace_ok else 'NO'}")
        ok = fix_ok and trace_ok
        print("RESULT:", "PASS" if ok else "FAIL")
        return 0 if ok else 1

    old_dir = GEN / "boiler_unsupported_origin" / "old"
    print(f"== slicing OLD (git {args.old_rev}) ==")
    export(args.old_rev, old_dir)
    old_rc, old_cases, old_dump = build_and_run(f"OLD (git {args.old_rev})", old_dir)

    def same(c, part):
        return c in old_dump and c in fix_dump and old_dump[c][part] == fix_dump[c][part]

    bad_defect = [c for c in DEFECT if not (old_cases.get(c) == "FAIL" and fix_cases.get(c) == "pass"
                                            and not same(c, "verdict") and same(c, "other"))]
    bad_control = [c for c in CONTROL if not (old_cases.get(c) == "pass" and same(c, "verdict")
                                              and same(c, "other"))]
    bad_trace = [c for c, t in FIX_TRACE.items() if old_dump.get(c, {}).get("trace") != t.replace(":2", ":1")]
    checks = [
        ("FIX passes every case, case list is DEFECT + CONTROL", fix_ok),
        ("FIX queue trace as expected (answers the gateway made carry source 2)", trace_ok),
        ("OLD exits 1", old_rc == 1),
        ("same case list OLD/FIX", sorted(old_cases) == sorted(fix_cases)),
        (f"defect cases {','.join(DEFECT)}: OLD fails, FIX passes, only the unsupported verdict differs"
         f" (not matching: {','.join(bad_defect) or 'none'})", not bad_defect),
        (f"control cases {','.join(CONTROL)}: OLD passes, dump byte-identical to FIX"
         f" (not matching: {','.join(bad_control) or 'none'})", not bad_control),
        (f"OLD fed the same frames, none tagged local (not matching: {','.join(bad_trace) or 'none'})",
         not bad_trace),
    ]
    print("== old-vs-fix verdict ==")
    for text, ok in checks:
        print(f"  {text}: {'yes' if ok else 'NO'}")
    ok = all(ok for _, ok in checks)
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
