#!/usr/bin/env python3
"""TASK-1774 host harness: a reset during a filesystem OTA keeps settings.ini, the reboot counter and the log.

    python test/host/test_fs_ota_rtc_snapshot.py                        # FIX and mutants
    python test/host/test_fs_ota_rtc_snapshot.py --old-rev 08f42d625    # plus OLD, before the fix

A filesystem OTA erases the LittleFS partition from its first chunk on, and the image it writes ships a
default settings.ini (src/OTGW-firmware/data/settings.ini, hostname OTGW2). A reset before the restore at
the end booted on that default with a lost counter (.63, 2026-11-10: 16 settings changed, boot count
178 -> 3). The fix (ADR-233) keeps the three files in an RTC_NOINIT snapshot.

test_fs_ota_rtc_snapshot.cpp is compiled from code extracted out of the revision under test:
- the fsOtaRtcSnapshot block (OTGW-ModUpdateServer-esp32.h markers), with FS_OTA_RTC_MIN_ADDR set to 0
  (a host address is not in RTC slow memory); empty before the fix, then the driver's stubs do nothing;
- the fsCommitTempFile block (helperStuff.ino markers);
- writeSettings() (settingStuff.ino), its key lines replaced by a stub body;
- updateRebootCount() (helperStuff.ino), whole.
The driver plays the upload server's order of calls; the static cases check that order on the source.

Runtime cases:
  T1  a reset during the write: the user's settings, the counter (41 -> 42) and the log come back
  T2  a complete filesystem OTA restores from RAM and leaves no armed snapshot
  T3  a reset after the restore's own settings write keeps that write, not the older snapshot
  T4  a settings save after a failed OTA wins over the snapshot, the counter is still kept
  T5  an upload refused before the first chunk leaves nothing in RTC
  T6  a settings.ini that does not fit is not kept partly; the counter and the log are
  T7  a snapshot with a bad CRC restores nothing and is wiped
  T8  an existing counter and log are not replaced; the snapshot is wiped at boot
  T9  a reset in the middle of the image write: the half-written, full image is formatted, then restored
  T10 a reset after Update.end() of a complete image: no format (the web UI stays), the restore as in T1
Static cases, on the source text:
  S1  _handleUploadStart() fills the snapshot after _keepFiles() and before LittleFS.end()
  S2  _handleUploadWrite() arms it after a successful chunk write
  S3  the two refusals before any write (too large, Update.begin() failed) wipe it
  S4  _restoreRuntimeFiles() wipes it only when the settings write succeeded
  S5  writeSettings() drops the settings slot after a successful commit
  S6  setup() restores between the LittleFS mount and readSettings()
  S7  _handleUploadEnd() marks the snapshot consistent after Update.end() and before the remount
  S8  _remountAfterFsWrite() marks it consistent after the format of a partial image

Exit code: 0 PASS, 1 FAIL, 2 harness error. A slice or compile failure is a harness error.
"""
import argparse
import re
import subprocess
import sys

from test_ot_reserved_range import GEN, HERE, REPO, find_vcvars

FW = "src/OTGW-firmware/"
UPD, SET, HELP, HDR, INO = (FW + "OTGW-ModUpdateServer-esp32.h", FW + "settingStuff.ino", FW + "helperStuff.ino",
                            FW + "OTGW-firmware.h", FW + "OTGW-firmware.ino")
RUNTIME = ["T1", "T2", "T3", "T4", "T5", "T6", "T7", "T8", "T9", "T10", "T11", "T12", "T13"]
STATIC = ["S1", "S2", "S3", "S4", "S5", "S6", "S7", "S8"]
OLD_MUST_FAIL = ["T1", "T4", "T6", "T9", "T10", "S1", "S2", "S3", "S4", "S5", "S6", "S7", "S8"]

MUTANTS = [
    ("MB1", "the boot restore treats settings.ini like the counter (only when missing)", UPD,
     [("    bool need = (i == 0);\n", "    bool need = false;\n")],
     ["T10"]),   # T1 passes anyway: its half image is formatted first, so settings.ini is missing
    ("MB2", "writeSettings() no longer drops the settings slot", SET,
     [("  fsOtaRtcDropSettings();   // TASK-1774: flash now holds newer settings than an armed RTC snapshot\n", "")],
     ["T3", "S5"]),   # T4: a write that never completed keeps the slot either way
    ("MB3", "the boot restore keeps the snapshot (no wipe)", UPD,
     [("    off += n;\n  }\n  fsOtaRtcWipe();\n}", "    off += n;\n  }\n}")],
     ["T8"]),
    ("MB4", "the boot restore skips the CRC check", UPD,
     [("|| g_fsOtaRtc.crc != fsOtaRtcCrc()) {", ") {")],
     ["T7"]),
    ("MB6", "the boot restore does not format a half-written image", UPD,
     [("  if (!g_fsOtaRtc.consistent) {   // the reset came during the image write: that half image takes no write\n",
       "  if (false) {   // the reset came during the image write: that half image takes no write\n")],
     ["T11", "T12", "T13", "T6", "T9"]),   # T13 through its unshifted control
    ("MB7", "Update.end() no longer marks the snapshot consistent", UPD,
     [("      fsOtaRtcMarkConsistent();\n    }\n    if (updateOk) {", "    }\n    if (updateOk) {")],
     ["S7"]),
    ("MB5", "arming moves to upload start", UPD,
     [("      fsOtaRtcFill();   // TASK-1774: survives a reset during the write; armed at the first chunk\n",
       "      fsOtaRtcFill();   // TASK-1774: survives a reset during the write; armed at the first chunk\n"
       "      fsOtaRtcArm();\n"),
      ("    if (_uploadTarget == UploadTarget::Filesystem) fsOtaRtcArm();   "
       "// TASK-1774: the old filesystem is being erased from here\n", "")],
     ["S2"]),
]


def read(path, rev):
    if rev is None:
        text = (REPO / path).read_text(encoding="utf-8", errors="replace")
    else:
        text = subprocess.run(["git", "-C", str(REPO), "show", f"{rev}:{path}"], capture_output=True,
                              text=True, encoding="utf-8", errors="replace", check=True).stdout
    return text.replace("\r\n", "\n")


def sources(rev):
    return {p: read(p, rev) for p in (UPD, SET, HELP, HDR, INO)}


def function_body(text, rx):
    lines = text.split("\n")
    hits = [i for i, l in enumerate(lines) if re.search(rx, l)]
    if len(hits) != 1:
        raise SystemExit(f"anchor must match exactly once, got {len(hits)}: {rx}")
    end = next((i for i in range(hits[0] + 1, len(lines)) if re.match(r"^\}", lines[i])), None)
    if end is None:
        raise SystemExit(f"no closing brace at column 0 after: {rx}")
    return lines[hits[0]:end + 1]


def method_body(text, rx):
    """A class member at 2-space indent: from its signature to the next line '  }'."""
    lines = text.split("\n")
    hits = [i for i, l in enumerate(lines) if re.search(rx, l)]
    if len(hits) != 1:
        raise SystemExit(f"anchor must match exactly once, got {len(hits)}: {rx}")
    end = next((i for i in range(hits[0] + 1, len(lines)) if lines[i] == "  }"), None)
    if end is None:
        raise SystemExit(f"no closing '  }}' after: {rx}")
    return "\n".join(lines[hits[0]:end + 1])


def settings_slice(src):
    body = function_body(src[SET], r"^bool writeSettings\(bool show\)")
    b = [i for i, l in enumerate(body) if l.strip() == 'file.print(F("{\\n"));']
    e = [i for i, l in enumerate(body) if re.match(r"^\s*size_t tail = file\.print\(", l)]
    if len(b) != 1 or len(e) != 1 or e[0] < b[0]:
        raise SystemExit(f"writeSettings(): need one opening print before one tail print, got {b} / {e}")
    return body[:b[0] + 1] + ["  writeSettingsBody(file);"] + body[e[0]:]


def block(text, name):
    m = re.search(rf"// --- {name} (?:start|begin)\n(.*?)// --- {name} end", text, re.S)
    return m.group(1) if m else ""


def snapshot_block(src):
    b = block(src[UPD], "fsOtaRtcSnapshot")
    if b:
        b, n = re.subn(r"#define FS_OTA_RTC_MIN_ADDR\s+0x[0-9A-Fa-f]+UL", "#define FS_OTA_RTC_MIN_ADDR 0UL", b)
        if n != 1:
            raise SystemExit("fsOtaRtcSnapshot block: FS_OTA_RTC_MIN_ADDR define not found once")
        if "fsOtaRtcMarkConsistent" not in b:   # the first (A), benched on .63 2026-11-14, had no such step
            b += "\ninline void fsOtaRtcMarkConsistent() {}\n"
    return b


def defines(src):
    return "\n".join(l for l in src[HDR].split("\n")
                     if re.match(r"#define (SETTINGS_(TMP_)?FILE|FS_UPDATE_INCOMPLETE_FILE)\s", l))


def code(text):
    return re.sub(r"//[^\n]*", "", text)


def order(text, *needles):
    pos = [text.find(n) for n in needles]
    return all(p >= 0 for p in pos) and pos == sorted(pos), pos


def static_cases(src):
    upd = code(src[UPD])
    start = method_body(upd, r"^  void _handleUploadStart\(")
    write = method_body(upd, r"^  void _handleUploadWrite\(")
    restore = method_body(upd, r"^  void _restoreRuntimeFiles\(")
    s1, p1 = order(start, "_keepFiles();", "fsOtaRtcFill();", "LittleFS.end();")
    s2, p2 = order(write, "Update.write(data, len)", "_uploadWrittenBytes += written;", "fsOtaRtcArm();")
    too = re.search(r'"filesystem image too large"[^}]*fsOtaRtcWipe\(\);', start, re.S)
    beg = re.search(r"!Update\.begin\(fsSize, U_SPIFFS\)\)\s*\{[^}]*fsOtaRtcWipe\(\);", start, re.S)
    early_arm = "fsOtaRtcArm" in start
    s4 = re.search(r"if \(saved\) fsOtaRtcWipe\(\);", restore) and restore.count("fsOtaRtcWipe") == 1
    ws = "\n".join(function_body(code(src[SET]), r"^bool writeSettings\(bool show\)"))
    c5 = ws.find("fsCommitTempFile(SETTINGS_TMP_FILE")
    r5 = ws.find("return false;", c5) if c5 >= 0 else -1
    d5 = ws.find("fsOtaRtcDropSettings();")
    s5, p5 = (0 <= c5 < r5 < d5), [c5, r5, d5]   # the failed commit returns first, the drop follows success
    setup = "\n".join(function_body(code(src[INO]), r"^void setup\(\)"))
    s6, p6 = order(setup, "LittleFSmounted = LittleFS.begin();", "fsOtaRtcRestoreAtBoot(LittleFSmounted);",
                   "readSettings(true);")
    end = method_body(upd, r"^  void _handleUploadEnd\(")
    s7, p7 = order(end, "Update.end(true);", "fsOtaRtcMarkConsistent();", "_remountAfterFsWrite(")
    remount = method_body(upd, r"^  bool _remountAfterFsWrite\(")
    s8, p8 = order(remount, "LittleFS.format();", "fsOtaRtcMarkConsistent();", "LittleFS.begin();")
    return {
        "S1": ("pass" if s1 else "FAIL", "_handleUploadStart() fills after _keepFiles() and before LittleFS.end()", str(p1)),
        "S2": ("pass" if s2 and not early_arm else "FAIL", "_handleUploadWrite() arms after a successful chunk write",
               f"{p2}, arm in upload start: {'yes' if early_arm else 'no'}"),
        "S3": ("pass" if too and beg else "FAIL", "the refusals before any write wipe the snapshot",
               f"too large {'wipes' if too else 'no wipe'}, begin failed {'wipes' if beg else 'no wipe'}"),
        "S4": ("pass" if s4 else "FAIL", "_restoreRuntimeFiles() wipes only after a successful settings write",
               "if (saved) fsOtaRtcWipe() once" if s4 else "missing or unconditional"),
        "S5": ("pass" if s5 else "FAIL", "writeSettings() drops the settings slot after a successful commit", str(p5)),
        "S6": ("pass" if s6 else "FAIL", "setup() restores between the mount and readSettings()", str(p6)),
        "S7": ("pass" if s7 else "FAIL", "_handleUploadEnd() marks consistent after Update.end(), before the remount", str(p7)),
        "S8": ("pass" if s8 else "FAIL", "_remountAfterFsWrite() marks consistent after the format", str(p8)),
    }


def export(src, out_dir):
    out_dir.mkdir(parents=True, exist_ok=True)
    head = "// GENERATED by test_fs_ota_rtc_snapshot.py. Do not edit.\n"
    (out_dir / "defines.inc").write_text(head + defines(src) + "\n", encoding="utf-8")
    (out_dir / "helper.inc").write_text(head + block(src[HELP], "fsCommitTempFile") + "\n", encoding="utf-8")
    (out_dir / "snapshot.inc").write_text(head + snapshot_block(src) + "\n", encoding="utf-8")
    (out_dir / "settings.inc").write_text(head + "\n".join(settings_slice(src)) + "\n", encoding="utf-8")
    reboot = function_body(src[HELP], r"^uint32_t updateRebootCount\(\)")
    (out_dir / "rebootcount.inc").write_text(head + "\n".join(reboot) + "\n", encoding="utf-8")


def build(label, out_dir):
    exe = out_dir / "test_fs_ota_rtc_snapshot.exe"
    if exe.exists():
        exe.unlink()
    bat = out_dir / "_compile.bat"
    fwd = str(out_dir).replace("\\", "/")
    bat.write_text("@echo off\r\n"
                   f"call \"{find_vcvars()}\" >nul 2>nul\r\n"
                   f"cl /nologo /EHsc /W3 /std:c++17 /utf-8 /D_CRT_SECURE_NO_WARNINGS "
                   f"/Fo:\"{fwd}/\" /Fe:\"{exe}\" \"{HERE / 'test_fs_ota_rtc_snapshot.cpp'}\" /I\"{out_dir}\"\r\n",
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
    has_block = bool(block(src[UPD], "fsOtaRtcSnapshot"))
    if has_block:
        for case, (verdict, title, got) in static_cases(src).items():
            results[case] = verdict
            text.append(f"CASE {case} {verdict}\n     {title}\n     got : {got}")
    else:
        for case in STATIC:
            results[case] = "FAIL"
            text.append(f"CASE {case} FAIL\n     no fsOtaRtcSnapshot block in this revision")
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
    failed = run_variant("FIX", GEN / "rtcsnap_fix", fix, not a.quiet)
    print(f"FIX: {'PASS' if not failed else 'FAIL ' + ','.join(failed)}")
    ok &= not failed
    for mid, what, path, subs, expect in MUTANTS:
        got = run_variant(mid, GEN / f"rtcsnap_{mid.lower()}", mutate(fix, path, subs, mid), False)
        good = got == sorted(expect)
        print(f"{mid} ({what}): fails {','.join(got) or 'nothing'}, expected {','.join(sorted(expect))}: "
              f"{'ok' if good else 'WRONG'}")
        ok &= good
    if a.old_rev:
        got = run_variant(f"OLD {a.old_rev}", GEN / "rtcsnap_old", sources(a.old_rev), not a.quiet)
        missing = [c for c in OLD_MUST_FAIL if c not in got]
        print(f"OLD {a.old_rev}: fails {','.join(got) or 'nothing'}"
              f"{'' if not missing else ' -- expected also ' + ','.join(missing)}")
        ok &= not missing
    print("RESULT: " + ("PASS" if ok else "FAIL"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
