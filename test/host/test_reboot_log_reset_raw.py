#!/usr/bin/env python3
"""TASK-1904 host harness: the reboot log names the raw reset code and a reset by the external 0x26 watchdog.

    python test/host/test_reboot_log_reset_raw.py                       # FIX and mutants
    python test/host/test_reboot_log_reset_raw.py --old-rev ff5eb599e   # plus OLD, before the fix

Two defects. (1) updateRebootLog() read the 0x26 status only when platformIsExternalReset() was true, and the
ESP32-S3 reset mapping (esp_system/port/soc/esp32s3/reset_reason.c) never returns ESP_RST_EXT, so a reset by the
external watchdog never reached the log. (2) "Other watchdog" (ESP_RST_WDT) covers six raw S3 codes; the log had
only the mapped number.

test_reboot_log_reset_raw.cpp is compiled from code extracted out of the revision under test:
updateRebootLog() and copyOlderRebootLogLines() (helperStuff.ino), whole. The hardware calls are stubs.

Runtime cases:
  E1  a reset by the external 0x26 watchdog (S3 reports power-on) is in the reboot log
  E2  the boot line carries the raw per-core code next to the mapped text
  E3  without the 0x26 flag there is no external-watchdog line
  E4  the longest boot line keeps the raw code and fits LOG_LINE_LENGTH
Static cases, on the source text:
  S1  setup() reads the 0x26 flag after the boot-time disarm and before updateRebootLog() and initWatchDog()
  S2  the read is one I2C transaction (no retry loop) on the already started Wire bus
  S3  /api/v2/device/info carries resetraw, resetrawlabel and extwdreset
  S4  every raw code of the pinned libs' soc/esp32s3 reset_reasons.h has a label; 0x01 says it is not distinguishable

Exit code: 0 PASS, 1 FAIL, 2 harness error.
"""
import argparse
import glob
import os
import re
import subprocess
import sys
from pathlib import Path

from test_ot_reserved_range import GEN, HERE, REPO, find_vcvars

FW = "src/OTGW-firmware/"
HELP, INO, CORE, REST = FW + "helperStuff.ino", FW + "OTGW-firmware.ino", FW + "OTGW-Core.ino", FW + "restAPI.ino"
PLAT = "src/libraries/Platform/src/platform_esp32.h"
RUNTIME = ["E1", "E2", "E3", "E4"]
STATIC = ["S1", "S2", "S3", "S4"]
OLD_MUST_FAIL = ["E1", "E2", "E4", "S1", "S2", "S3", "S4"]

MUTANTS = [
    ("MC1", "the log gate goes back to platformIsExternalReset()", HELP,
     [("  if (g_extWdResetAtBoot) {\n", "  if (platformIsExternalReset()) {\n")], ["E1"]),
    ("MC2", "the boot line drops the raw code", HELP,
     [("reboot cause: %s (%x) [rst %s] %s\\r\\n\"), myTime.year()", "reboot cause: %s (%x) %s\\r\\n\"), myTime.year()"),
      ("CSTR(text), errorCode, lastResetRaw, log_line_excpt);\n  } else {", "CSTR(text), errorCode, log_line_excpt);\n  } else {")],
     ["E2", "E4"]),   # E4 checks the raw code too
    ("MC3", "the 0x26 flag is read after the reboot log", INO,
     [("  readExtWatchdogResetFlag();   // TASK-1904: before the first feed clears it, and before the reboot log\n", ""),
      ("  updateRebootLog(lastReset);\n", "  updateRebootLog(lastReset);\n  readExtWatchdogResetFlag();\n")],
     ["S1"]),
    ("MC5", "LOG_LINE_LENGTH back to 140, CRLF guard kept (the cut line still ends on its own)", HELP,
     [("  #define LOG_LINE_LENGTH 160", "  #define LOG_LINE_LENGTH 140")], []),
    ("MC6", "LOG_LINE_LENGTH back to 140 without the CRLF guard (the boot line runs into the next)", HELP,
     [("  #define LOG_LINE_LENGTH 160", "  #define LOG_LINE_LENGTH 140"),
      ("  if (lineLen > 0 && log_line[lineLen - 1] != '\\n') {", "  if (false) {")], ["E4"]),
    ("MC4", "the 0x01 label guesses one cause", PLAT,
     [('return "power-on, brown-out or super watchdog (not distinguishable)";', 'return "power-on";')], ["S4"]),
]


def read(path, rev):
    if rev is None:
        text = (REPO / path).read_text(encoding="utf-8", errors="replace")
    else:
        r = subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{path}"], capture_output=True, text=True,
                           encoding="utf-8", errors="replace")
        text = r.stdout if r.returncode == 0 else ""
    return text.replace("\r\n", "\n")


def sources(rev):
    return {p: read(p, rev) for p in (HELP, INO, CORE, REST, PLAT)}


def function_body(text, rx):
    lines = text.split("\n")
    hits = [i for i, l in enumerate(lines) if re.search(rx, l)]
    if len(hits) != 1:
        raise SystemExit(f"anchor must match exactly once, got {len(hits)}: {rx}")
    end = next((i for i in range(hits[0] + 1, len(lines)) if re.match(r"^\}", lines[i])), None)
    if end is None:
        raise SystemExit(f"no closing brace at column 0 after: {rx}")
    return lines[hits[0]:end + 1]


def code(text):
    return re.sub(r"//[^\n]*", "", text)


def reset_reason_codes():
    """{name: value} of the S3 soc_reset_reason_t in the installed pinned libs, or None."""
    roots = [os.environ.get("PLATFORMIO_CORE_DIR", ""), r"D:\DevData\platformio", str(Path.home() / ".platformio")]
    for root in filter(None, roots):
        for h in glob.glob(os.path.join(root, "packages", "framework-arduinoespressif32-libs", "esp32s3", "include",
                                        "soc", "esp32s3", "include", "soc", "reset_reasons.h")):
            text = Path(h).read_text(encoding="utf-8", errors="replace")
            return {n: int(v, 16) for n, v in re.findall(r"(RESET_REASON_\w+)\s*=\s*(0x[0-9A-Fa-f]+)", text)}, h
    return None, None


def static_cases(src):
    setup = "\n".join(function_body(code(src[INO]), r"^void setup\(\)"))
    i_dis = setup.find("WatchDogEnabled(0);")
    i_read = setup.find("readExtWatchdogResetFlag();")
    i_log = setup.find("updateRebootLog(")
    i_init = setup.find("initWatchDog(")
    i_wire = setup.rfind("Wire.begin(activeI2cSda(), activeI2cScl());", 0, i_read if i_read >= 0 else 0)
    s1 = 0 <= i_dis < i_read < i_log < i_init
    core = code(src[CORE])
    m = re.search(r"void readExtWatchdogResetFlag\(\) \{(.*?)\n\}", core, re.S)
    body = m.group(1) if m else ""
    s2 = bool(m) and body.count("endTransmission(") == 1 and "for (" not in body and "while" not in body \
        and not re.search(r"Wire\.begin\(", body) and 0 <= i_wire < i_read
    rest = code(src[REST])
    s3 = all(f'F("{k}")' in rest for k in ("resetraw", "resetrawlabel", "extwdreset"))
    codes, header = reset_reason_codes()
    plat = src[PLAT]
    lm = re.search(r"platformResetRawLabel\(unsigned code\) \{(.*?)\n\}", plat, re.S)
    labelled = {int(v, 16): t for v, t in re.findall(r"case (0x[0-9A-Fa-f]+): return \"([^\"]*)\"", lm.group(1))} if lm else {}
    if codes is None:
        s4, got4 = False, "reset_reasons.h of the pinned libs not found (harness error)"
    else:
        missing = sorted({v for v in codes.values()} - set(labelled))
        honest = "not distinguishable" in labelled.get(0x01, "")
        s4 = not missing and honest
        got4 = f"{len(set(codes.values()))} codes in {Path(header).name}, missing {['0x%02x' % v for v in missing]}, " \
               f"0x01 label {labelled.get(0x01, 'none')!r}"
    return {
        "S1": ("pass" if s1 else "FAIL", "setup() reads the 0x26 flag after the disarm, before the reboot log and initWatchDog()",
               f"disarm {i_dis}, read {i_read}, reboot log {i_log}, initWatchDog {i_init}"),
        "S2": ("pass" if s2 else "FAIL", "the read is one I2C transaction on the already started Wire bus",
               f"function {'found' if m else 'missing'}, Wire.begin before it at {i_wire}"),
        "S3": ("pass" if s3 else "FAIL", "device/info carries resetraw, resetrawlabel and extwdreset",
               "all three" if s3 else "missing"),
        "S4": ("pass" if s4 else "FAIL", "every raw S3 reset code has a label; 0x01 is marked not distinguishable", got4),
    }


def export(src, out_dir):
    out_dir.mkdir(parents=True, exist_ok=True)
    head = "// GENERATED by test_reboot_log_reset_raw.py. Do not edit.\n"
    body = function_body(src[HELP], r"^static void copyOlderRebootLogLines\(") + \
        function_body(src[HELP], r"^bool updateRebootLog\(String text\)")
    (out_dir / "rebootlog.inc").write_text(head + "\n".join(body) + "\n", encoding="utf-8")


def build(label, out_dir):
    exe = out_dir / "test_reboot_log_reset_raw.exe"
    if exe.exists():
        exe.unlink()
    bat = out_dir / "_compile.bat"
    fwd = str(out_dir).replace("\\", "/")
    bat.write_text("@echo off\r\n"
                   f"call \"{find_vcvars()}\" >nul 2>nul\r\n"
                   f"cl /nologo /EHsc /W3 /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS "
                   f"/Fo:\"{fwd}/\" /Fe:\"{exe}\" \"{HERE / 'test_reboot_log_reset_raw.cpp'}\" /I\"{out_dir}\"\r\n",
                   encoding="ascii")
    c = subprocess.run(["cmd.exe", "/c", str(bat)], capture_output=True, text=True, errors="replace")
    if c.returncode != 0 or not exe.exists():
        print(c.stdout[-4000:], c.stderr[-2000:])
        print(f"COMPILATION FAILED ({label})")
        sys.exit(2)
    return exe


def run_variant(label, out_dir, src, verbose):
    export(src, out_dir)
    exe = build(label, out_dir)
    results, text = {}, []
    for case in RUNTIME:
        r = subprocess.run([str(exe), case], capture_output=True, text=True)
        m = re.search(rf"^CASE {case} (pass|FAIL)\b", r.stdout, re.M)
        if not m or r.returncode not in (0, 1):
            print(r.stdout, r.stderr)
            print(f"HARNESS ERROR: case {case} of {label} did not report (exit code {r.returncode})")
            sys.exit(2)
        results[case] = m.group(1)
        text.append(r.stdout.rstrip("\n"))
    for case, (verdict, title, got) in static_cases(src).items():
        results[case] = verdict
        text.append(f"CASE {case} {verdict}\n     {title}\n     got : {got}")
    if verbose:
        print(f"== {label} ==")
        print("\n".join(text))
    return sorted(k for k, v in results.items() if v == "FAIL")


def mutate(src, path, subs, mid):
    out = dict(src)
    text = out[path]
    for old, new in subs:
        if text.count(old) != 1 or old == new:
            raise SystemExit(f"{mid}: substitution must match exactly once: {old!r}")
        text = text.replace(old, new)
    out[path] = text
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--old-rev")
    ap.add_argument("-q", "--quiet", action="store_true")
    a = ap.parse_args()
    ok = True
    fix = sources(None)
    failed = run_variant("FIX", GEN / "rstraw_fix", fix, not a.quiet)
    print(f"FIX: {'PASS' if not failed else 'FAIL ' + ','.join(failed)}")
    ok &= not failed
    for mid, what, path, subs, expect in MUTANTS:
        got = run_variant(mid, GEN / f"rstraw_{mid.lower()}", mutate(fix, path, subs, mid), False)
        good = got == sorted(expect)
        print(f"{mid} ({what}): fails {','.join(got) or 'nothing'}, expected {','.join(sorted(expect))}: "
              f"{'ok' if good else 'WRONG'}")
        ok &= good
    if a.old_rev:
        got = run_variant(f"OLD {a.old_rev}", GEN / "rstraw_old", sources(a.old_rev), not a.quiet)
        missing = [c for c in OLD_MUST_FAIL if c not in got]
        print(f"OLD {a.old_rev}: fails {','.join(got) or 'nothing'}"
              f"{'' if not missing else ' -- expected also ' + ','.join(missing)}")
        ok &= not missing
    print("RESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
