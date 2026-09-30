#!/usr/bin/env python3
"""TASK-1179 AC#2 host harness: does a PR=A reply re-enable a PIC that boot
detection missed, through the real byte-level dispatch?

    python test/host/test_pic_banner_dispatch.py                  # working tree only
    python test/host/test_pic_banner_dispatch.py --old-rev 63f22a5e1

The PIC answers PR=A with "PR: A=OpenTherm Gateway x.x" (gateway.asm:5434-5435).
Those bytes pass OTGWSerial::read(), which feeds each one to matchBanner(); a
completed banner calls the registered callback fwreportinfo() on the PIC task,
which only raises g_picBannerPending. The loop's reportPendingPICRxErrors()
then runs applyPICBannerInfo(). This harness compiles that whole chain from
the REAL sources, sliced by anchor: matchBanner(), registerFirmwareCallback(),
firmwareVersion(), the banner table and file statics from the OTGWSerial
library; fwreportinfo(), the PIC-task flag block, applyPICBannerInfo() and
reportPendingPICRxErrors() from OTGW-Core.ino; isPICEnabled() from
OTGW-firmware.h. Only I/O is doubled (debug, MQTT, WebSocket).

With --old-rev, the OTGW-Core.ino and OTGW-firmware.h slices of that revision
are compiled against the same harness (the library is the same on both sides).
The run passes only when FIX passes every case and OLD fails exactly the two
defect cases (D1, D3) while passing the controls (D2, D4).
Exit code: 0 PASS, 1 FAIL, 2 harness error.
"""
import argparse
import re
import subprocess
import sys

from test_ot_reserved_range import GEN, HERE, REPO, find_vcvars, slice_by_anchor

LIB_H = "src/libraries/OTGWSerial/OTGWSerial.h"
LIB_CPP = "src/libraries/OTGWSerial/OTGWSerial.cpp"
CORE = "src/OTGW-firmware/OTGW-Core.ino"
FW_H = "src/OTGW-firmware/OTGW-firmware.h"

# (file, anchor) in the order the harness includes them. Library slices always
# come from the working tree; firmware slices from the revision under test.
LIB_SLICES = [
    (LIB_H, r"^typedef void OTGWFirmwareReport\("),
]
LIB_IMPL_SLICES = [
    (LIB_CPP, r"^static OTGWFirmware firmware\b"),
    (LIB_CPP, r"^static char fwversion\["),
    (LIB_CPP, r"^const char banner1\[\] PROGMEM"),
    (LIB_CPP, r"^const char banner2\[\] PROGMEM"),
    (LIB_CPP, r"^const char banner3\[\] PROGMEM"),
    (LIB_CPP, r"^const char\* const banners\[\] PROGMEM"),
    (LIB_CPP, r"^const char \*OTGWSerial::firmwareVersion\(\)"),
    (LIB_CPP, r"^void OTGWSerial::registerFirmwareCallback\("),
    (LIB_CPP, r"^void OTGWSerial::matchBanner\(char ch\)"),
]
# (file, start anchor, end anchor or None). With an end anchor the slice runs
# from the start line through the construct that begins on the end line; the
# declarations here carry trailing comments, which slice_by_anchor() cannot end on.
FW_SLICES = [
    (FW_H, r"^inline bool isPICEnabled\(", None),
    (CORE, r"^static volatile bool\s+g_picRxOverrunPending\b", r"^static volatile bool\s+g_picBannerPending\b"),
    (CORE, r"^static void applyPICBannerInfo\(\)", None),
    (CORE, r"^static uint32_t g_picRawDropTotal\b", r"^static void reportPendingPICRxErrors\(\)"),
    (CORE, r"^void fwreportinfo\(OTGWFirmware fw, const char \*version\)", None),
]
DEFECT = ["D1", "D3"]
CONTROL = ["D2", "D4"]


def read(path, rev):
    if rev is None:
        return (REPO / path).read_text(encoding="utf-8", errors="replace")
    return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{path}"], capture_output=True,
                          text=True, encoding="utf-8", errors="replace", check=True).stdout


def slice_range(lines, start_rx, end_rx):
    """From the line matching start_rx through the construct that begins on the
    line matching end_rx (its matching brace, or that line when it has none)."""
    hits = [i for i, l in enumerate(lines) if re.search(start_rx, l)]
    if len(hits) != 1:
        raise SystemExit(f"start anchor must match once, got {len(hits)}: {start_rx}")
    start = hits[0]
    end = next((i for i in range(start, len(lines)) if re.search(end_rx, lines[i])), None)
    if end is None:
        raise SystemExit(f"end anchor not found after the start: {end_rx}")
    if "{" not in lines[end]:
        return "\n".join(lines[start:end + 1])
    depth = 0
    for i in range(end, len(lines)):
        depth += lines[i].count("{") - lines[i].count("}")
        if depth == 0:
            return "\n".join(lines[start:i + 1])
    raise SystemExit(f"unterminated construct at: {end_rx}")


def slice_enum_ending(lines, name):
    """The typedef enum that closes with '} <name>;', sliced back to its 'typedef enum {' line."""
    end = next((i for i, l in enumerate(lines) if re.match(r"^\}\s*" + re.escape(name) + r"\s*;", l)), None)
    if end is None:
        raise SystemExit(f"anchor not found: }} {name};")
    start = next((i for i in range(end, -1, -1) if re.match(r"^typedef enum\s*\{", lines[i])), None)
    if start is None:
        raise SystemExit(f"no 'typedef enum {{' before }} {name};")
    return "\n".join(lines[start:end + 1])


def export(rev, out_dir):
    out_dir.mkdir(parents=True, exist_ok=True)
    lib_h = read(LIB_H, None).splitlines()
    lib_cpp = read(LIB_CPP, None).splitlines()
    parts_lib = [slice_enum_ending(lib_h, "OTGWFirmware")]
    parts_lib += [slice_by_anchor(lib_h, a) for _, a in LIB_SLICES]
    parts_impl = [slice_by_anchor(lib_cpp, a) for _, a in LIB_IMPL_SLICES]
    fw = {p: read(p, rev).splitlines() for p in (FW_H, CORE)}
    parts_fw = [slice_range(fw[p], a, e) if e else slice_by_anchor(fw[p], a) for p, a, e in FW_SLICES]
    origin = f"git {rev}" if rev else "working tree"
    head = f"// GENERATED by test_pic_banner_dispatch.py. Firmware slices: {origin}. Do not edit.\n"
    (out_dir / "dispatch_lib_types.inc").write_text(head + "\n\n".join(parts_lib) + "\n", encoding="utf-8")
    (out_dir / "dispatch_lib_impl.inc").write_text(head + "\n\n".join(parts_impl) + "\n", encoding="utf-8")
    (out_dir / "dispatch_fw.inc").write_text(head + "\n\n".join(parts_fw) + "\n", encoding="utf-8")


def build_and_run(label, out_dir):
    exe = out_dir / "test_pic_banner_dispatch.exe"
    if exe.exists():
        exe.unlink()
    bat = out_dir / "_compile.bat"
    fwd = str(out_dir).replace("\\", "/")
    bat.write_text("@echo off\r\n"
                   f"call \"{find_vcvars()}\" >nul 2>nul\r\n"
                   f"cl /nologo /EHsc /W3 /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS "
                   f"/Fo:\"{fwd}/\" /Fe:\"{exe}\" \"{HERE / 'test_pic_banner_dispatch.cpp'}\" /I\"{out_dir}\"\r\n",
                   encoding="ascii")
    c = subprocess.run(["cmd.exe", "/c", str(bat)], capture_output=True, text=True, errors="replace")
    if c.returncode != 0 or not exe.exists():
        print(c.stdout[-3000:], c.stderr[-2000:])
        print(f"COMPILATION FAILED ({label})")
        sys.exit(2)
    r = subprocess.run([str(exe)], capture_output=True, text=True)
    print(f"== {label} ==")
    print(r.stdout, end="")
    cases = dict(re.findall(r"^CASE (\S+) (pass|FAIL)\b", r.stdout, re.M))
    return r.returncode, cases


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--old-rev", help="also build OLD from this git revision and compare")
    args = ap.parse_args()
    fix_dir = GEN / "pic_banner_dispatch" / "fix"
    export(None, fix_dir)
    fix_rc, fix_cases = build_and_run("FIX (working tree)", fix_dir)
    if not args.old_rev:
        print("RESULT:", "PASS" if fix_rc == 0 else "FAIL")
        return 0 if fix_rc == 0 else 1
    old_dir = GEN / "pic_banner_dispatch" / "old"
    export(args.old_rev, old_dir)
    old_rc, old_cases = build_and_run(f"OLD (git {args.old_rev})", old_dir)
    checks = [
        ("FIX passes every case", fix_rc == 0 and all(v == "pass" for v in fix_cases.values())
         and sorted(fix_cases) == sorted(DEFECT + CONTROL)),
        ("OLD fails exactly the defect cases " + ",".join(DEFECT),
         all(old_cases.get(c) == "FAIL" for c in DEFECT)),
        ("OLD passes the controls " + ",".join(CONTROL), all(old_cases.get(c) == "pass" for c in CONTROL)),
        ("OLD exits 1", old_rc == 1),
    ]
    print("== old-vs-fix verdict ==")
    for text, ok in checks:
        print(f"  {text}: {'yes' if ok else 'NO'}")
    ok = all(ok for _, ok in checks)
    print("RESULT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
