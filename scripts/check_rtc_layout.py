#!/usr/bin/env python3
"""TASK-1918: the main firmware keeps its RTC_NOINIT records out of the first 32 B of RTC slow memory.

    python scripts/check_rtc_layout.py <firmware.elf>      # exit 0 OK, 1 violation, 2 tool error

The safeboot updater (ADR-191) has an initialised .rtc.force_slow segment of 0x20 B at 0x50000200 (the IDF variable
s_sleep_sub_mode_ref_cnt). The bootloader loads it every time safeboot starts, so anything the main firmware keeps
in 0x50000200-0x5000021F is overwritten on the way through safeboot (an app OTA). The main firmware reserves that
range with the guard array g_rtcSafebootGuard (initialised RTC data, .rtc.data, which the IDF linker script
places before .rtc_noinit) and keeps every RTC_NOINIT record at or above 0x50000220.

The check reads the ELF symbol table with the toolchain nm (-n -S) and fails when the guard is missing or not at
0x50000200 with at least 0x20 B, when _rtc_noinit_start is not exactly 0x50000220 (the records keep their address
across versions), or when a sized symbol inside [_rtc_noinit_start, _rtc_noinit_end) starts below 0x50000220.
"""
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

SAFEBOOT_RTC_END = 0x50000220      # end of safeboot's 0x20 B .rtc.force_slow load at 0x50000200
GUARD = "g_rtcSafebootGuard"


def find_nm():
    for cand in (shutil.which("xtensa-esp-elf-nm"), shutil.which("xtensa-esp32s3-elf-nm")):
        if cand:
            return cand
    roots = [os.environ.get("PLATFORMIO_CORE_DIR", ""), r"D:\DevData\platformio", str(Path.home() / ".platformio")]
    for root in filter(None, roots):
        for name in ("xtensa-esp-elf-nm.exe", "xtensa-esp-elf-nm"):
            p = Path(root) / "packages" / "toolchain-xtensa-esp-elf" / "bin" / name
            if p.exists():
                return str(p)
    return None


def parse_nm(text):
    """nm -n -S output -> (markers {name: addr}, sized [(addr, size, name)])."""
    markers, sized = {}, []
    for line in text.splitlines():
        parts = line.split()
        if len(parts) == 3:
            markers[parts[2]] = int(parts[0], 16)
        elif len(parts) == 4:
            sized.append((int(parts[0], 16), int(parts[1], 16), parts[3]))
    return markers, sized


def check(nm_text):
    """Returns a list of violation strings (empty = OK)."""
    markers, sized = parse_nm(nm_text)
    start, end = markers.get("_rtc_noinit_start"), markers.get("_rtc_noinit_end")
    if start is None or end is None:
        return ["_rtc_noinit_start/_rtc_noinit_end not in the symbol table"]
    bad = []
    if start != SAFEBOOT_RTC_END:
        bad.append(f"_rtc_noinit_start is 0x{start:08x}, must be exactly 0x{SAFEBOOT_RTC_END:08x}: the records must keep "
                   "their address across versions, so the reason the old firmware wrote is found after an OTA")
    guard = [(a, s, n) for a, s, n in sized if GUARD in n]
    if not guard:
        bad.append(f"{GUARD} (the 32 B safeboot guard) is not in the ELF")
    for a, s, n in guard:
        if a != 0x50000200 or s < SAFEBOOT_RTC_END - 0x50000200:
            bad.append(f"{GUARD} at 0x{a:08x} size 0x{s:x}: must start at 0x50000200 and cover 0x20 B")
    for a, s, n in sized:
        if start <= a < end and GUARD not in n and a < SAFEBOOT_RTC_END:
            bad.append(f"{n} at 0x{a:08x} (size 0x{s:x}) lies in safeboot's RTC range 0x50000200-0x5000021f")
    return bad


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    nm = find_nm()
    if not nm:
        print("check_rtc_layout: toolchain nm not found")
        return 2
    r = subprocess.run([nm, "-n", "-S", "-C", argv[1]], capture_output=True, text=True, errors="replace")
    if r.returncode != 0:
        print(f"check_rtc_layout: nm failed: {r.stderr.strip()[:300]}")
        return 2
    bad = check(r.stdout)
    if bad:
        print("RTC layout check FAILED (TASK-1918):")
        for b in bad:
            print("  " + b)
        return 1
    print("RTC layout check OK: no RTC_NOINIT record in safeboot's 0x50000200-0x5000021f")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
