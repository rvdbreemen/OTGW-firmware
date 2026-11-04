#!/usr/bin/env python3
"""TASK-1218 host harness: does the PIC-flash progress LED go through LEDC on
an LEDC-attached pin, and through digitalWrite on a plain GPIO?

    python test/host/test_pic_led_ledc.py                    # working tree
    python test/host/test_pic_led_ledc.py --old-rev <rev>    # also OLD from that revision

Sliced by anchor from the real sources (never copied): setLed() and the LEDC
statics from OTGW-firmware.ino, picProgressLedWrite() and the LED prelude of
fwupgradestart() from OTGW-Core.ino, OTGWSerial::SetLED() from the library
(class name renamed to the harness stub, nothing else). The platform double
refuses digitalWrite on an LEDC-attached pin like arduino-esp32 3.x does.
The harness is built twice: LEDC LEDs (cases L1, L2) and -DPLAIN_GPIO (P1).
With --old-rev the .ino/Core slices come from that revision; the run passes
only when FIX passes everything and OLD fails exactly L1,L2 while passing P1.
Exit code: 0 PASS, 1 FAIL, 2 harness error.
"""
import argparse
import re
import subprocess
import sys

from test_ot_reserved_range import GEN, HERE, REPO, find_vcvars, slice_by_anchor

INO = "src/OTGW-firmware/OTGW-firmware.ino"
CORE = "src/OTGW-firmware/OTGW-Core.ino"
LIB_CPP = "src/libraries/OTGWSerial/OTGWSerial.cpp"


def read(path, rev):
    if rev is None:
        return (REPO / path).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")
    return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{path}"], capture_output=True,
                          text=True, encoding="utf-8", errors="replace", check=True).stdout.replace("\r\n", "\n")


def export(rev, out):
    out.mkdir(parents=True, exist_ok=True)
    ino = read(INO, rev).splitlines()
    core_txt = read(CORE, rev)
    core = core_txt.splitlines()
    lib = read(LIB_CPP, rev).splitlines()
    led = [slice_by_anchor(ino, a) for a in (
        r"^static bool _led_state\[2\]", r"^static bool _ledsInitDone", r"^static inline uint8_t _ledIdx",
        r"^static void _ensureLEDsInit\(\)", r"^void setLed\(uint8_t led, uint8_t status\) \{")]
    (out / "led_ledc.inc").write_text("\n\n".join(led) + "\n", encoding="utf-8")
    (out / "led_plain.inc").write_text(slice_by_anchor(ino, r"^void setLed\(uint8_t led, uint8_t status\)\{") + "\n",
                                       encoding="utf-8")
    sl = slice_by_anchor(lib, r"^void OTGWSerial::SetLED\(int state\)").replace("OTGWSerial::", "OTGWSerialStub::")
    (out / "led_lib.inc").write_text(sl + "\n", encoding="utf-8")
    has_cb = re.search(r"^static void picProgressLedWrite\(", core_txt, re.M) is not None
    (out / "led_core_cb.inc").write_text((slice_by_anchor(core, r"^static void picProgressLedWrite\(")
                                          if has_cb else "// (no callback in this revision)") + "\n", encoding="utf-8")
    # LED prelude of fwupgradestart(): the lines between the comment and the debug line.
    start = next(i for i, l in enumerate(core) if "// Turn on LED to indicate flashing" in l)
    end = next(i for i in range(start, len(core)) if 'F("LED1 activated' in core[i])
    (out / "led_core_prelude.inc").write_text("\n".join(core[start + 1:end]) + "\n", encoding="utf-8")


def build_and_run(label, out, plain):
    exe = out / ("test_pic_led_plain.exe" if plain else "test_pic_led_ledc.exe")
    if exe.exists():
        exe.unlink()
    bat = out / ("_compile_plain.bat" if plain else "_compile_ledc.bat")
    fwd = str(out).replace("\\", "/")
    bat.write_text("@echo off\r\n"
                   f"call \"{find_vcvars()}\" >nul 2>nul\r\n"
                   f"cl /nologo /EHsc /W3 /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS {'/DPLAIN_GPIO ' if plain else ''}"
                   f"/Fo:\"{fwd}/{exe.stem}.obj\" /Fe:\"{exe}\" \"{HERE / 'test_pic_led_ledc.cpp'}\" /I\"{out}\"\r\n",
                   encoding="ascii")
    c = subprocess.run(["cmd.exe", "/c", str(bat)], capture_output=True, text=True, errors="replace")
    if c.returncode != 0 or not exe.exists():
        print(c.stdout[-3000:], c.stderr[-2000:])
        print(f"COMPILATION FAILED ({label})")
        sys.exit(2)
    r = subprocess.run([str(exe)], capture_output=True, text=True)
    print(f"== {label} {'plain GPIO' if plain else 'LEDC'} ==")
    print(r.stdout, end="")
    return r.returncode, dict(re.findall(r"^CASE (\S+) (pass|FAIL)\b", r.stdout, re.M))


def run(label, rev, name):
    out = GEN / "pic_led_ledc" / name
    export(rev, out)
    rc_l, c_l = build_and_run(label, out, False)
    rc_p, c_p = build_and_run(label, out, True)
    return rc_l, rc_p, {**c_l, **c_p}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--old-rev")
    args = ap.parse_args()
    rl, rp, fix = run("FIX (working tree)", None, "fix")
    fix_ok = rl == 0 and rp == 0 and sorted(fix) == ["L1", "L2", "P1"] and all(v == "pass" for v in fix.values())
    if not args.old_rev:
        print("RESULT:", "PASS" if fix_ok else "FAIL")
        return 0 if fix_ok else 1
    _, _, old = run(f"OLD (git {args.old_rev})", args.old_rev, "old")
    checks = [("FIX passes L1,L2,P1", fix_ok),
              ("OLD fails L1,L2 (digitalWrite on an LEDC pin)", old.get("L1") == "FAIL" and old.get("L2") == "FAIL"),
              ("OLD passes the plain-GPIO control P1", old.get("P1") == "pass")]
    print("== old-vs-fix verdict ==")
    for t, ok in checks:
        print(f"  {t}: {'yes' if ok else 'NO'}")
    ok = all(o for _, o in checks)
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
