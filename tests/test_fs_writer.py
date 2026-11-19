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
WRITER = Path(os.environ.get("OTGW_FSW_INO", FW / "fsWriter.ino"))   # TASK-2007: another copy for old-vs-fix runs
HEADER = Path(os.environ.get("OTGW_FSW_H", FW / "fsWriter.h"))
SAT = Path(os.environ.get("OTGW_SAT_INO", FW / "SATcontrol.ino"))

STUBS = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdarg>
#include <deque>
#include <map>
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
// TASK-2007: the file contents, and whether LittleFS.end() has run (a write then lands on a partition the image
// write is about to replace: counted as a violation).
static std::map<std::string, std::string> g_files;
static bool g_fsEnded = false;
static int  g_afterEnd = 0;
static bool g_openFail = false, g_shortWrite = false;   // TASK-2020: an open that fails, a write that is cut
static bool g_commitFail = false;   // TASK-2020 F3: close() does not commit (LittleFS keeps the old copy)
struct File {
  std::string path; std::string data; bool ok = false; bool rd = false; size_t pos = 0;
  size_t size() const { return data.size(); }
  size_t read(uint8_t* b, size_t n) { if (n > data.size() - pos) n = data.size() - pos; memcpy(b, data.data() + pos, n); pos += n; return n; }
  explicit operator bool() const { return ok; }
  size_t write(const uint8_t* b, size_t n) {   // TASK-2020: g_shortWrite takes only half
    if (g_shortWrite) n /= 2;
    data.append((const char*)b, n); return n; }
  void close() {
    if (rd) return;   // a read-back (TASK-2020)
    g_ops.push_back("W " + path + " " + data);
    if (g_fsEnded) g_afterEnd++; else if (!g_commitFail) g_files[path] = data; }
};
struct FS {
  File open(const char* p, const char* mode) {
    File f; f.path = p;
    if (mode[0] == 'r') { auto it = g_files.find(p); f.rd = true; f.ok = (it != g_files.end()); if (f.ok) f.data = it->second; return f; }
    f.ok = (mode[0] == 'w') && !g_openFail; return f; }
  bool remove(const char* p) { g_ops.push_back(std::string("R ") + p); if (g_fsEnded) g_afterEnd++; else g_files.erase(p); return true; }
} LittleFS;
// value-copy queue of fixed depth
struct Q { std::deque<std::vector<uint8_t>> items; size_t depth, size; };
typedef Q* PlatformQueue;
typedef void* PlatformTask;
#define PLATFORM_QUEUE_WAIT_FOREVER 0xFFFFFFFFUL
enum { PLATFORM_CORE_PROTOCOL = 0, PLATFORM_CORE_APP = 1 };
static PlatformQueue platformQueueCreate(size_t depth, size_t size) { return new Q{{}, depth, size}; }
static void (*g_onSend)() = nullptr;   // TASK-2007 F2: runs inside the send, as another task reading at that moment
static bool platformQueueSend(PlatformQueue q, const void* item) {
  if (g_onSend) g_onSend();
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
// TASK-1995: one pending write probe or mount check (helperStuff.ino, tested in test_health_no_fs_flash_ops.py). The
// stub serves a raised request and, like a good probe, mounts LittleFS again.
static int  g_healthReq = 0, g_healthRuns = 0;
static bool fsHealthService() { if (!g_healthReq) return false; g_healthReq = 0; g_healthRuns++; LittleFSmounted = true; return true; }
"""

MAIN = r"""
#ifdef HAS_STEP
static void writerStep() { fsWriterStep(); }   // TASK-2007: the real pass of the writer task
#else
static void writerStep() {   // code before TASK-2007: the writer task body for one job (fsWriterTask)
  if (::state.flash.bFsImageWriting || !LittleFSmounted) return;
  FsWriteJob job;
  if (!platformQueueReceive(s_fswQueue, &job, PLATFORM_QUEUE_WAIT_FOREVER)) return;
  fsWriterService(job);
  s_fswDone = s_fswDone + 1;
}
#endif
// The start of a filesystem OTA as OTGW-ModUpdateServer-esp32.h runs it (the copy of the files is _keepFiles()).
static std::map<std::string, std::string> g_kept;
static void otaStart() {
#ifdef HAS_STEP
  fsWriterPauseAndFlush(2000);
#else
  fsWriterDrain(2000);
#endif
  g_kept = g_files;                       // _keepFiles()
  g_onSend = nullptr;
}
static void otaAfterKeep() {
  g_fsEnded = true;                       // LittleFS.end(); LittleFSmounted stays true
  ::state.flash.bFsImageWriting = true;
#ifdef HAS_STEP
  fsWriterResume();
#endif
}
// The end of a filesystem OTA in the order OTGW-ModUpdateServer-esp32.h runs it: _remountAfterFsWrite() mounts the
// new image and clears bFsImageWriting, THEN _restoreRuntimeFiles() writes the heap copies back. The writer task
// takes a pass in between (bench 09, .63, beta.364: remount 09:50:38.338, restore .623).
static void otaFinish() {
  g_files.clear(); g_fsEnded = false;     // the new image lacks the state files
#ifdef HAS_PAUSE
  fsWriterPause();                         // TASK-2007: held until the copies are back
#endif
  ::state.flash.bFsImageWriting = false;  // _remountAfterFsWrite()
  writerStep();                            // the writer's pass between the remount and the restore
  for (auto& kv : g_kept) g_files[kv.first] = kv.second;   // _restoreRuntimeFiles(): _restoreKeptFiles()
#ifdef HAS_PAUSE
  fsWriterResume();
#endif
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
  } else if (c == "ota") {
    // TASK-2007 F1: the loop saves {old}, the OTA start runs, and the loop saves {new} after the files were copied
    // (the writer task runs concurrently: one pass right after the submit), before LittleFS.end().
    fsWriterSubmit("/sat_pid_state.json", "{old}", 5);
    otaStart();
    fsWriterSubmit("/sat_pid_state.json", "{new}", 5);
    writerStep();                          // the writer task on the other core takes its next pass
    otaAfterKeep();
    for (int i = 0; i < 5; i++) writerStep();   // during the image write
    printf("after_end_writes %d\n", g_afterEnd);
    otaFinish();
    fsWriterDrain(1000);
    printf("final %s\n", g_files["/sat_pid_state.json"].c_str());
  } else if (c == "ota_end_gap") {
    // TASK-2007 F1 (b): the writer task takes a pass between LittleFS.end() and bFsImageWriting = true (two
    // separate statements at the OTA site); a write there lands on the partition the image is about to replace.
    fsWriterSubmit("/sat_pid_state.json", "{old}", 5);
    otaStart();
    fsWriterSubmit("/sat_pid_state.json", "{new}", 5);
    g_fsEnded = true;                      // LittleFS.end()
    writerStep();                          // the writer's pass in the gap
    ::state.flash.bFsImageWriting = true;
#ifdef HAS_STEP
    fsWriterResume();
#endif
    printf("after_end_writes %d\n", g_afterEnd);
    otaFinish();
    fsWriterDrain(1000);
    printf("final %s\n", g_files["/sat_pid_state.json"].c_str());
#ifdef HAS_STEP
  } else if (c == "flush_waits") {
    // The writer is inside a job when the pause comes: the flush waits until it is between jobs, then writes.
    fsWriterSubmit("/a.json", "{1}", 3);
    s_fswBusy = true;
    static int left = 3;
    g_writerStep = [] { if (--left <= 0) s_fswBusy = false; };   // the writer finishes its job after 3 delays
    printf("flush %d\n", (int)fsWriterPauseAndFlush(2000));
    printf("waited_ms %u\n", (unsigned)(g_ms - 1000));
    dump("flushed");
  } else if (c == "flush_timeout") {
    // The writer stays busy: the flush gives up after the timeout and leaves the queue as it is.
    fsWriterSubmit("/a.json", "{1}", 3);
    s_fswBusy = true;
    g_writerStep = nullptr;
    printf("flush %d\n", (int)fsWriterPauseAndFlush(2000));
    dump("timeout");
#endif
  } else if (c == "inflight") {
    // TASK-2007 F2: another task reads fsWriterPending() while the job is inside the queue send.
    static unsigned seen = 99;
    g_onSend = [] { seen = fsWriterPending(); };
    fsWriterSubmit("/a.json", "{1}", 3);
    g_onSend = nullptr;
    printf("pending_during_send %u\n", seen);
  } else if (c == "health_unmounted") {
    // TASK-1995: after a failed probe LittleFS reads unmounted; the writer must still run the next probe.
    LittleFSmounted = false;
    g_healthReq = 1;
    writerStep();
    printf("health_runs %d mounted %d\n", g_healthRuns, (int)LittleFSmounted);
  } else if (c == "health_first") {
    // a request goes before a queued write; the write follows on the next pass
    fsWriterSubmit("/a.json", "{1}", 3);
    g_healthReq = 1;
    writerStep();
    printf("health_runs %d pending %u\n", g_healthRuns, (unsigned)fsWriterPending());
    writerStep();
    printf("pending %u\n", (unsigned)fsWriterPending());
#ifdef HAS_PAUSE
  } else if (c == "health_held") {
    // held while paused (a filesystem OTA start) and while the image is written
    g_healthReq = 1;
    fsWriterPause();
    writerStep();
    printf("paused_runs %d\n", g_healthRuns);
    fsWriterResume();
    ::state.flash.bFsImageWriting = true;
    writerStep();
    printf("image_runs %d\n", g_healthRuns);
    ::state.flash.bFsImageWriting = false;
    writerStep();
    printf("released_runs %d\n", g_healthRuns);
#endif
#ifdef HAS_WRITE_FAILED
  } else if (c == "writefail") {
    // TASK-2020: a failed open and a short write set the flag; the next complete write clears it
    g_openFail = true;  fsWriterSubmit("/a.json", "{1}", 3); fsWriterDrain(1000);
    printf("open_fail %d\n", (int)fsWriterWriteFailed());
    g_openFail = false; fsWriterSubmit("/a.json", "{2}", 3); fsWriterDrain(1000);
    printf("ok %d\n", (int)fsWriterWriteFailed());
    g_shortWrite = true; fsWriterSubmit("/a.json", "{3}", 3); fsWriterDrain(1000);
    printf("short %d\n", (int)fsWriterWriteFailed());
    g_shortWrite = false; fsWriterSubmit("/a.json", "{4}", 3); fsWriterDrain(1000);
    printf("ok2 %d\n", (int)fsWriterWriteFailed());
    g_commitFail = true; fsWriterSubmit("/a.json", "{5}", 3); fsWriterDrain(1000);   // F3: write() and close() look fine
    printf("commit_fail %d\n", (int)fsWriterWriteFailed());
    g_commitFail = false; fsWriterSubmit("/a.json", "{6}", 3); fsWriterDrain(1000);
    printf("ok3 %d\n", (int)fsWriterWriteFailed());
#endif
  } else if (c == "big") {
    // TASK-2007 F3: 300 B does not fit in a job.
    std::string big(300, 'x');
    printf("submit %d\n", (int)fsWriterSubmit("/a.json", big.c_str(), big.size()));
    fsWriterDrain(1000);
    dump("big");
    printf("written_len %zu\n", g_files.count("/a.json") ? g_files["/a.json"].size() : (size_t)0);
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
        src = WRITER.read_text(encoding="utf-8").replace('#include "fsWriter.h"', HEADER.read_text(encoding="utf-8"))
        cls.has_step = "fsWriterStep" in src   # TASK-2007 code; older code runs the cases through the old task body
        cpp = Path(cls.tmp.name) / "w.cpp"
        cls.has_write_failed = "fsWriterWriteFailed" in src   # TASK-2020
        pre = ("#define HAS_STEP 1\n" if cls.has_step else "") + ("#define HAS_WRITE_FAILED 1\n" if cls.has_write_failed else "") + ("#define HAS_PAUSE 1\n" if "void fsWriterPause()" in src else "")
        cpp.write_text(pre + STUBS + src + MAIN, encoding="utf-8")
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

    def test_a_save_during_the_ota_start_is_not_lost_and_not_written_after_end(self):
        # TASK-2007 F1 (d3's review): a save that arrives after the files were copied must not be written into the
        # partition the image replaces (lost at the restore); it waits for the remount and lands after the copies.
        out = self.run_w("ota")
        self.assertIn("after_end_writes 0", out)
        self.assertIn("final {new}", out, "the newest save is the file after the OTA (old code: the copy {old} wins)")

    def test_no_write_in_the_gap_between_end_and_the_image_flag(self):
        # TASK-2007 F1 (b): old code writes {new} after LittleFS.end() (and loses it at the restore)
        out = self.run_w("ota_end_gap")
        self.assertIn("after_end_writes 0", out, "a write landed after LittleFS.end()")
        self.assertIn("final {new}", out)

    def test_the_flush_waits_for_a_busy_writer_then_writes(self):
        if not self.has_step:
            self.skipTest("fsWriterPauseAndFlush() is TASK-2007 code")
        out = self.run_w("flush_waits")
        self.assertIn("flush 1", out)
        self.assertNotIn("waited_ms 0", out, "the flush did not wait for the busy writer")
        self.assertIn("flushed ops=1 pending=0", out)

    def test_the_flush_times_out_on_a_writer_that_stays_busy_and_keeps_the_queue(self):
        # Known limit (kept): after the timeout the OTA logs and goes on; a writer stuck >2 s in one job could then
        # still be inside open/write at LittleFS.end().
        if not self.has_step:
            self.skipTest("fsWriterPauseAndFlush() is TASK-2007 code")
        out = self.run_w("flush_timeout")
        self.assertIn("flush 0", out)
        self.assertIn("timeout ops=0 pending=1", out)

    def test_pending_is_never_zero_while_a_job_is_being_queued(self):
        # TASK-2007 F2: the count goes up before the send, so a drain on another task cannot return early.
        self.assertIn("pending_during_send 1", self.run_w("inflight"))

    def test_a_failed_or_short_write_is_reported_and_cleared_by_the_next_write(self):
        # TASK-2020: the probe no longer writes, so the real writes report a filesystem that stopped writing
        self.assertTrue(self.has_write_failed, "fsWriterWriteFailed() missing (code before TASK-2020)")
        out = self.run_w("writefail")
        keys = ("open_fail", "ok", "short", "ok2", "commit_fail", "ok3")
        # F3 (second reader): LittleFS commits at close() and the Arduino close() drops the result, so only the
        # read-back sees a file that still holds the old copy
        self.assertEqual([l for l in out if l.split()[0] in keys],
                         ["open_fail 1", "ok 0", "short 1", "ok2 0", "commit_fail 1", "ok3 0"], out)

    def test_a_job_over_256_bytes_is_refused_and_logged_not_cut(self):
        # TASK-2007 F3
        out = self.run_w("big")
        self.assertIn("submit 0", out)
        self.assertTrue(any(l.startswith("LOG [fswriter] /a.json refused: 300 B") for l in out), out)
        self.assertIn("written_len 0", out)

    def test_the_writer_runs_a_health_check_while_littlefs_reads_unmounted(self):
        # TASK-1995: the probe moved to the writer task; behind the mount gate a failed probe would never be retried
        self.assertIn("health_runs 1 mounted 1", self.run_w("health_unmounted"))

    def test_a_health_check_goes_before_a_queued_write_which_follows(self):
        out = self.run_w("health_first")
        self.assertIn("health_runs 1 pending 1", out)
        self.assertIn("pending 0", out)

    def test_a_health_check_is_held_while_paused_and_during_an_image_write(self):
        if "void fsWriterPause()" not in WRITER.read_text(encoding="utf-8"):
            self.skipTest("fsWriterPause() is TASK-2007 code")
        out = self.run_w("health_held")
        self.assertEqual(out[1:4], ["paused_runs 0", "image_runs 0", "released_runs 1"], out)

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

    def test_filesystem_ota_pauses_and_flushes_before_it_copies_the_files(self):
        # TASK-2007 F1: pause + flush before _keepFiles(), resume only after bFsImageWriting holds the writer.
        keep = self.ota.index("_keepFiles();")
        flush = self.ota.index("fsWriterPauseAndFlush(")
        end = self.ota.index("LittleFS.end();", keep)
        flag = self.ota.index("::state.flash.bFsImageWriting = true;", end)
        resume = self.ota.index("fsWriterResume();")
        self.assertLess(flush, keep)
        self.assertLess(flag, resume)
        self.assertNotIn("fsWriterDrain(", self.ota[flush - 400:keep], "the plain drain left a gap before the copy")

    def test_the_remount_pauses_the_writer_until_the_restore(self):
        # TASK-2007 (bench 09, .63, beta.364): the remount clears the image flag before the heap copies are written
        # back; the writer must stay paused from before that flag drops until after _restoreRuntimeFiles().
        a = self.ota.index("bool _remountAfterFsWrite(bool partial) {")
        body = self.ota[a:self.ota.index("\n  }\n", a)]
        self.assertLess(body.index("fsWriterPause();"), body.index("::state.flash.bFsImageWriting = false;"))
        calls = [i for i in range(len(self.ota)) if self.ota.startswith("= _remountAfterFsWrite(", i)]
        self.assertEqual(len(calls), 2, "the end handler and the abort path")
        for c in calls:
            restore = self.ota.index("_restoreRuntimeFiles();", c)
            resume = self.ota.index("fsWriterResume();", c)
            self.assertLess(restore, resume, "resume only after the restore")
            nxt = self.ota.find("= _remountAfterFsWrite(", c + 1)
            self.assertTrue(nxt < 0 or resume < nxt, "each remount has its own resume")


if __name__ == "__main__":
    unittest.main(verbosity=2)
