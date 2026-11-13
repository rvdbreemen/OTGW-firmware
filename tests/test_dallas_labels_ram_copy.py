#!/usr/bin/env python3
"""TASK-1912: the Dallas sensor labels are served from a RAM copy and written to flash on the loop task.

GET /api/v2/sensors/labels answers from the copy, POST/PUT replaces it, and loopDallasLabels() (loop task) writes
/dallas_labels.ini through a temp file and a rename. The web UI saves a label as read, merge, write
(v2.js saveDallasLabel), so a GET after a POST must return that POST also while the file is still the old one.

The test compiles the real "// --- dallasLabels" block out of sensors_ext.ino and the real getDallasLabels() and
updateAllDallasLabels() out of restAPI.ino against an in-memory LittleFS, a mutex stub and a webSend() stub.

  * read-your-writes before any flush; no file access from either handler;
  * the flush writes temp + rename, only when the copy changed; a failed write keeps the old file and the edit, and
    waits 30 s before it tries again;
  * boot load: the file, or "{}" when it is missing, empty, too large or not an object;
  * busy lock: both handlers answer 503 (the UI's fetchWithRetry retries on 503);
  * a POST while the flush writes flash is neither blocked nor lost: the flush holds the lock only for the snapshot
    (09's bench on 962a3c7: GET/POST got 503 for up to ~0.9 s after a save, and without a retry the second of two
    quick renames was lost). The fake filesystem runs that POST from inside the flash write; the lock stub refuses a
    timed lock while another holder has it, as a real 100 ms wait does against a flash write;
  * FSexplorer upload or delete of the file (09's bench on 962a3c7: the copy kept the old names and the next save
    wrote them back over the user's file): dallasLabelsFileChanged() asks the loop task to reload, the last action
    wins (a pending older edit is dropped, a POST after the request is kept), other paths are ignored;
  * the lost-update guard: a mutant whose GET reads the file again (the GET from before TASK-1912) with the new POST
    returns the old labels after a POST, the window the RAM copy closes.

OTGW_RESTAPI_INO / OTGW_SENSORS_INO / OTGW_FSEXPLORER_INO point at other copies.

Run: python tests/test_dallas_labels_ram_copy.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SRC = REPO / "src" / "OTGW-firmware"
REST_INO = Path(os.environ.get("OTGW_RESTAPI_INO", SRC / "restAPI.ino"))
SENS_INO = Path(os.environ.get("OTGW_SENSORS_INO", SRC / "sensors_ext.ino"))
FSX_INO = Path(os.environ.get("OTGW_FSEXPLORER_INO", SRC / "FSexplorer.ino"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402
from test_rest_no_littlefs_on_async_tcp import blank_comments_and_strings, function_body  # noqa: E402

# The GET handler from before TASK-1912: it reads the file again (exists + stream).
OLD_GET = r"""void getDallasLabels() {
  if (!LittleFS.exists(F("/dallas_labels.ini"))) {
    webSend(200, F("application/json"), F("{}"));
    return;
  }
  webSendFile("/dallas_labels.ini", F("application/json"), /*gzip=*/false);
}"""

STUBS = r"""
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <map>
typedef unsigned char uint8_t;
struct __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
#define PSTR(s) (s)
#define FPSTR(s) (s)
#define DebugTf(...) do { } while (0)
#define DebugTln(x) do { } while (0)
#ifndef __GNUC__
// single-threaded host stand-ins for the GCC builtins (MSVC)
#define __atomic_load_n(p, o) (*(p))
#define __atomic_store_n(p, v, o) (*(p) = (v))
template <typename T> static T atomicExchangeStub(T* p, T v) { T o = *p; *p = v; return o; }
#define __atomic_exchange_n(p, v, o) atomicExchangeStub((p), (v))
#define __ATOMIC_RELAXED 0
#define __ATOMIC_ACQUIRE 0
#define __ATOMIC_RELEASE 0
#define __ATOMIC_ACQ_REL 0
#endif
static size_t strlcpy(char* d, const char* s, size_t n) { size_t l = strlen(s); if (n) { size_t c = l < n - 1 ? l : n - 1; memcpy(d, s, c); d[c] = 0; } return l; }
static uint32_t g_ms = 100000;
static uint32_t millis() { return g_ms; }
static bool LittleFSmounted = true;

// ---- in-memory LittleFS
static std::map<std::string, std::string> g_fs;
static int g_fsOps = 0;            // every call into the fake filesystem
static void (*g_onWrite)() = nullptr;   // runs once inside the next File::write: a REST call during the flash write
static bool g_failOpenW = false, g_failRename = false;
struct File {
  std::string path; bool w = false, ok = false; size_t pos = 0;
  explicit operator bool() const { return ok; }
  size_t size() const { auto it = g_fs.find(path); return it == g_fs.end() ? 0 : it->second.size(); }
  size_t read(uint8_t* b, size_t n) { g_fsOps++; const std::string& s = g_fs[path]; size_t c = s.size() - pos < n ? s.size() - pos : n; memcpy(b, s.data() + pos, c); pos += c; return c; }
  size_t write(const uint8_t* b, size_t n) { g_fsOps++; g_fs[path].append((const char*)b, n);
    if (g_onWrite) { void (*f)() = g_onWrite; g_onWrite = nullptr; f(); } return n; }
  void close() { }
};
struct FSStub {
  File open(const char* p, const char* m) { g_fsOps++; File f; f.path = p; f.w = m[0] == 'w';
    if (f.w) { if (g_failOpenW) return f; g_fs[p] = ""; f.ok = true; } else f.ok = g_fs.count(p) > 0; return f; }
  File open(const __FlashStringHelper* p, const char* m) { return open((const char*)p, m); }
  bool exists(const char* p) { g_fsOps++; return g_fs.count(p) > 0; }
  bool exists(const __FlashStringHelper* p) { return exists((const char*)p); }
  bool rename(const char* a, const char* b) { g_fsOps++; if (g_failRename || !g_fs.count(a)) return false; g_fs[b] = g_fs[a]; g_fs.erase(a); return true; }
  bool remove(const char* p) { g_fsOps++; return g_fs.erase(p) > 0; }
} LittleFS;

// ---- mutex stub: g_busy makes a timed lock fail
typedef int* PlatformMutex;
static int g_mutexObj; static bool g_busy = false; static int g_held = 0;
static PlatformMutex platformMutexCreate() { return &g_mutexObj; }
static bool platformMutexLock(PlatformMutex, uint32_t waitMs = 0) { if ((g_busy || g_held > 0) && waitMs != 0) return false; g_held++; return true; }
static void platformMutexUnlock(PlatformMutex) { g_held--; }

// ---- web stubs
static int g_code = 0; static std::string g_body; static std::string g_req;
static const char* bodyCompat() { return g_req.c_str(); }
static void webSend(int c, const __FlashStringHelper*, const char* b) { g_code = c; g_body = b; }
static void webSend(int c, const __FlashStringHelper*, const __FlashStringHelper* b) { g_code = c; g_body = (const char*)b; }
static void webSendP(int c, const char*, const char* b) { g_code = c; g_body = b; }
static void webSendFile(const char* p, const __FlashStringHelper*, bool) { g_fsOps++; g_code = 200; g_body = g_fs[p]; }
static void sendApiError(int c, const __FlashStringHelper* m) { g_code = c; g_body = (const char*)m; }
"""

MAIN = r"""
static void get()  { g_code = 0; getDallasLabels(); }
static void post(const char* b) { g_code = 0; g_req = b; updateAllDallasLabels(); }
static int g_duringCode = 0;
static void postDuringWrite() { post("{\"28AA\":\"F\"}"); g_duringCode = g_code; }
#define P(k, v) printf("%s=%s\n", k, v)
#define N(k, v) printf("%s=%d\n", k, (int)(v))
int main() {
  // boot: the file holds A
  g_fs["/dallas_labels.ini"] = "{\"28AA\":\"A\"}";
  loadDallasLabels();
  int ops = g_fsOps;
  get(); P("boot_get", g_body.c_str()); N("boot_get_code", g_code);
  // POST B, GET before any loop pass: must be B, the file still A, no file access by either handler
  post("{\"28AA\":\"B\"}"); N("post_code", g_code); P("post_body", g_body.c_str());
  get(); P("get_after_post", g_body.c_str());
  N("handler_fs_ops", g_fsOps - ops);
  P("file_before_flush", g_fs["/dallas_labels.ini"].c_str());
  // the loop pass writes B through the temp file
  loopDallasLabels();
  P("file_after_flush", g_fs["/dallas_labels.ini"].c_str());
  N("tmp_left", g_fs.count("/dallas_labels.tmp"));
  ops = g_fsOps; loopDallasLabels(); N("clean_loop_fs_ops", g_fsOps - ops);
  // a failed rename keeps the old file and the edit; retry after 30 s
  post("{\"28AA\":\"C\"}"); g_failRename = true; loopDallasLabels();
  P("file_after_failed", g_fs["/dallas_labels.ini"].c_str()); N("tmp_after_failed", g_fs.count("/dallas_labels.tmp"));
  get(); P("get_after_failed", g_body.c_str());
  g_failRename = false; ops = g_fsOps; g_ms += 10000; loopDallasLabels(); N("retry_inside_30s_ops", g_fsOps - ops);
  g_ms += 21000; loopDallasLabels(); P("file_after_retry", g_fs["/dallas_labels.ini"].c_str());
  // busy lock: 503 from both handlers, the copy unchanged
  g_busy = true; get(); N("busy_get", g_code); post("{\"28AA\":\"D\"}"); N("busy_post", g_code); g_busy = false;
  get(); P("after_busy", g_body.c_str());
  // invalid body: 400, copy unchanged
  post("not json"); N("bad_post", g_code); get(); P("after_bad", g_body.c_str());
  // boot load variants
  g_fs.erase("/dallas_labels.ini"); loadDallasLabels(); get(); P("load_missing", g_body.c_str());
  g_fs["/dallas_labels.ini"] = "garbage"; loadDallasLabels(); get(); P("load_garbage", g_body.c_str());
  g_fs["/dallas_labels.ini"] = std::string(1, '{') + std::string(3000, ' ') + "}"; loadDallasLabels(); get(); P("load_oversize", g_body.c_str());
  // a POST while the flush writes E to flash: answered at once, and written by the next pass
  g_fs["/dallas_labels.ini"] = "{\"28AA\":\"A\"}"; loadDallasLabels();
  post("{\"28AA\":\"E\"}"); g_onWrite = postDuringWrite; loopDallasLabels();
  N("during_flush_code", g_duringCode);
  P("during_file_first", g_fs["/dallas_labels.ini"].c_str());
  get(); P("during_get", g_body.c_str());
  loopDallasLabels(); P("during_file_second", g_fs["/dallas_labels.ini"].c_str());
  ops = g_fsOps; loopDallasLabels(); N("during_clean_after", g_fsOps - ops);
  // FSexplorer: an upload replaces the file behind the copy; the next loop pass takes it over and writes nothing
  post("{\"28AA\":\"P\"}");                                 // a pending edit, older than the upload
  g_fs["/dallas_labels.ini"] = "{\"28AA\":\"U\"}"; dallasLabelsFileChanged("/dallas_labels.ini");
  ops = g_fsOps; loopDallasLabels(); get(); P("upload_get", g_body.c_str());
  P("upload_file", g_fs["/dallas_labels.ini"].c_str());
  ops = g_fsOps; loopDallasLabels(); N("upload_clean_after", g_fsOps - ops);
  // a delete (FSexplorer passes the name without the slash too): the copy becomes {}
  g_fs.erase("/dallas_labels.ini"); dallasLabelsFileChanged("dallas_labels.ini"); loopDallasLabels();
  get(); P("delete_get", g_body.c_str()); N("delete_file_back", g_fs.count("/dallas_labels.ini"));
  // a POST after the upload request wins over the uploaded file
  g_fs["/dallas_labels.ini"] = "{\"28AA\":\"U2\"}"; dallasLabelsFileChanged("/dallas_labels.ini");
  post("{\"28AA\":\"Q\"}"); loopDallasLabels();
  get(); P("later_post_get", g_body.c_str()); P("later_post_file", g_fs["/dallas_labels.ini"].c_str());
  // another file: no reload
  g_fs["/dallas_labels.ini"] = "{\"28AA\":\"Z\"}"; dallasLabelsFileChanged("/settings.ini"); loopDallasLabels();
  get(); P("other_path_get", g_body.c_str());
  N("mutex_balanced", g_held == 0);
  return 0;
}
"""


def read(p: Path) -> str:
    return p.read_text(encoding="utf-8").replace("\r\n", "\n")


def commit_helper() -> str:
    # TASK-1913's real fsCommitTempFile() out of helperStuff.ino: the flush commits its temp file through it
    m = re.search(r"// --- fsCommitTempFile start\n(.*?)// --- fsCommitTempFile end", read(SRC / "helperStuff.ino"), re.S)
    return m.group(1) if m else ""


def block(src: str) -> str:
    m = re.search(r"// --- dallasLabels begin\n(.*?)// --- dallasLabels end", src, re.S)
    if not m:
        raise AssertionError("dallasLabels block not found in " + str(SENS_INO))
    return m.group(1)


def build_and_run(workdir: Path, get_src: str):
    rest = read(REST_INO)
    cpp = workdir / "t.cpp"
    cpp.write_text(STUBS + commit_helper() + "\n" + block(read(SENS_INO)) + "\n" + get_src + "\n" +
                   function_body(rest, "updateAllDallasLabels").join(["void updateAllDallasLabels() ", "\n"]) +
                   MAIN, encoding="utf-8")
    exe = compile_cpp(cpp, workdir / "t.exe")
    if exe is None:
        return None
    out = subprocess.run([str(exe)], capture_output=True, text=True, check=True, timeout=60).stdout
    return dict(line.split("=", 1) for line in out.splitlines() if "=" in line)


class RamCopy(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._dir = tempfile.TemporaryDirectory()
        rest = read(REST_INO)
        get_src = "void getDallasLabels() " + function_body(rest, "getDallasLabels")
        cls.o = build_and_run(Path(cls._dir.name), get_src)
        if cls.o is None:
            raise unittest.SkipTest("no host C++ compiler")
        mdir = Path(cls._dir.name) / "mutant"; mdir.mkdir()
        cls.m = build_and_run(mdir, OLD_GET)

    @classmethod
    def tearDownClass(cls):
        cls._dir.cleanup()

    def test_read_your_writes_before_the_flush(self):
        o = self.o
        self.assertEqual(o["boot_get"], '{"28AA":"A"}')
        self.assertEqual(o["post_code"], "200")
        self.assertEqual(o["post_body"], '{"success":true}')
        self.assertEqual(o["get_after_post"], '{"28AA":"B"}')
        self.assertEqual(o["file_before_flush"], '{"28AA":"A"}')

    def test_handlers_touch_no_file(self):
        self.assertEqual(self.o["handler_fs_ops"], "0")

    def test_loop_flush_temp_and_rename_only_when_changed(self):
        o = self.o
        self.assertEqual(o["file_after_flush"], '{"28AA":"B"}')
        self.assertEqual(o["tmp_left"], "0")
        self.assertEqual(o["clean_loop_fs_ops"], "0")

    def test_failed_write_keeps_old_file_and_edit_then_retries_after_30s(self):
        o = self.o
        self.assertEqual(o["file_after_failed"], '{"28AA":"B"}')
        self.assertEqual(o["tmp_after_failed"], "0")
        self.assertEqual(o["get_after_failed"], '{"28AA":"C"}')
        self.assertEqual(o["retry_inside_30s_ops"], "0")
        self.assertEqual(o["file_after_retry"], '{"28AA":"C"}')

    def test_busy_and_invalid_leave_the_copy(self):
        o = self.o
        self.assertEqual(o["busy_get"], "503")
        self.assertEqual(o["busy_post"], "503")
        self.assertEqual(o["after_busy"], '{"28AA":"C"}')
        self.assertEqual(o["bad_post"], "400")
        self.assertEqual(o["after_bad"], '{"28AA":"C"}')

    def test_post_during_the_flash_write_is_not_blocked_and_not_lost(self):
        o = self.o
        self.assertEqual(o["during_flush_code"], "200", "a POST during the flash write got " + o["during_flush_code"])
        self.assertEqual(o["during_file_first"], '{"28AA":"E"}')       # the pass wrote its snapshot
        self.assertEqual(o["during_get"], '{"28AA":"F"}')              # the copy holds the later POST
        self.assertEqual(o["during_file_second"], '{"28AA":"F"}')      # still dirty, so the next pass writes it
        self.assertEqual(o["during_clean_after"], "0")

    def test_fsexplorer_upload_and_delete_reload_the_copy(self):
        o = self.o
        self.assertEqual(o["upload_get"], '{"28AA":"U"}', "the copy did not follow an uploaded file")
        self.assertEqual(o["upload_file"], '{"28AA":"U"}', "an older pending edit was written over the upload")
        self.assertEqual(o["upload_clean_after"], "0")
        self.assertEqual(o["delete_get"], "{}")
        self.assertEqual(o["delete_file_back"], "0")
        self.assertEqual(o["later_post_get"], '{"28AA":"Q"}')
        self.assertEqual(o["later_post_file"], '{"28AA":"Q"}')
        self.assertEqual(o["other_path_get"], '{"28AA":"Q"}')

    def test_boot_load_falls_back_to_empty_object(self):
        o = self.o
        self.assertEqual(o["load_missing"], "{}")
        self.assertEqual(o["load_garbage"], "{}")
        self.assertEqual(o["load_oversize"], "{}")
        self.assertEqual(o["mutex_balanced"], "1")

    def test_lost_update_window_without_the_ram_copy(self):
        # the GET from before TASK-1912 with the new POST: the read after a POST returns the old labels
        if self.m is None:
            self.skipTest("mutant did not build")
        self.assertEqual(self.m["get_after_post"], '{"28AA":"A"}', self.m)
        self.assertNotEqual(self.m["handler_fs_ops"], "0")


class Wiring(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sens = blank_comments_and_strings(read(SENS_INO))
        cls.helper = blank_comments_and_strings(read(SRC / "helperStuff.ino"))
        cls.main = blank_comments_and_strings(read(SRC / "OTGW-firmware.ino"))

    def test_defaults_writer_flushes_then_reloads_under_the_lock(self):
        body = function_body(self.sens, "ensureSensorDefaultLabels")
        self.assertRegex(body, r"platformMutexLock\(s_dlMutex\);\s*if \(flushDallasLabelsLocked\(\)\) \{\s*"
                               r"ensureSensorDefaultLabelsFile\(\);\s*loadDallasLabelsLocked\(\);\s*\}\s*"
                               r"platformMutexUnlock\(s_dlMutex\);")

    def test_fsexplorer_upload_and_both_deletes_report_the_change(self):
        fx = blank_comments_and_strings(read(FSX_INO))
        self.assertRegex(fx, r"LittleFS\.remove\(deletePath\)\) \{\s*dallasLabelsFileChanged\(deletePath\);")
        self.assertRegex(fx, r'LittleFS\.remove\(argCompat\("\s*"\)\);\s*dallasLabelsFileChanged\(argCompat\("\s*"\)\);')
        self.assertRegex(fx, r"dallasLabelsFileChanged\(request->_tempFile\.path\(\)\);\s*request->_tempFile\.close\(\);")

    def test_loop_flushes_restart_flushes_boot_loads_before_the_web_server(self):
        self.assertIn("loopDallasLabels();", function_body(self.helper, "loopLittleFSStatus"))
        self.assertRegex(self.helper, r"flushSettings\(\);\s*flushDallasLabels\(\);")
        a, b = self.main.find("loadDallasLabels();"), self.main.find("startWebserver();")
        self.assertTrue(0 <= a < b, "loadDallasLabels() must run before startWebserver()")


if __name__ == "__main__":
    unittest.main(verbosity=2)
