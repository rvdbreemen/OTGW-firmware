#!/usr/bin/env python3
"""TASK-1911: the SAT calibration markers are served from a RAM copy and written to flash on the loop task.

    python tests/test_sat_markers_ram_copy.py
    OTGW_MARKERS_REV=<sha> python tests/test_sat_markers_ram_copy.py   # the handler of another revision

GET/POST/DELETE /api/v2/sat/markers (handleSAT, restAPI.ino) used to read and write /sat_markers.json on the
async_tcp task. Now they work on the RAM copy of satMarkers.ino under its mutex, and loopSatMarkers() (loop task)
writes the file through a temp file and fsCommitTempFile(). The test compiles the real "// --- satMarkers" block
out of satMarkers.ino and the real markers branch of handleSAT() against an in-memory LittleFS, a mutex stub and
web stubs, and checks, with the two bench lessons of the TASK-1912 labels copy:
  * no file access from any handler call (the old handler: red);
  * boot load, read-your-writes before any flush, ids max+1, delete of first/middle/last/only, 404, 409 at 20;
  * the flush writes temp + rename only when the copy changed; a failed write keeps the old file and the edit
    and waits 30 s; busy lock answers 503;
  * a POST while the flush writes flash is neither blocked nor lost (the flush holds the lock only for the snapshot);
  * an FSexplorer upload/delete of the file is taken over by the loop task; a POST after that request wins.
With OTGW_MARKERS_REV the handler branch comes from that revision (the module stays the working tree's); the old
handler touches the filesystem, so the no-file-access check goes red.
"""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_published_curve import compile_cpp  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
SRC = REPO / "src" / "OTGW-firmware"
REV = os.environ.get("OTGW_MARKERS_REV")


def read_rest():
    if REV is None:
        return (SRC / "restAPI.ino").read_text(encoding="utf-8").replace("\r\n", "\n")
    return subprocess.run(["git", "-C", str(REPO), "show", f"{REV}:src/OTGW-firmware/restAPI.ino"],
                          capture_output=True, text=True, encoding="utf-8", check=True).stdout.replace("\r\n", "\n")


STUBS = r"""
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <ctime>
#include <string>
#include <map>
struct __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
#define PSTR(s) (s)
#define PROGMEM
#define snprintf_P snprintf
#define strcasecmp_P strcmp
#define DebugTf(...) do { } while (0)
#define DebugTln(x) do { } while (0)
#ifndef __GNUC__
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
#define strlcpy_P strlcpy
static uint32_t g_ms = 100000;
static uint32_t millis() { return g_ms; }
static bool LittleFSmounted = true;
static void* platformPsramAllocOnce(size_t) { return nullptr; }   // no PSRAM: the internal fallback

// ---- in-memory LittleFS
static std::map<std::string, std::string> g_fs;
static int g_fsOps = 0;
static void (*g_onWrite)() = nullptr;
static bool g_failOpenW = false, g_failRename = false;
struct File {
  std::string path; bool w = false, ok = false; size_t pos = 0;
  explicit operator bool() const { return ok; }
  size_t size() const { auto it = g_fs.find(path); return it == g_fs.end() ? 0 : it->second.size(); }
  bool available() { return pos < g_fs[path].size(); }
  int read() { g_fsOps++; return pos < g_fs[path].size() ? (unsigned char)g_fs[path][pos++] : -1; }
  size_t read(uint8_t* b, size_t n) { g_fsOps++; const std::string& s = g_fs[path]; size_t c = s.size() - pos < n ? s.size() - pos : n; memcpy(b, s.data() + pos, c); pos += c; return c; }
  size_t write(const uint8_t* b, size_t n) { g_fsOps++; g_fs[path].append((const char*)b, n);
    if (g_onWrite) { void (*f)() = g_onWrite; g_onWrite = nullptr; f(); } return n; }
  size_t print(const char* s) { return write((const uint8_t*)s, strlen(s)); }
  size_t print(char c) { return write((const uint8_t*)&c, 1); }
  void close() { }
};
struct FSStub {
  File open(const char* p, const char* m) { g_fsOps++; File f; f.path = p; f.w = m[0] == 'w';
    if (f.w) { if (g_failOpenW) return f; g_fs[p] = ""; f.ok = true; } else f.ok = g_fs.count(p) > 0; return f; }
  bool exists(const char* p) { g_fsOps++; return g_fs.count(p) > 0; }
  bool rename(const char* a, const char* b) { g_fsOps++; if (g_failRename || !g_fs.count(a)) return false; g_fs[b] = g_fs[a]; g_fs.erase(a); return true; }
  bool remove(const char* p) { g_fsOps++; return g_fs.erase(p) > 0; }
} LittleFS;
static bool fsCommitTempFile(const char* tmp, const char* path, size_t expected) {   // as helperStuff.ino
  const size_t onFlash = g_fs.count(tmp) ? g_fs[tmp].size() : 0;
  if (expected == 0 || onFlash != expected || !LittleFS.rename(tmp, path)) { LittleFS.remove(tmp); return false; }
  return true;
}

// ---- mutex stub: g_busy makes a timed lock fail; a holder makes a timed lock fail too
typedef int* PlatformMutex;
static int g_mutexObj; static bool g_busy = false; static int g_held = 0;
static PlatformMutex platformMutexCreate() { return &g_mutexObj; }
static bool platformMutexLock(PlatformMutex, uint32_t waitMs = 0) { if ((g_busy || g_held > 0) && waitMs != 0) return false; g_held++; return true; }
static void platformMutexUnlock(PlatformMutex) { g_held--; }

// ---- web stubs
enum HTTPMethod { HTTP_GET = 1, HTTP_POST = 2, HTTP_DELETE = 4, HTTP_PUT = 8 };
static int g_code = 0; static std::string g_body, g_req;
static bool hasArgCompat(const __FlashStringHelper*) { return !g_req.empty(); }
static const char* bodyCompat() { return g_req.c_str(); }
static void webSend(int c, const __FlashStringHelper*, const char* b) { g_code = c; g_body = b; }
static void webSend(int c, const __FlashStringHelper*, const __FlashStringHelper* b) { g_code = c; g_body = (const char*)b; }
static void webSendFile(const char* p, const __FlashStringHelper*, bool) { g_fsOps++; g_code = 200; g_body = g_fs[p]; }
static void webPushHeader(const __FlashStringHelper*, const __FlashStringHelper*) { }
static void sendApiError(int c, const __FlashStringHelper* m) { g_code = c; g_body = (const char*)m; }
static void sendApiMethodNotAllowed(const __FlashStringHelper*) { g_code = 405; }
static void extractJsonField(const char* body, const __FlashStringHelper* key, char* out, size_t n) {
  out[0] = 0; std::string k = std::string("\"") + (const char*)key + "\":";
  const char* p = strstr(body, k.c_str()); if (!p) return; p += k.size(); while (*p == ' ') p++;
  bool q = *p == '"'; if (q) p++; size_t i = 0;
  while (*p && i + 1 < n && (q ? *p != '"' : (*p != ',' && *p != '}'))) out[i++] = *p++;
  out[i] = 0;
}
#define API_WORD_LEN 32
"""

MAIN = r"""
static void handleMarkers(const char words[][API_WORD_LEN], uint8_t wc, HTTPMethod method) {
  const char* sub = "markers";
  if (0) { }
@@BRANCH@@
}
static const char W[6][API_WORD_LEN] = {"api", "v2", "sat", "markers", "", ""};
static int call(HTTPMethod m, const char* body = "", int delId = 0) {
  char w[6][API_WORD_LEN]; memcpy(w, W, sizeof(w));
  uint8_t wc = 4;
  if (delId) { snprintf(w[5], API_WORD_LEN, "%d", delId); wc = 6; }
  g_code = 0; g_req = body;
  const int before = g_fsOps;
  handleMarkers(w, wc, m);
  return g_fsOps - before;
}
static int g_fsDuringHandlers = 0;
static int H(HTTPMethod m, const char* body = "", int delId = 0) { int n = call(m, body, delId); g_fsDuringHandlers += n; return g_code; }
static int g_duringCode = 0;
static void postDuringWrite() { g_duringCode = H(HTTP_POST, "{\"outside_temp\":3.0,\"flow_temp\":50.0,\"label\":\"during\"}"); }
#define P(k, v) printf("%s=%s\n", k, std::string(v).c_str())
#define N(k, v) printf("%s=%d\n", k, (int)(v))
int main() {
  const char* M1 = "{\"id\":1,\"outside_temp\":5.0,\"flow_temp\":55.0,\"added\":1,\"label\":\"a\"}";
  const char* M2 = "{\"id\":4,\"outside_temp\":0.0,\"flow_temp\":60.0,\"added\":2,\"label\":\"b\"}";
  g_fs["/sat_markers.json"] = std::string("[") + M1 + "," + M2 + "]";
  loadSatMarkers();
  N("boot_get", H(HTTP_GET)); P("boot_body", g_body);
  N("post", H(HTTP_POST, "{\"outside_temp\":-2.5,\"flow_temp\":62.0,\"label\":\"x{y}z\"}")); P("post_body", g_body);
  H(HTTP_GET); P("ryw_body", g_body); P("file_before_loop", g_fs["/sat_markers.json"]);
  loopSatMarkers(); P("file_after_loop", g_fs["/sat_markers.json"]);
  int ops = g_fsOps; loopSatMarkers(); N("clean_loop_ops", g_fsOps - ops);
  N("del_mid", H(HTTP_DELETE, "", 4)); H(HTTP_GET); P("after_del_mid", g_body);
  N("del_last", H(HTTP_DELETE, "", 5)); H(HTTP_GET); P("after_del_last", g_body);
  N("del_only", H(HTTP_DELETE, "", 1)); H(HTTP_GET); P("after_del_only", g_body);
  N("del_404", H(HTTP_DELETE, "", 9));
  N("post_bad", H(HTTP_POST, "{\"outside_temp\":40,\"flow_temp\":55}"));
  int full = 0; for (int i = 0; i < 21; i++) full = H(HTTP_POST, "{\"outside_temp\":1.0,\"flow_temp\":40.0,\"label\":\"0123456789012345678901234567890\"}");
  N("post_21st", full);
  loopSatMarkers();
  // busy lock
  g_busy = true; N("busy_get", H(HTTP_GET)); N("busy_post", H(HTTP_POST, "{\"outside_temp\":1,\"flow_temp\":40}")); g_busy = false;
  // a failed write keeps the old file and the edit, then waits 30 s
  std::string before = g_fs["/sat_markers.json"];
  H(HTTP_DELETE, "", 2);
  g_failRename = true; loopSatMarkers(); g_failRename = false;
  N("fail_keeps_file", g_fs["/sat_markers.json"] == before); N("fail_no_tmp", g_fs.count("/sat_markers.tmp") == 0);
  ops = g_fsOps; g_ms += 10000; loopSatMarkers(); N("retry_waits_ops", g_fsOps - ops);
  g_ms += 21000; loopSatMarkers(); N("retry_written", g_fs["/sat_markers.json"].find("\"id\":2,") == std::string::npos);
  // a POST while the flush writes flash: not blocked, not lost
  H(HTTP_DELETE, "", 3);
  g_onWrite = postDuringWrite; loopSatMarkers();
  N("during_code", g_duringCode);
  N("during_in_copy", (H(HTTP_GET), g_body.find("during") != std::string::npos));
  N("during_file_before", g_fs["/sat_markers.json"].find("during") != std::string::npos);
  loopSatMarkers(); N("during_file_after", g_fs["/sat_markers.json"].find("during") != std::string::npos);
  // FSexplorer upload: reload on the loop task; a POST after the request wins
  g_fs["/sat_markers.json"] = std::string("[") + M1 + "]";
  satMarkersFileChanged("/sat_markers.json"); satMarkersFileChanged("/other.json");
  loopSatMarkers(); H(HTTP_GET); P("after_upload", g_body);
  g_fs["/sat_markers.json"] = std::string("[") + M2 + "]";
  satMarkersFileChanged("sat_markers.json");
  H(HTTP_POST, "{\"outside_temp\":2.0,\"flow_temp\":45.0,\"label\":\"late\"}");
  loopSatMarkers(); H(HTTP_GET); N("late_post_wins", g_body.find("late") != std::string::npos && g_body.find("\"id\":1,") != std::string::npos);
  N("handler_fs_ops", g_fsDuringHandlers);
  return 0;
}
"""


class SatMarkersRamCopy(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        mod = (SRC / "satMarkers.ino").read_text(encoding="utf-8").replace("\r\n", "\n")
        a, b = mod.find("// --- satMarkers begin"), mod.find("// --- satMarkers end")
        if a < 0 or b < 0:
            raise AssertionError("satMarkers block not found in satMarkers.ino")
        rest = read_rest()
        h = rest.find('  else if (strcasecmp_P(sub, PSTR("markers")) == 0) {')
        e = rest.find("  // --- TASK-587: DS18B20 sensor-to-SAT-area mapping ---", h)
        if h < 0 or e < 0:
            raise AssertionError("markers branch of handleSAT not found")
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(STUBS + mod[a:b] + MAIN.replace("@@BRANCH@@", rest[h:e]), encoding="utf-8")
        exe = compile_cpp(cpp, Path(cls.tmp.name) / "t.exe")
        if exe is None:
            raise unittest.SkipTest("no host C++ compiler")
        out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
        cls.r = dict(line.split("=", 1) for line in out.splitlines() if "=" in line)
        print(out)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_no_file_access_from_the_handler(self):
        self.assertEqual(self.r["handler_fs_ops"], "0", "the handler touched LittleFS on async_tcp")

    def test_boot_load_and_read_your_writes(self):
        r = self.r
        self.assertEqual(r["boot_get"], "200")
        self.assertTrue(r["boot_body"].startswith('[{"id":1,'))
        self.assertEqual((r["post"], r["post_body"]), ("201", '{"status":"ok","id":5}'))
        self.assertIn('"id":5,"outside_temp":-2.5,"flow_temp":62.0', r["ryw_body"])
        self.assertIn('"label":"x_y_z"', r["ryw_body"], "braces in a label are replaced")
        self.assertNotIn('"id":5', r["file_before_loop"])
        self.assertEqual(r["file_after_loop"], r["ryw_body"])
        self.assertEqual(r["clean_loop_ops"], "0")

    def test_delete_keeps_valid_json(self):
        r = self.r
        self.assertEqual(r["del_mid"], "200")
        self.assertTrue(r["after_del_mid"].startswith('[{"id":1,') and '"id":4' not in r["after_del_mid"])
        self.assertEqual(r["del_last"], "200")
        self.assertTrue(r["after_del_last"].startswith('[{"id":1,') and r["after_del_last"].endswith('"label":"a"}]'))
        self.assertEqual((r["del_only"], r["after_del_only"]), ("200", "[]"))
        self.assertEqual(r["del_404"], "404")

    def test_validation_and_limit(self):
        self.assertEqual(self.r["post_bad"], "400")
        self.assertEqual(self.r["post_21st"], "409")

    def test_busy_lock_answers_503(self):
        self.assertEqual((self.r["busy_get"], self.r["busy_post"]), ("503", "503"))

    def test_failed_write_keeps_file_and_retries_after_30_s(self):
        r = self.r
        self.assertEqual((r["fail_keeps_file"], r["fail_no_tmp"]), ("1", "1"))
        self.assertEqual(r["retry_waits_ops"], "0")
        self.assertEqual(r["retry_written"], "1")

    def test_post_during_flash_write_is_not_blocked_or_lost(self):
        r = self.r
        self.assertEqual(r["during_code"], "201")
        self.assertEqual(r["during_in_copy"], "1")
        self.assertEqual(r["during_file_before"], "0")
        self.assertEqual(r["during_file_after"], "1")

    def test_fsexplorer_change_reloads_and_a_later_post_wins(self):
        self.assertTrue(self.r["after_upload"].startswith('[{"id":1,') and '"id":4' not in self.r["after_upload"])
        self.assertEqual(self.r["late_post_wins"], "1")


if __name__ == "__main__":
    unittest.main(verbosity=2)
