#!/usr/bin/env python3
"""TASK-1913 host harness: a reset or a full filesystem during a save keeps the old settings.ini.

    python test/host/test_settings_atomic_write.py                       # FIX and mutants
    python test/host/test_settings_atomic_write.py --old-rev 4210b6481   # plus OLD, before the fix

writeSettings() opened /settings.ini with "w". esp_littlefs (1.20.3) syncs a file opened for
writing, so the truncation is on flash at once, and the new content only at close(): a reset in
between left an empty settings.ini and the next boot ran on defaults. A full filesystem left a
short one (TASK-1495 only detected that afterwards). updateRebootCount() had the same pattern.
The fix writes a temp file and renames it over the original (fsCommitTempFile(), helperStuff.ino);
littlefs replaces an existing destination in one commit.

test_settings_atomic_write.cpp is compiled from code extracted out of the revision under test:
- writeSettings() (settingStuff.ino), with the key lines between its opening `file.print(F("{\\n"));`
  and `size_t tail =` replaced by a stub body; the open, the close, the check and the commit are
  the revision's own;
- updateRebootCount() (helperStuff.ino), whole;
- the fsCommitTempFile block (helperStuff.ino markers), when the revision has it;
- the SETTINGS_FILE / SETTINGS_TMP_FILE defines (OTGW-firmware.h).
Only LittleFS is modelled (see the .cpp header for what the model follows and where from).

Runtime cases:
  W1  power cut in the middle of a save: settings.ini keeps the old content
  W2  a normal save replaces settings.ini, no temp file left
  W3  a full filesystem fails the save and keeps the old settings.ini, no temp file left
  W4  a save while settings.ini is open elsewhere fails and keeps it
  R1  power cut while the reboot counter is written: the old count stays
  R2  a normal boot counts 41 -> 42, no temp file left
Static cases, on the source text:
  S1  nothing in src/ opens SETTINGS_FILE or REBOOTCNT_FILE with "w"
  S2  updateRebootLog() does not remove the old log before the rename and checks the rename
  S3  the FSexplorer read guard covers SETTINGS_TMP_FILE (it holds the same secrets, ADR-056)
  S4  setup() removes a stale SETTINGS_TMP_FILE before the web server starts

Exit code: 0 PASS, 1 FAIL, 2 harness error. A slice or compile failure is a harness error.
"""
import argparse
import re
import subprocess
import sys

from test_ot_reserved_range import GEN, HERE, REPO, find_vcvars

FW = "src/OTGW-firmware/"
SET, HELP, HDR, FSX, INO = (FW + "settingStuff.ino", FW + "helperStuff.ino", FW + "OTGW-firmware.h",
                            FW + "FSexplorer.ino", FW + "OTGW-firmware.ino")
RUNTIME = ["W1", "W2", "W3", "W4", "R1", "R2"]
STATIC = ["S1", "S2", "S3", "S4"]
OLD_MUST_FAIL = ["W1", "W3", "R1", "S1", "S2", "S3", "S4"]

MUTANTS = [
    ("MA1", "writeSettings() opens settings.ini directly again", SET,
     [('File file = LittleFS.open(SETTINGS_TMP_FILE, "w");', 'File file = LittleFS.open(SETTINGS_FILE, "w");')],
     ["W1", "W2", "W3", "W4", "S1"]),   # W2/W4 too: the commit then finds no temp file and fails the save
    ("MA2", "fsCommitTempFile() renames without the size check", HELP,
     [("if (expected == 0 || onFlash != expected || !LittleFS.rename(tmpPath, path)) {",
       "if (!LittleFS.rename(tmpPath, path)) {")],
     ["W3"]),
    ("MA3", "updateRebootLog() removes the old log before the rename again", HELP,
     [("      if (!LittleFS.rename(TEMPLOG_FILE, REBOOTLOG_FILE)) {",
       "      LittleFS.remove(REBOOTLOG_FILE);\n      if (!LittleFS.rename(TEMPLOG_FILE, REBOOTLOG_FILE)) {")],
     ["S2"]),
    ("MA4", "the FSexplorer guard forgets the temp file", FSX,
     [(" || strcasecmp_P(sp, PSTR(SETTINGS_TMP_FILE)) == 0", "")],
     ["S3"]),
]


def read(path, rev):
    if rev is None:
        text = (REPO / path).read_text(encoding="utf-8", errors="replace")
    else:
        text = subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{path}"], capture_output=True,
                              text=True, encoding="utf-8", errors="replace", check=True).stdout
    return text.replace("\r\n", "\n")


def sources(rev):
    out = {p: read(p, rev) for p in (SET, HELP, HDR, FSX, INO)}
    out["src"] = all_src(rev)
    return out


def all_src(rev):
    if rev is None:
        files = [p for p in (REPO / FW).iterdir() if p.suffix in (".ino", ".h", ".cpp")]
        return {p.name: p.read_text(encoding="utf-8", errors="replace") for p in files}
    names = subprocess.run(["git", "-C", str(REPO), "ls-tree", "--name-only", rev, FW], capture_output=True,
                           text=True, check=True).stdout.split()
    return {n.split("/")[-1]: read(n, rev) for n in names if n.endswith((".ino", ".h", ".cpp"))}


def function_body(text, rx):
    lines = text.split("\n")
    hits = [i for i, l in enumerate(lines) if re.search(rx, l)]
    if len(hits) != 1:
        raise SystemExit(f"anchor must match exactly once, got {len(hits)}: {rx}")
    end = next((i for i in range(hits[0] + 1, len(lines)) if re.match(r"^\}", lines[i])), None)
    if end is None:
        raise SystemExit(f"no closing brace at column 0 after: {rx}")
    return lines[hits[0]:end + 1]


def settings_slice(src):
    body = function_body(src[SET], r"^bool writeSettings\(bool show\)")
    b = [i for i, l in enumerate(body) if l.strip() == 'file.print(F("{\\n"));']
    e = [i for i, l in enumerate(body) if re.match(r"^\s*size_t tail = file\.print\(", l)]
    if len(b) != 1 or len(e) != 1 or e[0] < b[0]:
        raise SystemExit(f"writeSettings(): need one opening print before one tail print, got {b} / {e}")
    return body[:b[0] + 1] + ["  writeSettingsBody(file);"] + body[e[0]:]


def helper_block(src):
    m = re.search(r"// --- fsCommitTempFile start\n(.*?)// --- fsCommitTempFile end", src[HELP], re.S)
    return m.group(1) if m else ""


def defines(src):
    return "\n".join(l for l in src[HDR].split("\n") if re.match(r"#define SETTINGS_(TMP_)?FILE\s", l))


def code(text):
    return re.sub(r"//[^\n]*", "", text)


def static_cases(src):
    opens = []
    for name, text in src["src"].items():
        for m in re.finditer(r'LittleFS\.open\(\s*(SETTINGS_FILE|REBOOTCNT_FILE)\s*,\s*"w"', code(text)):
            opens.append(f"{name}: {m.group(0)}")
    log = "\n".join(function_body(code(src[HELP]), r"^bool updateRebootLog\("))
    rn = log.find("LittleFS.rename(TEMPLOG_FILE, REBOOTLOG_FILE)")
    removed_first = re.search(r"LittleFS\.remove\(REBOOTLOG_FILE\)", log[:rn]) if rn >= 0 else None
    checked = re.search(r"if\s*\(\s*!LittleFS\.rename\(TEMPLOG_FILE, REBOOTLOG_FILE\)\)", log)
    fsx = code(src[FSX])
    guard = re.search(r"strcasecmp_P\(sp, PSTR\(SETTINGS_TMP_FILE\)\) == 0[^;]*checkHttpAuth\(\)", fsx, re.S)
    setup = "\n".join(function_body(code(src[INO]), r"^void setup\(\)"))
    rm = setup.find("LittleFS.remove(SETTINGS_TMP_FILE)")
    web = setup.find("startWebserver();")
    return {
        "S1": ("pass" if not opens else "FAIL", 'nothing opens SETTINGS_FILE or REBOOTCNT_FILE with "w"',
               ", ".join(opens) or "none"),
        "S2": ("pass" if rn >= 0 and not removed_first and checked else "FAIL",
               "updateRebootLog() renames over the old log without a remove first and checks the rename",
               f"rename {'found' if rn >= 0 else 'missing'}, remove before it {'yes' if removed_first else 'no'}, "
               f"checked {'yes' if checked else 'no'}"),
        "S3": ("pass" if guard else "FAIL", "the FSexplorer read guard covers SETTINGS_TMP_FILE",
               "guarded" if guard else "not guarded"),
        "S4": ("pass" if 0 <= rm < web else "FAIL", "setup() removes a stale SETTINGS_TMP_FILE before startWebserver()",
               f"remove at {rm}, startWebserver at {web}"),
    }


def export(src, out_dir):
    out_dir.mkdir(parents=True, exist_ok=True)
    head = "// GENERATED by test_settings_atomic_write.py. Do not edit.\n"
    (out_dir / "defines.inc").write_text(head + defines(src) + "\n", encoding="utf-8")
    (out_dir / "helper.inc").write_text(head + helper_block(src) + "\n", encoding="utf-8")
    (out_dir / "settings.inc").write_text(head + "\n".join(settings_slice(src)) + "\n", encoding="utf-8")
    reboot = function_body(src[HELP], r"^uint32_t updateRebootCount\(\)")
    (out_dir / "rebootcount.inc").write_text(head + "\n".join(reboot) + "\n", encoding="utf-8")


def build(label, out_dir):
    exe = out_dir / "test_settings_atomic_write.exe"
    if exe.exists():
        exe.unlink()
    bat = out_dir / "_compile.bat"
    fwd = str(out_dir).replace("\\", "/")
    bat.write_text("@echo off\r\n"
                   f"call \"{find_vcvars()}\" >nul 2>nul\r\n"
                   f"cl /nologo /EHsc /W3 /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS "
                   f"/Fo:\"{fwd}/\" /Fe:\"{exe}\" \"{HERE / 'test_settings_atomic_write.cpp'}\" /I\"{out_dir}\"\r\n",
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
    out["src"] = dict(src["src"])
    out["src"][path.split("/")[-1]] = text
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--old-rev")
    ap.add_argument("-q", "--quiet", action="store_true")
    a = ap.parse_args()
    ok = True
    fix = sources(None)
    failed = run_variant("FIX", GEN / "atomic_fix", fix, not a.quiet)
    print(f"FIX: {'PASS' if not failed else 'FAIL ' + ','.join(failed)}")
    ok &= not failed
    for mid, what, path, subs, expect in MUTANTS:
        got = run_variant(mid, GEN / f"atomic_{mid.lower()}", mutate(fix, path, subs, mid), False)
        good = got == sorted(expect)
        print(f"{mid} ({what}): fails {','.join(got) or 'nothing'}, expected {','.join(sorted(expect))}: "
              f"{'ok' if good else 'WRONG'}")
        ok &= good
    if a.old_rev:
        got = run_variant(f"OLD {a.old_rev}", GEN / "atomic_old", sources(a.old_rev), not a.quiet)
        missing = [c for c in OLD_MUST_FAIL if c not in got]
        print(f"OLD {a.old_rev}: fails {','.join(got) or 'nothing'}"
              f"{'' if not missing else ' -- expected also ' + ','.join(missing)}")
        ok &= not missing
    print("RESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
