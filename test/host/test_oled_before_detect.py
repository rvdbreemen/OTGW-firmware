#!/usr/bin/env python3
"""TASK-1678 host harness: does setup()'s OLED bring-up find the display on a
board without a PIC before board detection, without breaking its I2C bus?

    python test/host/test_oled_before_detect.py                  # working tree
    python test/host/test_oled_before_detect.py --rev origin/dev # any git revision
    python test/host/test_oled_before_detect.py --mutant         # fallback attaches the button

Slices the REAL probeOLED(), oledStart() (when present), initOLED() and
initOLEDBeforeDetect() (when present) from src/OTGW-firmware/OLED.ino, plus the
real OLED call that setup() makes before startWiFi() in
src/OTGW-firmware/OTGW-firmware.ino, and compiles them with MSVC into
test_oled_before_detect.cpp against a fake I2C bus. Exit code: 0 PASS, 1 FAIL,
2 harness error.

Limit: the fake bus models only which pin pair the display answers on and
which pins get pinMode(); the real SSD1306 traffic and the WiFiManager portal
are proven on the bench, not here.
"""
import argparse
import re
import subprocess
import sys

from test_ot_reserved_range import GEN, HERE, REPO, find_vcvars

OLED_SRC = "src/OTGW-firmware/OLED.ino"
MAIN_SRC = "src/OTGW-firmware/OTGW-firmware.ino"


def read(path, rev):
    if rev is None:
        return (REPO / path).read_text(encoding="utf-8", errors="replace")
    r = subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{path}"],
                       capture_output=True, text=True, encoding="utf-8", errors="replace")
    if r.returncode != 0:
        raise SystemExit(f"git show {rev}:{path} failed: {r.stderr.strip()}")
    return r.stdout


def slice_function(lines, name, required=True):
    rx = re.compile(r"^(static\s+)?(void|bool)\s+" + re.escape(name) + r"\s*\(")
    begin = next((i for i, l in enumerate(lines) if rx.search(l)), None)
    if begin is None:
        if required:
            raise SystemExit(f"function not found: {name}")
        return None
    depth, out, opened = 0, [], False
    for line in lines[begin:]:
        out.append(line)
        depth += line.count("{") - line.count("}")
        opened = opened or "{" in line
        if opened and depth == 0:
            return "\n".join(out)
    raise SystemExit(f"unterminated function: {name}")


def slice_boot_call(lines):
    anchor = next((i for i, l in enumerate(lines) if "Bring up the OLED BEFORE startWiFi()" in l), None)
    if anchor is None:
        raise SystemExit("anchor not found: 'Bring up the OLED BEFORE startWiFi()'")
    for l in lines[anchor:]:
        s = l.strip()
        if s and not s.startswith("//"):
            m = re.match(r"^(initOLED\w*\(\);)", s)
            if not m:
                raise SystemExit(f"unexpected boot OLED statement: {s}")
            return m.group(1)
    raise SystemExit("no statement after the OLED anchor")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--rev", help="git revision to slice from (default: working tree)")
    ap.add_argument("--mutant", action="store_true",
                    help="make the pre-detection fallback attach the button (must FAIL)")
    args = ap.parse_args()
    origin = f"git {args.rev}" if args.rev else "working tree"

    oled = read(OLED_SRC, args.rev).splitlines()
    parts = [slice_function(oled, "probeOLED"),
             slice_function(oled, "oledStart", required=False),
             slice_function(oled, "initOLED"),
             slice_function(oled, "initOLEDBeforeDetect", required=False)]
    body = "\n\n".join(p for p in parts if p)
    if args.mutant:
        if "oledStart(false)" not in body:
            raise SystemExit("mutant needs oledStart(false) in the sliced code")
        body = body.replace("oledStart(false)", "oledStart(true)")
    boot = slice_boot_call(read(MAIN_SRC, args.rev).splitlines())

    GEN.mkdir(exist_ok=True)
    (GEN / "oled_before_detect.inc").write_text(
        f"// GENERATED from {OLED_SRC} and {MAIN_SRC} ({origin}) by test_oled_before_detect.py. Do not edit.\n"
        + body + "\n\nstatic void bootOledCall() { " + boot + " }\n",
        encoding="utf-8")
    print(f"== sliced the OLED init from {origin}{' (MUTANT)' if args.mutant else ''}; boot call: {boot}")

    exe = GEN / "test_oled_before_detect.exe"
    if exe.exists():
        exe.unlink()
    bat = GEN / "_compile_oled_before_detect.bat"
    gen_fwd = str(GEN).replace("\\", "/")
    bat.write_text("@echo off\r\n"
                   f"call \"{find_vcvars()}\" >nul 2>nul\r\n"
                   f"cl /nologo /EHsc /W3 /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS "
                   f"/Fo:\"{gen_fwd}/\" /Fe:\"{exe}\" \"{HERE / 'test_oled_before_detect.cpp'}\" /I\"{HERE}\"\r\n",
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
