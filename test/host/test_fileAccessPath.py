#!/usr/bin/env python3
"""TASK-1935 host harness: the access check in handleFile() and the file it opens name the same file.

    python test/host/test_fileAccessPath.py                   # working tree
    python test/host/test_fileAccessPath.py --old-rev HEAD    # old vs fix

handleFile() is sliced from src/OTGW-firmware/FSexplorer.ino of the revision under test (from the fsCanonicalPath()
comment block, or from 'bool handleFile(' on code without it, through the closing '}' at column 0), never copied.
The filesystem is the real littlefs of the installed ESP8266 core 2.7.4 on a RAM block device. The probe set is the
decoded form of otgw-firmware-09's pre-fix request set on .35 (the web server decodes %2e and %2f before
handleFile(), FSexplorer.ino onNotFound). With a password set and no credentials, a probe passes when it does not
return the settings file. The controls (plain and authorised reads, normal files, no password) must pass on both.

The run passes when FIX passes every probe and control and, with --old-rev, OLD leaks on at least the probes 09 saw
leak on the device while passing the controls. Compiler: MSVC via vswhere. Exit: 0 PASS, 1 FAIL, 2 harness error.
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
GEN = HERE / "build" / "file_access_path"
FSX = "src/OTGW-firmware/FSexplorer.ino"
LFS = Path.home() / "AppData/Local/Arduino15/packages/esp8266/hardware/esp8266/2.7.4/libraries/LittleFS/lib/littlefs"

# decoded paths (onNotFound decodes %2e -> '.', %2f -> '/', %2E -> '.'); 09 saw these leak on .35 (1.7.9)
LEAKED_ON_DEVICE = ["/./settings.ini", "/x/../settings.ini", "/.//settings.ini", "/sat/../settings.ini",
                    "/../settings.ini", "/a/b/../../settings.ini", "/../settings.ini", "/i18n/../settings.ini"]
EXTRA = ["/./././settings.ini", "/x/y/z/../../../settings.ini", "/sat/./../settings.ini", "/./x/./../settings.ini"]
PROBES = sorted(set(LEAKED_ON_DEVICE + EXTRA))


def find_vcvars():
    vswhere = Path(r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe")
    root = subprocess.run([str(vswhere), "-latest", "-products", "*", "-property", "installationPath"],
                          capture_output=True, text=True).stdout.strip()
    v = Path(root) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
    if not v.exists():
        raise SystemExit("vcvars64.bat not found")
    return v


def read(rev):
    if rev is None:
        return (REPO / FSX).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")
    return subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{FSX}"], capture_output=True, text=True,
                          encoding="utf-8", errors="replace", check=True).stdout.replace("\r\n", "\n")


def slice_handle_file(text):
    lines = text.split("\n")
    start = next((i for i, l in enumerate(lines) if l.startswith("// TASK-1935 (port of dev TASK-1934)")), None)
    if start is None:
        start = next(i for i, l in enumerate(lines) if re.match(r"^bool handleFile\(String&& path\)", l))
    hf = next(i for i in range(start, len(lines)) if re.match(r"^bool handleFile\(String&& path\)", lines[i]))
    end = next(i for i in range(hf + 1, len(lines)) if lines[i].startswith("}"))
    body = "\n".join(lines[start:end + 1])
    stmt = re.compile(r"\(\{(File f = LittleFS\.open\(path, \"r\"\); streamFileGuarded\(f, contentType\(path\)\); "
                      r"f\.close\(\); )true;\}\)")
    if len(stmt.findall(body)) != 1:
        raise SystemExit("the statement expression to rewrite was not found exactly once")
    return stmt.sub(lambda m: "[&]{" + m.group(1) + "return true;}()", body)


def build(label, rev, out):
    out.mkdir(parents=True, exist_ok=True)
    (out / "handlefile.inc").write_text("// GENERATED\n" + slice_handle_file(read(rev)) + "\n", encoding="utf-8")
    exe = out / "t.exe"
    if exe.exists():
        exe.unlink()
    fwd = str(out).replace("\\", "/")
    bat = out / "_c.bat"
    bat.write_text("@echo off\r\n"
                   f"call \"{find_vcvars()}\" >nul 2>nul\r\n"
                   f"cl /nologo /EHsc /W3 /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS /DLFS_NO_DEBUG /DLFS_NO_WARN "
                   f"/DLFS_NO_ERROR /Fo:\"{fwd}/\" /Fe:\"{exe}\" /I\"{out}\" /I\"{LFS}\" "
                   f"\"{HERE / 'test_fileAccessPath.cpp'}\" \"{LFS / 'lfs.c'}\" \"{LFS / 'lfs_util.c'}\"\r\n",
                   encoding="ascii")
    c = subprocess.run(["cmd.exe", "/c", str(bat)], capture_output=True, text=True, errors="replace")
    if c.returncode != 0 or not exe.exists():
        print(c.stdout[-4000:], c.stderr[-2000:])
        print(f"COMPILATION FAILED ({label})")
        sys.exit(2)
    return exe


def run(label, rev, out, quiet):
    exe = build(label, rev, out)
    p = subprocess.run([str(exe), "probe", *PROBES], capture_output=True, text=True)
    verdict = dict(reversed(l.split(" ", 1)) for l in p.stdout.splitlines() if " " in l)
    leaks = [x for x in PROBES if verdict.get(x) == "SETTINGS"]
    c = subprocess.run([str(exe), "controls"], capture_output=True, text=True)
    if not quiet:
        print(f"== {label} ==")
        for x in PROBES:
            print(f"  {verdict.get(x, '?'):9} {x}")
        print(c.stdout.rstrip())
    print(f"{label}: {len(leaks)} of {len(PROBES)} probes return the settings file without credentials; "
          f"controls {'ok' if c.returncode == 0 else 'FAIL'}")
    return leaks, c.returncode == 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--old-rev")
    ap.add_argument("-q", "--quiet", action="store_true")
    a = ap.parse_args()
    if not (LFS / "lfs.c").exists():
        print(f"HARNESS: littlefs of core 2.7.4 not found at {LFS}")
        return 2
    leaks, ctl = run("FIX", None, GEN / "fix", a.quiet)
    ok = not leaks and ctl
    if a.old_rev:
        old_leaks, old_ctl = run(f"OLD {a.old_rev}", a.old_rev, GEN / "old", a.quiet)
        missing = [x for x in set(LEAKED_ON_DEVICE) if x not in old_leaks]
        print(f"OLD reproduces the device leaks: {'yes' if not missing else 'NO, missing ' + ', '.join(missing)}")
        ok &= not missing and old_ctl
    print("RESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
