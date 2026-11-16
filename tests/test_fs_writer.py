#!/usr/bin/env python3
"""TASK-1989: the SAT state-file writes go through one FIFO to a writer task, off the loop task.

Bench evidence (09, .61, beta.342, 2026-11-16): 40 writes of a 256 B file on the loop task gave a [loop-stall] at every
6th write (250-410 ms); the same 40 writes from a task on the protocol core gave none. fsWriter.ino queues the writes.

WriterLogic compiles the real fsWriter.ino on the host. Stubs: the value-copy queue (a bounded deque), the task shim (the
task body is not run; the stub of platformTaskDelay() plays the writer task: one job per step, held while a filesystem
image is written), LittleFS (a log of operations). Checked: writes before fsWriterBegin() happen on the caller as before;
after it a write is only queued, then written in FIFO order (write / remove / write keeps its order); a full queue waits
for the writer and keeps the order; nothing is queued while LittleFS is unmounted outside an image write; the writer
holds during an image write (the drain times out); the task runs on the protocol core.
Wiring checks the call sites the host cannot run: the three savers submit instead of opening the file, fsWriterBegin()
before initSAT(), the drain in doRestart() before prepareForReboot() and before the filesystem OTA copies the files.
OTGW_SAT_INO points at another SATcontrol.ino for old-vs-fix runs of the wiring checks.
Run: python tests/test_fs_writer.py
"""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_pwm_override as base  # noqa: E402

FW = base.REPO / "src" / "OTGW-firmware"
WRITER = FW / "fsWriter.ino"
SAT = Path(os.environ.get("OTGW_SAT_INO", FW / "SATcontrol.ino"))

STUBS = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdarg>
#include <deque>
#include <string>
#include <vector>
#define PSTR(s) s
static size_t strlcpy(char* d, const char* s, size_t n) { snprintf(d, n, "%s", s); return strlen(s); }
static uint32_t g_ms = 1000;
static uint32_t millis() { return g_ms; }
static void DebugTf(const char* fmt, ...) { char b[256]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof b, fmt, a); va_end(a); printf("LOG %s", b); }
static bool LittleFSmounted = true;
struct { struct { bool bFsImageWriting = false; } flash; } state;
static std::vector<std::string> g_ops;
struct File {
  std::string path; std::string data; bool ok = false;
  explicit operator bool() const { return ok; }
  size_t write(const uint8_t* b, size_t n) { data.append((const char*)b, n); return n; }
  void close() { g_ops.push_back("W " + path + " " + data); }
};
struct FS {
  File open(const char* p, const char* mode) { File f; f.ok = (mode[0] == 'w'); f.path = p; return f; }
  bool remove(const char* p) { g_ops.push_back(std::string("R ") + p); return true; }
} LittleFS;
// value-copy queue of fixed depth
struct Q { std::deque<std::vector<uint8_t>> items; size_t depth, size; };
typedef Q* PlatformQueue;
typedef void* PlatformTask;
#define PLATFORM_QUEUE_WAIT_FOREVER 0xFFFFFFFFUL
enum { PLATFORM_CORE_PROTOCOL = 0, PLATFORM_CORE_APP = 1 };
static PlatformQueue platformQueueCreate(size_t depth, size_t size) { return new Q{{}, depth, size}; }
static bool platformQueueSend(PlatformQueue q, const void* item) {
  if (!q || q->items.size() >= q->depth) return false;
  q->items.emplace_back((const uint8_t*)item, (const uint8_t*)item + q->size); return true;
}
static bool platformQueueReceive(PlatformQueue q, void* item, uint32_t) {
  if (!q || q->items.empty()) return false;
  memcpy(item, q->items.front().data(), q->size); q->items.pop_front(); return true;
}
static int g_taskCore = -1;
static PlatformTask platformTaskCreatePinnedCore(void (*)(void*), const char*, uint32_t, void*, unsigned, int core) { g_taskCore = core; return (PlatformTask)1; }
static void (*g_writerStep)() = nullptr;   // set in main: one step of the writer task
static void platformTaskDelay(uint32_t ms) { g_ms += ms; if (g_writerStep) g_writerStep(); }
"""

MAIN = r"""
static void writerStep() {   // the writer task body for one job (fsWriterTask in fsWriter.ino)
  if (::state.flash.bFsImageWriting || !LittleFSmounted) return;
  FsWriteJob job;
  if (!platformQueueReceive(s_fswQueue, &job, PLATFORM_QUEUE_WAIT_FOREVER)) return;
  fsWriterService(job);
  s_fswDone = s_fswDone + 1;
}
static void dump(const char* tag) {
  printf("%s ops=%zu pending=%u\n", tag, g_ops.size(), (unsigned)fsWriterPending());
  for (auto& o : g_ops) printf("OP %s\n", o.c_str());
  g_ops.clear();
}
int main(int argc, char** argv) {
  std::string c = argv[1];
  g_writerStep = writerStep;
  if (c == "before") { fsWriterSubmit("/a.json", "{1}", 3); dump("before"); return 0; }
  fsWriterBegin();
  printf("core %d\n", g_taskCore);
  if (c == "fifo") {
    fsWriterSubmit("/a.json", "{1}", 3); fsWriterRemove("/b.json"); fsWriterSubmit("/a.json", "{2}", 3);
    dump("queued");
    printf("drain %d\n", (int)fsWriterDrain(1000));
    dump("drained");
  } else if (c == "full") {
    for (int i = 0; i < 8; i++) { char d[8]; snprintf(d, sizeof d, "{%d}", i); fsWriterSubmit("/a.json", d, strlen(d)); }
    fsWriterDrain(1000);
    dump("full");
  } else if (c == "unmounted") {
    LittleFSmounted = false;
    printf("submit %d\n", (int)fsWriterSubmit("/a.json", "{1}", 3));
    dump("unmounted");
  } else if (c == "hold") {
    fsWriterSubmit("/a.json", "{1}", 3);
    ::state.flash.bFsImageWriting = true;
    printf("drain %d\n", (int)fsWriterDrain(300));
    dump("held");
    ::state.flash.bFsImageWriting = false;
    printf("drain %d\n", (int)fsWriterDrain(300));
    dump("released");
  }
  return 0;
}
"""


class WriterLogic(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not WRITER.exists():
            raise AssertionError("fsWriter.ino missing (code before the TASK-1989 fix)")
        cls.tmp = tempfile.TemporaryDirectory()
        src = WRITER.read_text(encoding="utf-8").replace('#include "fsWriter.h"', (FW / "fsWriter.h").read_text(encoding="utf-8"))
        cpp = Path(cls.tmp.name) / "w.cpp"
        cpp.write_text(STUBS + src + MAIN, encoding="utf-8")
        cls.exe = base.compile_cpp(cpp, Path(cls.tmp.name) / "w.exe")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        if not self.exe:
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")

    def run_w(self, case):
        return subprocess.run([str(self.exe), case], capture_output=True, text=True, check=True).stdout.splitlines()

    def test_before_begin_the_caller_writes_as_before(self):
        out = self.run_w("before")
        self.assertIn("before ops=1 pending=0", out)
        self.assertIn("OP W /a.json {1}", out)

    def test_after_begin_writes_are_queued_then_written_in_fifo_order(self):
        out = self.run_w("fifo")
        self.assertIn("core 0", out)                       # the protocol core
        self.assertIn("queued ops=0 pending=3", out)       # nothing on the caller
        self.assertIn("drain 1", out)
        i = out.index("drained ops=3 pending=0")
        self.assertEqual(out[i + 1:i + 4], ["OP W /a.json {1}", "OP R /b.json", "OP W /a.json {2}"])

    def test_a_full_queue_waits_and_keeps_the_order(self):
        out = self.run_w("full")
        i = out.index("full ops=8 pending=0")
        self.assertEqual(out[i + 1:i + 9], ["OP W /a.json {%d}" % k for k in range(8)])

    def test_nothing_is_queued_while_littlefs_is_unmounted(self):
        out = self.run_w("unmounted")
        self.assertIn("submit 0", out)
        self.assertIn("unmounted ops=0 pending=0", out)

    def test_the_writer_holds_during_a_filesystem_image_write(self):
        out = self.run_w("hold")
        self.assertEqual(out[out.index("held ops=0 pending=1") - 1], "drain 0")      # held: the drain times out
        self.assertEqual(out[out.index("released ops=1 pending=0") - 1], "drain 1")  # written once the image is done


class Wiring(unittest.TestCase):
    def setUp(self):
        self.sat = SAT.read_text(encoding="utf-8").replace("\r\n", "\n")
        self.main = (FW / "OTGW-firmware.ino").read_text(encoding="utf-8").replace("\r\n", "\n")
        self.helper = (FW / "helperStuff.ino").read_text(encoding="utf-8").replace("\r\n", "\n")
        self.ota = (FW / "OTGW-ModUpdateServer-esp32.h").read_text(encoding="utf-8").replace("\r\n", "\n")

    def body(self, head):
        a = self.sat.index(head)
        return self.sat[a:self.sat.index("\n}\n", a)]

    def test_the_three_savers_submit_instead_of_writing_on_the_loop(self):
        for head, const in (("void satSavePidState()", "SAT_PID_STATE_FILE"), ("void satSaveEnergyState()", "SAT_ENERGY_FILE"),
                            ("static void satSaveEstimatedEnergy()", "SAT_EST_ENERGY_FILE")):
            b = self.body(head)
            self.assertIn("fsWriterSubmit(%s, buf" % const, b, head)  # a plain const char*: fsWriterSubmit takes no FPSTR()
            self.assertNotIn("LittleFS.open(", b, head)

    def test_no_other_write_or_remove_of_these_files(self):
        for const in ("SAT_PID_STATE_FILE", "SAT_ENERGY_FILE", "SAT_EST_ENERGY_FILE"):
            for bad in ('LittleFS.open(FPSTR(%s), "w")' % const, "LittleFS.remove(FPSTR(%s))" % const):
                self.assertNotIn(bad, self.sat)

    def test_begin_before_initsat(self):
        self.assertLess(self.main.index("fsWriterBegin();"), self.main.index("  initSAT();"))

    def test_restart_drains_before_the_teardown(self):
        r = self.helper[self.helper.index("void doRestart(const char* str) {"):]
        self.assertLess(r.index("fsWriterDrain("), r.index("prepareForReboot();"))

    def test_filesystem_ota_drains_before_it_copies_the_files(self):
        self.assertLess(self.ota.index("fsWriterDrain("), self.ota.index("_keepFiles();"))
        self.assertLess(self.ota.index("fsWriterDrain("), self.ota.index("LittleFS.end();", self.ota.index("_keepFiles();")))


if __name__ == "__main__":
    unittest.main(verbosity=2)
