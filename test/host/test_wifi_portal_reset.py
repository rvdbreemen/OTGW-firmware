#!/usr/bin/env python3
"""TASK-1930 / ADR-238 host harness: the triple-reset WiFi config portal trigger on the ESP32-S3.

    python test/host/test_wifi_portal_reset.py                     # the fix (working tree)
    python test/host/test_wifi_portal_reset.py --old-rev <sha>     # plus the code before the fix, which must be red

Before TASK-1930, shouldForceWifiConfigPortal() counted a boot only when esp_reset_reason() == ESP_RST_EXT. The S3
gives the reset button (EN pin) the chip power-on code 0x01 (ESP_RST_POWERON) and never reports ESP_RST_EXT, so the
gesture was dead. ADR-238 (Robert, option A) counts reset-button presses and power-ons alike.

The .cpp is compiled from the real code of the revision under test: the WIFI_PORTAL_RESET block of OTGW-firmware.ino
(from the #defines to wifiPortalResetWindowExpired()) and every platformIs...Reset/PowerOn() shim of
platform_esp32.h. Only the hardware calls are stubs: esp_reset_reason() per modelled boot, NVS that survives every
boot, millis() and the RAM globals that restart at 0.

Cases:
  P1  three reset-button presses within the 10 s window force the portal
  P2  three software resets never count
  P3  presses each after the window has expired never force the portal
  P4  the counter fed ESP_RST_EXT (ESP8266 semantics) forces the portal on the third
  P5  three power-ons (pull the plug three times) within the window force the portal (ADR-238)
  P6  USB resets and system brown-outs (0x0F) never force the portal

Exit code: 0 PASS (fix all green; with --old-rev also the old code red on P1 and P5), 1 FAIL, 2 harness error.
"""
import argparse
import re
import subprocess
import sys

from test_ot_reserved_range import GEN, HERE, REPO, find_vcvars

INO = "src/OTGW-firmware/OTGW-firmware.ino"
PLAT = "src/libraries/Platform/src/platform_esp32.h"
CASES = ["P1", "P2", "P3", "P4", "P5", "P6"]
EXPECT_OLD = {"P1": "FAIL", "P2": "pass", "P3": "pass", "P4": "pass", "P5": "FAIL", "P6": "pass"}   # the dead path
EXPECT_FIX = {c: "pass" for c in CASES}                                                             # ADR-238


def read(path, rev):
    if rev is None:
        return (REPO / path).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")
    r = subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{path}"], capture_output=True, text=True,
                       encoding="utf-8", errors="replace")
    if r.returncode != 0:
        raise SystemExit(f"cannot read {path} at {rev}")
    return r.stdout.replace("\r\n", "\n")


def export(rev, out):
    out.mkdir(parents=True, exist_ok=True)
    ino = read(INO, rev)
    a = ino.find("#define WIFI_PORTAL_RESET_MAGIC")
    m = re.search(r"^bool wifiPortalResetWindowExpired\(\) \{.*?^\}", ino[a:], re.S | re.M)
    if a < 0 or not m:
        raise SystemExit("WIFI_PORTAL_RESET block not found")
    (out / "portal.inc").write_text("// GENERATED\n" + ino[a:a + m.end()] + "\n", encoding="utf-8")
    plat = read(PLAT, rev)
    shims = re.findall(r"inline bool platformIs\w*(?:Reset|PowerOn)\w*\(\) \{.*?\n\}", plat, re.S)
    if not shims:
        raise SystemExit("no platformIs...Reset() shim found")
    (out / "extreset.inc").write_text(
        "// GENERATED\n" + "\n".join(x.replace("inline ", "static ") for x in shims) + "\n", encoding="utf-8")


def build(out):
    exe = out / "test_wifi_portal_reset.exe"
    if exe.exists():
        exe.unlink()
    bat = out / "_compile.bat"
    fwd = str(out).replace("\\", "/")
    bat.write_text("@echo off\r\n"
                   f"call \"{find_vcvars()}\" >nul 2>nul\r\n"
                   f"cl /nologo /EHsc /W3 /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS /Fo:\"{fwd}/\" /Fe:\"{exe}\" "
                   f"\"{HERE / 'test_wifi_portal_reset.cpp'}\" /I\"{out}\"\r\n", encoding="ascii")
    c = subprocess.run(["cmd.exe", "/c", str(bat)], capture_output=True, text=True, errors="replace")
    if c.returncode != 0 or not exe.exists():
        print(c.stdout[-4000:], c.stderr[-2000:])
        print("COMPILATION FAILED")
        sys.exit(2)
    return exe


def run_rev(rev, label, out, quiet):
    export(rev, out)
    exe = build(out)
    got = {}
    for case in CASES:
        r = subprocess.run([str(exe), case], capture_output=True, text=True)
        m = re.search(rf"^CASE {case} (pass|FAIL)\b", r.stdout, re.M)
        if not m:
            print(r.stdout, r.stderr)
            print(f"HARNESS ERROR: case {case} of {label} did not report")
            sys.exit(2)
        got[case] = m.group(1)
        if not quiet:
            print(r.stdout.rstrip("\n"))
    print(f"{label}: " + ", ".join(f"{k} {v}" for k, v in got.items()))
    return got


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--old-rev", help="revision before the fix; must show the dead path (P1 and P5 red)")
    ap.add_argument("-q", "--quiet", action="store_true")
    a = ap.parse_args()
    ok = run_rev(None, "FIX", GEN / "wifiportal_fix", a.quiet) == EXPECT_FIX
    if a.old_rev:
        old = run_rev(a.old_rev, f"OLD {a.old_rev}", GEN / "wifiportal_old", a.quiet)
        met = old == EXPECT_OLD
        print(f"OLD expectation (P1 and P5 red, the rest green): {'met' if met else 'NOT met'}")
        ok &= met
    print("RESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
