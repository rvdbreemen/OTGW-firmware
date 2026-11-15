#!/usr/bin/env python3
"""TASK-1761: OT-Direct commands from another task run on the loop task, in arrival order.

Before: addCommandToQueue() called handleOTDirectCommand() on the caller's task, so a REST GW=R (async_tcp) ran
resetTransientState() next to loopOTDirect(), and a CS= right after it in the same burst could be wiped by the
late reset. Now a command from another task goes into the otdCmdTextRing and loopOTDirect() runs it between
frames; a loop-task caller drains the ring first, so it never overtakes an older command.

The code under test is REAL: the "// --- otdCmdTextRing" block of src/OTGW-firmware/OTDirect.ino and the
OT-Direct branch of addCommandToQueue() in OTGW-Core.ino (the `if (isOTDirectEnabled()) {` block), compiled with:
  - platformOnLoopTask() reading a thread_local flag (the loop thread sets it);
  - portMUX_TYPE / portENTER_CRITICAL / portEXIT_CRITICAL as a std::mutex (a host stand-in for the spinlock: the
    test checks the ring logic under two real producer threads, not the ESP32 spinlock itself);
  - handleOTDirectCommand() recording the text and whether it ran on the loop thread.
Source checks: loopOTDirect() drains only with no master request in flight, the PIC part of addCommandToQueue() is
unchanged against git origin/dev, and the REST command handler answers 503 on Dropped.

Old-vs-fix: OTGW_OTD_INO=<old OTDirect.ino> OTGW_CORE_INO=<old OTGW-Core.ino> python tests/test_otd_loop_task_commands.py
Run: python tests/test_otd_loop_task_commands.py
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

FW = REPO / "src" / "OTGW-firmware"
OTD_INO = Path(os.environ.get("OTGW_OTD_INO", FW / "OTDirect.ino"))
CORE_INO = Path(os.environ.get("OTGW_CORE_INO", FW / "OTGW-Core.ino"))
OTD = OTD_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
CORE = CORE_INO.read_text(encoding="utf-8").replace("\r\n", "\n")


def braced(src, start):
    i, depth = src.index("{", start), 0
    for j in range(i, len(src)):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[start:j + 1]
    raise AssertionError("unbalanced")


def add_cmd_fn(core):
    a = core.index("CmdQueueResult addCommandToQueue(const char* buf, const int len, const bool forceQueue")
    return braced(core, a)


def otd_branch(core):
    fn = add_cmd_fn(core)
    return braced(fn, fn.index("if (isOTDirectEnabled()) {"))


PRE = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
static struct { struct { uint16_t iCmdRingDropped = 0; } otd; } state;   // TASK-1966: the ring counts refused commands
#include <cstdlib>
#include <mutex>
#include <thread>
#include <vector>
#include <string>
#define PIC_CMD_MAX_LEN 15
enum class CmdQueueResult : uint8_t { Queued, Replaced, Sent, Dropped };
struct portMUX_TYPE { std::mutex m; };
#define portMUX_INITIALIZER_UNLOCKED {}
#define portENTER_CRITICAL(x) ((x)->m.lock())
#define portEXIT_CRITICAL(x) ((x)->m.unlock())
static thread_local bool g_loop = false;
static bool platformOnLoopTask() { return g_loop; }
static bool isOTDirectEnabled() { return true; }
static bool otMasterRequestActive = false;   // a master request in flight (loopOTDirect owns it)
#define OTDebugTf(...) do {} while (0)
#define PSTR(s) (s)
struct Ran { std::string text; bool onLoop; bool satOwn; };
static std::vector<Ran> g_ran; static std::mutex g_ranMux;
bool handleOTDirectCommand(const char* buf, int len, bool satOwn = false) {
  std::lock_guard<std::mutex> l(g_ranMux); g_ran.push_back({std::string(buf, len), g_loop, satOwn});
  return std::string(buf, len) != "XX=refused";
}
"""

MAIN = r"""
static CmdQueueResult submit(const char* s, bool satOwn = false) {
  const char* buf = s; const int len = (int)strlen(s);
  @BRANCH@
  return CmdQueueResult::Dropped;
}
static const char* rs(CmdQueueResult r) {
  return r == CmdQueueResult::Queued ? "Q" : r == CmdQueueResult::Sent ? "S" : r == CmdQueueResult::Dropped ? "D" : "R";
}
static void dump(const char* tag) {
  printf("%s", tag);
  for (auto& r : g_ran) printf(" %s@%s", r.text.c_str(), r.onLoop ? "loop" : "other");
  printf("\n"); g_ran.clear();
}
#ifndef HAVE_RING
static void otdCmdTextDrain() {}
static void otdMasterGuard() {}   // TASK-1966: boiler line guard, tests/test_otd_master_line_stale_delay.py
#endif
int main(int argc, char** argv) {
  const std::string mode = argc > 1 ? argv[1] : "";
  if (mode == "burst") {           // REST burst GW=R then CS=20 from async_tcp, then the loop pass
    CmdQueueResult a, b;
    std::thread t([&] { a = submit("GW=R"); b = submit("CS=20"); }); t.join();
    printf("results %s %s\n", rs(a), rs(b)); dump("before");
    g_loop = true; otdCmdTextDrain(); dump("after");
  } else if (mode == "overtake") {  // a queued REST GW=R, then a loop-task SAT CS= (and a refused one)
    std::thread t([] { submit("GW=R"); }); t.join();
    g_loop = true;
    CmdQueueResult r1 = submit("CS=21", true); CmdQueueResult r2 = submit("XX=refused", true);
    printf("results %s %s\n", rs(r1), rs(r2)); dump("ran");
  } else if (mode == "inflight") {  // a queued REST GW=R, then a loop-task CS= while a master request is in flight
    std::thread t([] { submit("GW=R"); }); t.join();
    g_loop = true; otMasterRequestActive = true;
    CmdQueueResult r1 = submit("CS=21", true);
    printf("results %s\n", rs(r1)); dump("in_flight");
    otMasterRequestActive = false; otdCmdTextDrain(); dump("after");
    CmdQueueResult r2 = submit("CS=22", true);   // nothing waits any more: direct again
    printf("results %s\n", rs(r2)); dump("direct");
  } else if (mode == "full") {      // 8 slots, one stays empty: 7 fit
    std::string out;
    std::thread t([&] { for (int i = 0; i < 8; i++) { char c[8]; snprintf(c, sizeof c, "CS=%d", i); out += rs(submit(c)); } });
    t.join(); printf("results %s\n", out.c_str());
    g_loop = true; otdCmdTextDrain(); dump("after");
    std::thread t2([&] { printf("long %s\n", rs(submit("CS=12345678901234567"))); }); t2.join();
  } else if (mode == "race") {      // two producers at once, the loop thread draining meanwhile
    const int N = 400; int dropped[2] = {0, 0};
    auto prod = [&](int p) { for (int i = 0; i < N; i++) { char c[16]; snprintf(c, sizeof c, "P%d=%d", p, i);
      while (submit(c) == CmdQueueResult::Dropped) { dropped[p]++; std::this_thread::yield(); } } };
    std::thread a(prod, 0), b(prod, 1);
    std::thread consumer([&] { g_loop = true; for (int k = 0; k < 200000; k++) { otdCmdTextDrain(); std::this_thread::yield();
      std::lock_guard<std::mutex> l(g_ranMux); if ((int)g_ran.size() >= 2 * N) break; } });
    a.join(); b.join(); consumer.join();
    g_loop = true; otdCmdTextDrain();
    int next[2] = {0, 0}, bad = 0, offloop = 0;
    for (auto& r : g_ran) { int p = r.text[1] - '0'; int v = atoi(r.text.c_str() + 3); if (v != next[p]) bad++; next[p] = v + 1;
      if (!r.onLoop) offloop++; }
    printf("race total=%zu p0=%d p1=%d out_of_order=%d offloop=%d retries=%d\n", g_ran.size(), next[0], next[1], bad,
           offloop, dropped[0] + dropped[1]);
  }
  return 0;
}
"""

_tmp = None
_exe = None


def setUpModule():
    global _tmp, _exe
    m = re.search(r"// --- otdCmdTextRing begin\n(.*?)// --- otdCmdTextRing end", OTD, re.S)
    ring = ("#define HAVE_RING 1\n" + m.group(1)) if m else ""
    branch = otd_branch(CORE)
    src = PRE + ring + MAIN.replace("@BRANCH@", branch)
    _tmp = Path(tempfile.mkdtemp(prefix="otd_loop_"))
    cpp = _tmp / "t.cpp"
    cpp.write_text(src, encoding="utf-8")
    _exe = compile_cpp(cpp, _tmp / ("t.exe" if os.name == "nt" else "t"))
    if _exe is None:
        raise unittest.SkipTest("no C++ compiler")


def tearDownModule():
    if _tmp:
        shutil.rmtree(_tmp, ignore_errors=True)


def run(mode):
    return subprocess.run([str(_exe), mode], capture_output=True, text=True, check=True, timeout=120).stdout.splitlines()


class LoopTaskCommands(unittest.TestCase):
    def test_rest_burst_runs_on_the_loop_task_in_order(self):
        out = run("burst")
        self.assertEqual(out[0], "results Q Q")
        self.assertEqual(out[1], "before", "nothing runs on the async_tcp task")
        self.assertEqual(out[2], "after GW=R@loop CS=20@loop")

    def test_a_loop_task_command_never_overtakes_and_keeps_its_result(self):
        out = run("overtake")
        self.assertEqual(out[0], "results S D", "SAT keeps the synchronous Sent / Dropped (TASK-1593)")
        self.assertEqual(out[1], "ran GW=R@loop CS=21@loop XX=refused@loop")

    def test_queued_commands_never_run_while_a_frame_is_in_flight(self):
        # 92's review F1: GW= calls setOTDirectMode() at once, so a queued GW=R must wait for the frame boundary
        # also when a loop-task command arrives mid-frame; that command queues behind it.
        out = run("inflight")
        self.assertEqual(out[0], "results Q")
        self.assertEqual(out[1], "in_flight", "nothing runs while the master request is in flight")
        self.assertEqual(out[2], "after GW=R@loop CS=21@loop")
        self.assertEqual(out[3:5], ["results S", "direct CS=22@loop"])

    def test_a_full_ring_is_dropped(self):
        out = run("full")
        self.assertEqual(out[0], "results QQQQQQQD")
        self.assertEqual(out[1], "after CS=0@loop CS=1@loop CS=2@loop CS=3@loop CS=4@loop CS=5@loop CS=6@loop")
        self.assertEqual(out[2], "long D", "longer than PIC_CMD_MAX_LEN is Dropped")

    def test_two_producers_at_once(self):
        out = run("race")[0]
        self.assertRegex(out, r"^race total=800 p0=400 p1=400 out_of_order=0 offloop=0 ")


class SourceChecks(unittest.TestCase):
    def test_loop_drains_between_frames(self):
        fn = braced(OTD, OTD.index("void loopOTDirect() {"))
        self.assertIn("if (!otMasterRequestActive) otdCmdTextDrain();", fn)
        self.assertLess(fn.index("otdCmdTextDrain();"), fn.index("otMaster.process();"))

    def test_pic_path_of_add_command_to_queue_unchanged(self):
        base = subprocess.run(["git", "-C", str(REPO), "show", "origin/dev:src/OTGW-firmware/OTGW-Core.ino"],
                              capture_output=True, text=True, encoding="utf-8", check=True).stdout.replace("\r\n", "\n")
        def pic_part(core):
            fn = add_cmd_fn(core)
            return fn.replace(otd_branch(core), "")
        self.assertEqual(pic_part(CORE), pic_part(base))

    def test_rest_reports_a_dropped_command(self):
        rest = (FW / "restAPI.ino").read_text(encoding="utf-8")
        self.assertIn("if (addCommandToQueue(cmdStr, static_cast<int>(cmdLen)) == CmdQueueResult::Dropped) {", rest)
        self.assertIn('sendApiError(503, F("Command not queued (queue full)"));', rest)


if __name__ == "__main__":
    unittest.main(verbosity=2)
