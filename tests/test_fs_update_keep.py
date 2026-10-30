#!/usr/bin/env python3
"""
TASK-1286: what a filesystem update keeps.

A filesystem update writes a fresh LittleFS image over the whole partition.
OTGW-ModUpdateServer-esp32.h puts runtime files back in two ways:
  - from RAM, after the remount: settings.ini, the SAT learned state
    (satPersistAllToFs), the OpenTherm profiles (saveOtSupportFilesForced) and,
    in doRestart(), the DHW water total;
  - from a heap copy made before the write (KEEP_LIST), for files without a RAM
    model.

This is a SOURCE test: it reads the firmware text with comments removed, it does
not run the C++ (the update server class needs the ESP32 core and a device). A
call that is only mentioned in a comment, or sits behind an if, does not count.
It pins:
  - the keep list, its per-file caps, the total cap and three distinct skip
    messages (over the file cap, over the total cap, low heap);
  - the keep list names the files the writers really create;
  - the copy happens before LittleFS.end(); the write-back runs after the
    remount, in order, unguarded, while the update flag is up;
  - the copies are freed on every failure path (image too large, Update.begin
    fails, short write, error answer, end, abort, a second upload);
  - satPersistAllToFs() calls every SAT saver, and no saver has a "due" guard;
  - both OpenTherm profile files are marked dirty before they are saved;
  - every writer under /sat creates the folder (a fresh image has none);
  - the guide and the /update page say the same.
The bench proof (values equal before and after a real update) is a separate step.

OTGW_SRC_ROOT points the test at another tree (used to show it goes red on
mutated copies). Run: python tests/test_fs_update_keep.py
"""

import io
import os
import re
import sys
import unittest
from pathlib import Path

if hasattr(sys.stdout, "buffer"):
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

REPO_ROOT = Path(os.environ.get("OTGW_SRC_ROOT") or Path(__file__).resolve().parent.parent)


def read(*parts):
    return Path(REPO_ROOT, *parts).read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


def strip_comments(src):
    """Remove // and /* */ comments, keep string and char literals as they are."""
    out, i, n = [], 0, len(src)
    while i < n:
        c = src[i]
        if c in "\"'":
            j = i + 1
            while j < n and src[j] != c:
                j += 2 if src[j] == "\\" else 1
            out.append(src[i:j + 1])
            i = j + 1
        elif src.startswith("//", i):
            while i < n and src[i] != "\n":
                i += 1
        elif src.startswith("/*", i):
            j = src.find("*/", i + 2)
            i = n if j < 0 else j + 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def code(*parts):
    return strip_comments(read(*parts))


FWDIR = ("src", "OTGW-firmware")
UPD = code(*FWDIR, "OTGW-ModUpdateServer-esp32.h")
SATCTL = code(*FWDIR, "SATcontrol.ino")
SATCYC = code(*FWDIR, "SATcycles.ino")
OTCORE = code(*FWDIR, "OTGW-Core.ino")

EXPECTED_KEEP = {
    "/dallas_labels.ini": 2048,
    "/sat_markers.json": 2560,
    "/reboot_count.txt": 16,
    "/reboot_log.txt": 3072,
}
TOTAL_MAX = 8192


def function_body(src, signature_regex):
    """Text of the brace-balanced block that follows the first match (src has no comments)."""
    m = re.search(signature_regex, src)
    assert m, "function not found: " + signature_regex
    i = src.index("{", m.end())
    depth = 0
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[i:j + 1]
    raise AssertionError("unbalanced braces after " + signature_regex)


def is_plain_statement(body, stmt):
    """stmt is on its own line and no brace-less if/else/for/while controls it."""
    lines = [l.strip() for l in body.split("\n") if l.strip()]
    if stmt not in lines:
        return False
    prev = lines[lines.index(stmt) - 1]
    controlled = re.match(r"(\}\s*)?(else\s+)?(if|for|while)\b.*\)$", prev) or prev in ("else", "} else")
    return not controlled


def keep_list():
    m = re.search(r"KEEP_LIST\[\]\s*=\s*\{(.*?)\n\s*\};", UPD, re.S)
    assert m, "KEEP_LIST not found"
    return {p: int(n) for p, n in re.findall(r'\{\s*"([^"]+)"\s*,\s*(\d+)\s*\}', m.group(1))}


END = r"void _handleUploadEnd\(size_t totalSize\)"
ABORT = r"void _handleUploadAbort\(\)"
RESTORE = r"void _restoreRuntimeFiles\(\)"
START = r"void _handleUploadStart\(AsyncWebServerRequest \*request, const String &filename\)"


class TestKeepList(unittest.TestCase):
    def test_list_contents(self):
        self.assertEqual(keep_list(), EXPECTED_KEEP)

    def test_caps(self):
        caps = keep_list()
        self.assertEqual(caps["/dallas_labels.ini"], 2048, "labels behaviour must stay as TASK-1266 left it")
        self.assertTrue(all(0 < c <= 3072 for c in caps.values()), caps)
        self.assertRegex(UPD, r"KEEP_TOTAL_MAX\s*=\s*%d\s*;" % TOTAL_MAX)
        self.assertRegex(UPD, r"KEEP_HEAP_RESERVE\s*=\s*\d+\s*;")

    def test_keep_names_match_the_writers(self):
        """A rename in a writer must not leave the keep list pointing at a dead name."""
        writers = {
            "/dallas_labels.ini": ("sensors_ext.ino", "dallas_labels.ini"),
            "/sat_markers.json": ("restAPI.ino", "/sat_markers.json"),
            "/reboot_count.txt": ("helperStuff.ino", "/reboot_count.txt"),
            "/reboot_log.txt": ("helperStuff.ino", "/reboot_log.txt"),
        }
        for path, (fname, needle) in writers.items():
            self.assertIn(needle, code(*FWDIR, fname), "%s: %s no longer names it" % (path, fname))

    def test_heap_checked_before_alloc(self):
        body = function_body(UPD, r"void _keepFiles\(\)")
        check = re.search(r"if \(platformMaxFreeBlock\(\) < len \+ KEEP_HEAP_RESERVE\)", body)
        self.assertTrue(check, "no heap check against the largest free block")
        self.assertLess(check.start(), body.index("malloc("))

    def test_three_distinct_skip_messages(self):
        """Each skip reason sits in its own branch with its own message."""
        body = function_body(UPD, r"void _keepFiles\(\)")
        self.assertRegex(body, r"if \(len > KEEP_LIST\[i\]\.maxLen\) \{[^}]*too large to keep")
        self.assertRegex(body, r"if \(total \+ len > KEEP_TOTAL_MAX\) \{[^}]*total cap")
        self.assertRegex(body, r"if \(platformMaxFreeBlock\(\) < len \+ KEEP_HEAP_RESERVE\) \{[^}]*low heap")
        self.assertNotIn("||", body[:body.index("malloc(")], "a combined condition would share one message")

    def test_restore_and_drop(self):
        drop = function_body(UPD, r"void _dropKeptFiles\(\)")
        self.assertIn("free(_keepBuf[i])", drop)
        self.assertIn("_keepBuf[i] = nullptr", drop)
        self.assertIn("_keepLen[i] = 0", drop)
        self.assertIn("_dropKeptFiles();", function_body(UPD, r"void _restoreKeptFiles\(\)"))
        self.assertIn("_dropKeptFiles();", function_body(UPD, END))
        self.assertIn("_dropKeptFiles();", function_body(UPD, r"void _handleUploadAbort\(\)"))
        self.assertIn("_dropKeptFiles();", function_body(UPD, r"void _keepFiles\(\)"), "a second upload must not leak the first copy")

    def test_copies_freed_on_every_failure_path(self):
        """After these errors _handleUploadEnd does not run, and the error answer clears bESPactive,
        which turns the abort cleanup off: each path has to free the copies itself."""
        drop = r"\s*_dropKeptFiles\(\);"
        self.assertTrue(re.search(r'"filesystem image too large", sizeof\(_updaterError\)\);' + drop, UPD), "image too large")
        self.assertTrue(re.search(r"!Update\.begin\(fsSize, U_SPIFFS\)\) \{\s*_setUpdaterError\(\);" + drop, UPD), "Update.begin fails")
        self.assertTrue(re.search(r"if \(written != len\) \{\s*_setUpdaterError\(\);" + drop, UPD), "short write")
        self.assertTrue(re.search(r"if \(Update\.hasError\(\) \|\| _updaterError\[0\] != '\\0'\) \{\s*::state\.flash\.bESPactive = false;" + drop,
                         UPD), "error answer")
        # an Update.end(true) failure and an incomplete image reach the end handler's own drop
        end = function_body(UPD, END)
        self.assertTrue(is_plain_statement(end, "_dropKeptFiles();"), "the end handler's drop must be unconditional")

    def test_old_labels_names_are_gone(self):
        for old in ("_keepLabelsFile", "_restoreLabelsFile", "_dropLabelsFile", "_labelsCopy", "LABELS_KEEP_MAX"):
            self.assertNotIn(old, UPD, old)


class TestOrdering(unittest.TestCase):
    def test_copy_before_unmount(self):
        start = function_body(UPD, START)
        self.assertTrue(is_plain_statement(start, "_keepFiles();"))
        self.assertLess(start.index("_keepFiles();"), start.index("LittleFS.end()"))

    def test_writeback_steps_in_order(self):
        restore = function_body(UPD, RESTORE)
        order = ["bool saved = writeSettings(false);", "settingsMarkClean();", "if (!saved) settingsMarkDirty();",
                 "_restoreKeptFiles();", "saveOtSupportFilesForced();", "satPersistAllToFs();"]
        pos = [restore.find(s) for s in order]
        self.assertTrue(all(p >= 0 for p in pos), dict(zip(order, pos)))
        self.assertEqual(pos, sorted(pos), "write-back steps out of order: " + str(dict(zip(order, pos))))
        for stmt in order:
            self.assertTrue(is_plain_statement(restore, stmt), stmt + " is guarded or not a statement of its own")

    def test_writeback_after_remount(self):
        end = function_body(UPD, END)
        call = "_restoreRuntimeFiles();"
        self.assertTrue(is_plain_statement(end, call), "end handler: " + call + " missing or guarded")
        pos = end.index(call)
        self.assertLess(end.index("LittleFSmounted = _remountAfterFsWrite("), pos)
        # inside the "remount worked and the image is complete" branch
        self.assertLess(end.index("version.hash"), pos)
        self.assertLess(pos, end.index("LittleFS mount failed"))
        # still while the update flag is up (the loop task stays off these files)
        self.assertLess(pos, end.index("bESPactive = false"))


    def test_declarations(self):
        self.assertRegex(UPD, r"extern void satPersistAllToFs\(\);")
        self.assertRegex(UPD, r"extern void saveOtSupportFilesForced\(\);")
        hdr = code(*FWDIR, "OTGW-firmware.h")
        self.assertRegex(hdr, r"void satPersistAllToFs\(\);")
        self.assertRegex(hdr, r"void saveOtSupportFilesForced\(\);")


class TestAbortRestore(unittest.TestCase):
    """TASK-1495: an interrupted filesystem upload leaves the sectors written so far
    (no rollback for U_SPIFFS), so settings.ini can be the new image's shipped default
    while RAM holds the user's settings. The abort path must write them back."""

    def setUp(self):
        self.abort = function_body(UPD, ABORT)

    def test_abort_restores_after_remount(self):
        self.assertRegex(self.abort, r"if \(LittleFSmounted\) \{\s*_restoreRuntimeFiles\(\);",
                         "abort must restore the runtime files once the remount worked")
        pos = self.abort.index("_restoreRuntimeFiles();")
        self.assertLess(self.abort.index("LittleFSmounted = _remountAfterFsWrite("), pos)
        self.assertLess(pos, self.abort.index("bESPactive = false"))

    def test_abort_drops_copies_only_after_the_restore(self):
        drops = [m.start() for m in re.finditer(r"_dropKeptFiles\(\);", self.abort)]
        self.assertEqual(len(drops), 1, "abort frees the heap copies once")
        self.assertLess(self.abort.index("_restoreRuntimeFiles();"), drops[0],
                        "the copies are freed before the restore could use them")
        self.assertTrue(is_plain_statement(self.abort, "_dropKeptFiles();"))

    def test_abort_marks_the_filesystem_incomplete(self):
        self.assertRegex(self.abort, r"if \(_uploadWrittenBytes > 0\) _markFsUpdateIncomplete\(\);",
                         "a 0-byte abort left the old image intact and needs no marker")
        self.assertLess(self.abort.index("_restoreRuntimeFiles();"), self.abort.index("_markFsUpdateIncomplete();"))

    def test_short_upload_at_end_is_an_error_with_marker(self):
        end = function_body(UPD, END)
        m = re.search(r"if \(shortUpload\) \{([^}]*)\}", end)
        self.assertTrue(m, "the end handler must not trust version.hash alone")
        self.assertIn("_updaterError", m.group(1))
        self.assertIn("_markFsUpdateIncomplete();", m.group(1))

    def test_partial_write_formats_before_remount_and_restore(self):
        """Bench 2026-10-30: after a dropped upload at ~90% the sectors written so far stay
        (no rollback for U_SPIFFS) and the LittleFS accepts no writes ("short write (0 of
        1512 bytes)"), so the restore was lost. A partial image is formatted first."""
        helper = function_body(UPD, r"bool _remountAfterFsWrite\(bool partial\)")
        self.assertRegex(helper, r"if \(partial\) \{[^}]*LittleFS\.format\(\)")
        self.assertLess(helper.index("LittleFS.format()"), helper.index("LittleFS.begin()"),
                        "format the unmounted partition, then mount it")
        self.assertIn("millis()", helper, "the format duration is logged")
        self.assertRegex(self.abort, r"LittleFSmounted = _remountAfterFsWrite\(_uploadWrittenBytes > 0\);")
        self.assertLess(self.abort.index("_remountAfterFsWrite("), self.abort.index("_restoreRuntimeFiles();"))
        self.assertNotIn("LittleFS.begin()", self.abort, "the abort remounts through the helper only")

    def test_short_upload_at_end_formats_before_restore(self):
        end = function_body(UPD, END)
        self.assertRegex(end, r"const bool shortUpload = _uploadExpectedBytes > 0 && _uploadWrittenBytes != _uploadExpectedBytes;")
        self.assertRegex(end, r"LittleFSmounted = _remountAfterFsWrite\(shortUpload\);")
        self.assertLess(end.index("_remountAfterFsWrite("), end.index("_restoreRuntimeFiles();"))
        self.assertRegex(end, r"LittleFSmounted && !shortUpload && !LittleFS\.exists\(F\(\"/version\.hash\"\)\)",
                         "a formatted short upload has no version.hash; that is not the version.hash error")

    def test_abort_remount_not_gated_on_mounted_flag(self):
        """Bench-found bug of 2026-10-30: _handleUploadStart calls LittleFS.end() but keeps
        LittleFSmounted true (isFlashing() relies on it), so an abort condition that also
        required !LittleFSmounted never ran: no remount, no restore, no marker."""
        start = function_body(UPD, START)
        self.assertIn("LittleFS.end();", start)
        self.assertNotRegex(start, r"LittleFSmounted\s*=\s*false",
                            "the start keeps the flag true across LittleFS.end()")
        m = re.search(r"if \(([^\n]*)\) \{\s*LittleFSmounted = _remountAfterFsWrite\(", self.abort)
        self.assertTrue(m, "abort must remount the filesystem")
        self.assertIn("UploadTarget::Filesystem", m.group(1))
        self.assertNotIn("!LittleFSmounted", m.group(1),
                         "the flag is still true after the start's LittleFS.end(), so it cannot gate the remount")

    def test_marker_is_not_kept_and_is_reported(self):
        self.assertNotIn(".fs_update_incomplete", keep_list(), "a complete image must remove the marker")
        mark = function_body(UPD, r"void _markFsUpdateIncomplete\(\)")
        self.assertIn("LittleFS.open(FS_UPDATE_INCOMPLETE_FILE", mark)
        hdr = code(*FWDIR, "OTGW-firmware.h")
        self.assertRegex(hdr, r'#define FS_UPDATE_INCOMPLETE_FILE\s+"/\.fs_update_incomplete"')
        rest = code(*FWDIR, "restAPI.ino")
        self.assertRegex(rest, r'je\.field\(F\("fsupdateincomplete"\), LittleFSmounted && LittleFS\.exists\(F\(FS_UPDATE_INCOMPLETE_FILE\)\)\);')


class TestSettingsPersistence(unittest.TestCase):
    """TASK-1495: a failed settings write must not clear the dirty flag, and an empty or
    unreadable settings.ini must be rewritten from RAM at the next flush."""

    def setUp(self):
        self.st = code(*FWDIR, "settingStuff.ino")

    def test_write_settings_reports_failure(self):
        self.assertRegex(code(*FWDIR, "OTGW-firmware.h"), r"bool writeSettings\(bool show\);")
        self.assertRegex(UPD, r"extern bool writeSettings\(bool show\);")
        body = function_body(self.st, r"bool writeSettings\(bool show\)")
        self.assertRegex(body, r"if \(!file\)\s*\{[^}]*return false;", "open failure")
        # open("w") truncates first: a later short write must be caught by reading the size back
        self.assertIn("file.position()", body)
        self.assertRegex(body, r"File check = LittleFS\.open\(SETTINGS_FILE, \"r\"\);")
        self.assertRegex(body, r"if \(!tailWritten \|\| intended == 0 \|\| onFlash != intended\) \{[^}]*return false;",
                         "short write")
        self.assertTrue(body.rstrip("}").rstrip().endswith("return true;"))

    def test_failed_restore_write_stays_dirty(self):
        """The abort path writes onto a half-written partition, where writeSettings is most
        likely to fail; clearing dirty there would bring the defaults back at the reboot."""
        mark = function_body(self.st, r"void settingsMarkDirty\(\)")
        self.assertIn("settingsDirty = true;", mark)
        self.assertRegex(UPD, r"extern void settingsMarkDirty\(\);")

    def test_flush_keeps_dirty_on_failure(self):
        body = function_body(self.st, r"void flushSettings\(\)")
        self.assertRegex(body, r"if \(writeSettings\(false\)\) \{\s*settingsDirty = false;\s*\}",
                         "dirty may only be cleared after a successful write")
        self.assertEqual(body.count("settingsDirty = false;"), 1)

    def test_empty_or_unreadable_file_marks_dirty(self):
        body = function_body(self.st, r"void readSettings\(bool show\)")
        self.assertRegex(body, r"if \(!file\) \{[^}]*settingsDirty = true;\s*return;", "unreadable file")
        self.assertRegex(body, r"if \(file\.size\(\) == 0\) \{[^}]*settingsDirty = true;\s*return;", "empty file")


class TestSatWriteBack(unittest.TestCase):
    SAVERS = {
        "satSavePidState": "/sat_pid_state.json",
        "satSaveEnergyState": "/sat_energy.json",
        "satSaveEstimatedEnergy": "/sat_energy_estimate.json",
    }

    def test_calls_every_saver(self):
        body = function_body(SATCTL, r"\nvoid satPersistAllToFs\(\)")
        for fn in list(self.SAVERS) + ["satHCRSaveState"]:
            if fn == "satSavePidState":
                continue   # gated on a room reading, see test_pid_save_gated_on_room_seen
            self.assertTrue(is_plain_statement(body, fn + "();"), fn + " is not called unconditionally")
        self.assertIn("if (!LittleFSmounted) return;", body)

    def test_pid_save_gated_on_room_seen(self):
        """Sergeant D entry 115 (TASK-1365): a PID state restored at boot, with no room reading
        since, stays on flash; satPersistAllToFs saves it only once a room reading was seen."""
        body = function_body(SATCTL, r"\nvoid satPersistAllToFs\(\)")
        self.assertRegex(body, r"if\s*\(\s*satPidRoomSeen\(\)\s*\)\s*satSavePidState\(\);")
        self.assertFalse(is_plain_statement(body, "satSavePidState();"),
                         "satSavePidState must stay gated on satPidRoomSeen() (entry 115)")

    def test_savers_write_unconditionally(self):
        """The 'due' guards (hourly timer, 0.1 kWh boundary, 5 min PID timer) belong to the
        callers; a guard inside a saver would make the forced write skip."""
        for fn in self.SAVERS:
            body = function_body(SATCTL, r"\n(?:static )?void %s\(\)" % fn)
            ifs = re.findall(r"\bif\s*\(([^)]*)\)", body)
            self.assertEqual([i.strip() for i in ifs], ["!f"], "%s has a guard besides the open check: %s" % (fn, ifs))
            self.assertRegex(body, r'LittleFS\.open\(.*,\s*"w"\)', fn)
        hcr = function_body(SATCYC, r"\nvoid satHCRSaveState\(\)\s*\n(?=\{)")
        self.assertEqual([i.strip() for i in re.findall(r"\bif\s*\(([^)]*)\)", hcr)][:1], ["!f"])

    def test_due_guards_stay_in_the_callers(self):
        caller = function_body(SATCTL, r"static void satPersistEnergyIfDue\(\)")
        self.assertIn("SAT_ENERGY_SAVE_INTERVAL_MS", caller)
        self.assertIn("floorf(", caller)

    def test_file_names(self):
        for name in ("/sat_pid_state.json", "/sat_energy.json", "/sat_energy_estimate.json"):
            self.assertIn('"%s"' % name, SATCTL)
        self.assertIn('"/sat/sat_hcr.json"', SATCYC)

    def test_dhw_total_still_flushed_on_restart(self):
        self.assertIn("flushDHWWaterMeter();", function_body(code(*FWDIR, "helperStuff.ino"), r"void doRestart\(const char\* str\)"))


class TestOtProfiles(unittest.TestCase):
    def test_forced_save_marks_both_files_dirty_first(self):
        body = function_body(OTCORE, r"\nvoid saveOtSupportFilesForced\(\)")
        a, b, c = (body.find(s) for s in ("thermostatFileDirty = true;", "boilerFileDirty     = true;", "saveOtSupportFilesIfDirty();"))
        self.assertTrue(0 <= a < c and 0 <= b < c, "both dirty flags must be set before the save: %s" % ((a, b, c),))
        self.assertTrue(is_plain_statement(body, "saveOtSupportFilesIfDirty();"))

    def test_if_dirty_writes_both_files(self):
        body = function_body(OTCORE, r"\nvoid saveOtSupportFilesIfDirty\(\)")
        self.assertIn("writeOtThermoFile(", body)
        self.assertIn("writeOtBoilerFile(", body)


class TestSatFolder(unittest.TestCase):
    """A fresh image has no /sat folder; a writer or a rename into it has to create it."""

    def test_hcr_saver_creates_the_folder(self):
        body = function_body(SATCYC, r"\nvoid satHCRSaveState\(\)\s*\n(?=\{)")
        self.assertRegex(body, r'LittleFS\.open\(FPSTR\(SAT_HCR_FILE\), "w", true\)')

    def test_migration_creates_the_folder_before_the_rename(self):
        body = function_body(SATCTL, r"static void satMigrateFile\(PGM_P oldPath, PGM_P newPath\)")
        self.assertIn("LittleFS.mkdir(", body)
        self.assertLess(body.index("LittleFS.mkdir("), body.index("LittleFS.rename("))

    def test_no_other_writer_under_sat_without_create(self):
        """Every open for writing of a constant that lives under /sat/ passes create=true."""
        sat_consts = set()
        srcs = {}
        for p in (REPO_ROOT / "src" / "OTGW-firmware").glob("*.ino"):
            srcs[p.name] = strip_comments(p.read_text(encoding="utf-8", errors="replace"))
            sat_consts.update(re.findall(r'(\w+)\[\]\s*PROGMEM\s*=\s*"/sat/', srcs[p.name]))
        self.assertIn("SAT_HCR_FILE", sat_consts)
        for name, text in srcs.items():
            for const in sat_consts:
                for m in re.finditer(r'LittleFS\.open\((?:FPSTR\()?%s\)?,\s*"[wa]\+?"(,\s*true)?\)' % const, text):
                    self.assertTrue(m.group(1), "%s opens %s for writing without create=true" % (name, const))
            for m in re.finditer(r'LittleFS\.open\("/sat/[^"]*",\s*"[wa]\+?"(,\s*true)?\)', text):
                self.assertTrue(m.group(1), name + ": literal /sat/ path opened without create=true")


class TestDocsMatch(unittest.TestCase):
    def test_guide_names_every_kept_file(self):
        guide = read("docs", "guides", "WIFI_FIRMWARE_UPDATE.md")
        section = guide[guide.index("### What a filesystem update keeps and loses"):]
        section = section[:section.index("### Cross-version contract")]
        for name in ("settings.ini", "dallas_labels.ini", "sat_pid_state.json", "sat_energy.json", "sat_energy_estimate.json",
                     "sat_hcr.json", "sat_markers.json", "ot-boiler.json", "ot-thermo.json", "reboot_log.txt",
                     "reboot_count.txt", "dhw_water.json"):
            self.assertIn(name, section, name)
        lost = section[section.index("Lost"):]
        for name in ("bootdetect.log", "topic_cleanup.bin"):
            self.assertIn(name, lost, name)
        for name in ("sat_pid_state.json", "ot-boiler.json", "reboot_log.txt"):
            self.assertNotIn(name, lost, name + " is kept, it must not be listed as lost")

    def test_page_lines(self):
        page = read(*FWDIR, "updateServerHtml.h")
        kept = re.search(r"<li><b>Kept:</b>([^<]*)</li>", page).group(1).lower()
        fresh = re.search(r"<li><b>Starts fresh:</b>([^<]*)</li>", page).group(1).lower()
        for w in ("sat learned values", "opentherm profiles", "reboot log", "sensor names"):
            self.assertIn(w, kept)
            self.assertNotIn(w, fresh)


if __name__ == "__main__":
    unittest.main(verbosity=2)
