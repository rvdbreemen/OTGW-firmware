#!/usr/bin/env python3
"""TASK-1666 host harness: the diagnostic recording download and status routes (ADR-213 decision 5).

    python test/host/test_diag_recording_routes.py              # the real code as it is in the working tree
    python test/host/test_diag_recording_routes.py --mutant     # also build two broken copies of the header

The C++ part (test_diag_recording_routes.cpp) includes the real src/OTGW-firmware/diagRecorderStuff.h and calls
the code the REST glue uses: diagDownloadRange(), the file header writer, diagDownloadFill() (the filler body)
and diagStatusSnapshot(). The Python part adds text checks (W1-W4) that the route table and the glue in
restAPI.ino call those functions and keep the ring out of the REST file.

Mutants (applied to a copy of the header in the generated include directory only):
  DIAG-RANGE-END  the filler ends at the live newest block instead of the block count fixed at request start
                  -> must fail R1 (appended blocks sent) and R6 (overwritten block) only
  DIAG-SINCE      the since= clamp is removed -> must fail R2 only
Compiler: MSVC through vswhere. Exit: 0 PASS, 1 FAIL, 2 error.
"""
import argparse
import re
import subprocess
import sys

from test_ot_reserved_range import GEN, HERE, REPO, find_vcvars

SRC = REPO / "src" / "OTGW-firmware"
HEADER = SRC / "diagRecorderStuff.h"
REST = SRC / "restAPI.ino"
RANGE_END_MUTANT = ("  if (maxLen == 0 || (index >= d.hdrLen && (!r.hasData() || "
                    "d.first + (uint32_t)((index - d.hdrLen) / DIAG_REC_BLOCK_SIZE) > r.newestSeq()))) return 0;\r\n")
MUTANTS = [("DIAG-RANGE-END", ["R1", "R6"], "range follows the live ring", RANGE_END_MUTANT),
           ("DIAG-SINCE", ["R2"], "since= ignored", "")]


def build_and_run(label, inc_dir, out_dir):
    out_dir.mkdir(parents=True, exist_ok=True)
    exe = out_dir / "test_diag_recording_routes.exe"
    if exe.exists():
        exe.unlink()
    bat = out_dir / "_compile.bat"
    fwd = str(out_dir).replace("\\", "/")
    bat.write_text("@echo off\r\n"
                   f"call \"{find_vcvars()}\" >nul 2>nul\r\n"
                   f"cl /nologo /EHsc /W3 /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS "
                   f"/Fo:\"{fwd}/\" /Fe:\"{exe}\" \"{HERE / 'test_diag_recording_routes.cpp'}\" /I\"{inc_dir}\"\r\n",
                   encoding="ascii")
    c = subprocess.run(["cmd.exe", "/c", str(bat)], capture_output=True, text=True, errors="replace")
    if c.returncode != 0 or not exe.exists():
        print(c.stdout[-4000:], c.stderr[-2000:])
        print(f"COMPILATION FAILED ({label})")
        sys.exit(2)
    r = subprocess.run([str(exe)], capture_output=True, text=True, errors="replace", timeout=300)
    print(f"== {label} ==")
    print(r.stdout, end="")
    cases = dict(re.findall(r"^CASE (\S+) (pass|FAIL)\b", r.stdout, re.M))
    return r.returncode, cases


def wiring_cases():
    """W1-W4: text checks on restAPI.ino (the glue cannot be compiled on the host: it needs the web server)."""
    rest = REST.read_text(encoding="utf-8")
    res = {}
    table = re.search(r"kV2Routes\[\] PROGMEM = \{(.*?)\};", rest, re.S)
    rows = re.findall(r"\{\s*(\w+),\s*(\w+)\s*\}", table.group(1)) if table else []
    res["W1"] = (bool(rows) and ("kRouteDiag", "handleDiag") in rows and rows[-1] == ("nullptr", "nullptr")
                 and rows[-2] == ("kRouteDiag", "handleDiag"))                 # appended last: route ids of old recordings stay valid
    h = re.search(r"^static void handleDiag\([^;]*?\)\n\{.*?\n\}\n", rest, re.S | re.M)
    body = h.group(0) if h else ""
    res["W2"] = all(s in body for s in ('PSTR("recording")', 'PSTR("status")', 'PSTR("start")', 'PSTR("stop")',
                                         "sendDiagRecordingDownload()", "sendDiagRecordingStatus()", "sendDiagRecordingControl("))
    res["W2"] = res["W2"] and "HTTP_POST" in body and body.count("HTTP_GET") >= 2
    d = re.search(r"void sendDiagRecordingDownload\(\)\n\{.*?\n\}\n", rest, re.S)
    dl = d.group(0) if d else ""
    res["W3"] = ("diagDownloadRange(diagRec" in dl and "diagDownloadFill(diagRec" in dl and "make_shared<DiagDownload>" in dl
                 and dl.index("diagDownloadRange(") < dl.index("beginResponse(")        # range fixed before the response starts
                 and "diagRec.ring" not in rest and not re.search(r"diagRec\.mem", dl) and "memcpy(" not in dl.replace("memcpy_P(", ""))
    c = re.search(r"void sendDiagRecordingControl\(bool start\)\n\{.*?\n\}\n", rest, re.S)
    cb = c.group(0) if c else ""
    res["W4"] = "diagRecorderStart()" in cb and "diagRecorderStop()" in cb and "diagStartReasonText(" in cb
    print("== wiring (text checks on restAPI.ino) ==")
    for k in sorted(res):
        print(f"CASE {k} {'pass' if res[k] else 'FAIL'}")
    return res


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--mutant", action="store_true", help="also build the header with the range / since lines broken")
    args = ap.parse_args()
    base = GEN / "diag_recording_routes"
    wiring = wiring_cases()
    rc, cases = build_and_run("REAL header (working tree)", HEADER.parent, base / "real")
    ok_real = rc == 0 and len(cases) >= 8 and all(v == "pass" for v in cases.values()) and all(wiring.values())
    if not args.mutant:
        print("RESULT:", "PASS" if ok_real else "FAIL")
        return 0 if ok_real else 1
    raw = HEADER.read_bytes().decode("utf-8")
    lines = raw.splitlines(keepends=True)
    checks = [("REAL passes every case", ok_real)]
    for mark, red, what, repl in MUTANTS:
        hit = [i for i, l in enumerate(lines) if mark in l]
        if len(hit) != 1:
            print(f"expected exactly one {mark} line in the header, found {len(hit)}")
            return 2
        mut = list(lines)
        mut[hit[0]] = repl
        mut_inc = base / ("mutant_inc_" + mark)
        mut_inc.mkdir(parents=True, exist_ok=True)
        (mut_inc / HEADER.name).write_bytes("".join(mut).encode("utf-8"))
        mrc, mcases = build_and_run(f"MUTANT header ({what})", mut_inc, base / ("mutant_" + mark))
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
