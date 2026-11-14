#!/usr/bin/env python3
"""TASK-1930 host harness: the reset-button WiFi config portal trigger (ADR-043) on the ESP32-S3.

    python test/host/test_wifi_portal_reset.py              # the working tree
    python test/host/test_wifi_portal_reset.py --rev <sha>  # another revision

shouldForceWifiConfigPortal() counts a boot only when platformIsExternalReset() (esp_reset_reason() == ESP_RST_EXT)
is true. The S3 gives the reset button (EN pin) the chip power-on code 0x01 and never reports ESP_RST_EXT, so the
gesture is dead. The .cpp is compiled from the real code of the revision under test (the WIFI_PORTAL_RESET block of
OTGW-firmware.ino and platformIsExternalReset() of platform_esp32.h); only the hardware calls are stubs.

Cases: P1 three reset-button presses force the portal (options A/B of the open decision; RED on the current code),
P2 software resets never count, P3 presses outside the 10 s window never count, P4 the counter itself works when fed
ESP_RST_EXT. Which of P1's expectation stands depends on Robert's decision (TASK-1930); under option C (long press)
P1 is replaced, not made green.

Exit code: 0 when the run matches EXPECT, 1 otherwise, 2 harness error.
"""
import argparse
import re
import subprocess
import sys

from test_ot_reserved_range import GEN, HERE, REPO, find_vcvars

INO = "src/OTGW-firmware/OTGW-firmware.ino"
PLAT = "src/libraries/Platform/src/platform_esp32.h"
CASES = ["P1", "P2", "P3", "P4"]
EXPECT_CURRENT = {"P1": "FAIL", "P2": "pass", "P3": "pass", "P4": "pass"}   # the dead path, before TASK-1930


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
    m = re.search(r"inline bool platformIsExternalReset\(\) \{.*?\n\}", plat, re.S)
    if not m:
        raise SystemExit("platformIsExternalReset() not found")
    (out / "extreset.inc").write_text("// GENERATED\n" + m.group(0).replace("inline ", "static ") + "\n",
                                      encoding="utf-8")


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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rev")
    a = ap.parse_args()
    out = GEN / "wifiportal"
    export(a.rev, out)
    exe = build(out)
    got = {}
    for case in CASES:
        r = subprocess.run([str(exe), case], capture_output=True, text=True)
        m = re.search(rf"^CASE {case} (pass|FAIL)\b", r.stdout, re.M)
        if not m:
            print(r.stdout, r.stderr)
            print(f"HARNESS ERROR: case {case} did not report")
            return 2
        got[case] = m.group(1)
        print(r.stdout.rstrip("\n"))
    match = got == EXPECT_CURRENT
    print(f"{a.rev or 'working tree'}: " + ", ".join(f"{k} {v}" for k, v in got.items()))
    print("RESULT: " + ("matches the dead-path expectation (P1 red)" if match else "differs from the dead-path expectation"))
    return 0 if match else 1


if __name__ == "__main__":
    sys.exit(main())
