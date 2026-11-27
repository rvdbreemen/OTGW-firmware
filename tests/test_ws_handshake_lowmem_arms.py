"""Lane A (TASK-3119 / TASK-3123): the /ws handshake handler with its two bench arms, wscool (30 s cooldown) and wsabort (RST instead of the 401).

One prerelease binary has to give four arms for the bench: base, cooldown only, abort only, both. This test runs the REAL wsHandshakeHandler()
of webSocketStuff.ino (with the real wsAdmitHandshake(), wsCooldownLeftMs() and the arm helpers) on a grid of free heap, connected clients, time
since the last recovery and both switches, against stubs, and compares it with the REAL handler of the send-guard build 3f9176298 (the B1
candidate, read with git show):
 - cooldown off + abort off: stdout equal to the B1 handler, byte for byte (log text, burst events, return value, no abort);
 - cooldown off + abort on: the same, plus exactly one abort on every refusal and none on an admission;
 - cooldown on: inside the 30 s a refusal is counted as the cooldown event and logged with its own text; outside it equals the B1 handler;
 - a release build (no _VERSION_PRERELEASE) ignores the switches: both measures are always on.
What it cannot reach: the library's 401 path, lwIP, the recovery itself and the heap: bench (n >= 5 per arm on .76, telnet key T after each stall).
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402

SRC = REPO / "src" / "OTGW-firmware"
WS_INO = Path(os.environ.get("OTGW_WS_INO", SRC / "webSocketStuff.ino"))
HELPER_INO = SRC / "helperStuff.ino"
B1_COMMIT = "3f9176298"

PRELUDE = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#define MAX_WEBSOCKET_CLIENTS 3
#define HEAP_WARNING_THRESHOLD @@WARN@@
#define PSTR(s) (s)
@@PRE@@
static uint32_t g_free = 0; static uint16_t g_connected = 0; static unsigned long g_now = 0;
static uint32_t platformFreeHeap() { return g_free; }
static unsigned long millis() { return g_now; }
static int g_aborts = 0;
static const uint8_t WS_BURST_REJECTED_MAX = 3;
static const uint8_t WS_BURST_REJECTED_HEAP = 4;
static const uint8_t WS_BURST_REJECTED_COOLDOWN = 6;
static void noteWebSocketBurstEvent(uint8_t t) { std::printf("burst=%u\n", (unsigned)t); }
#define DebugTf(...) std::printf(__VA_ARGS__)
struct AsyncClient { int8_t abort() { ++g_aborts; return 0; } };
struct AsyncWebServerRequest { AsyncClient c; AsyncClient* client() { return &c; } };
struct FakeWs { uint32_t count() const { return g_connected; } } otLogWs;
struct { struct { bool bWsCooldown = true; bool bWsAbort = true; } debug; } state;
"""

MAIN = r"""
@@BODY@@
int main(int argc, char** argv) {
  if (argc < 7) return 2;
  g_free = (uint32_t)strtoul(argv[1], 0, 10); g_connected = (uint16_t)atoi(argv[2]);
  g_now = (unsigned long)strtoul(argv[3], 0, 10);
  @@SETREC@@
  state.debug.bWsCooldown = argv[5][0] == '1'; state.debug.bWsAbort = argv[6][0] == '1';
  AsyncWebServerRequest req;
  const bool admitted = wsHandshakeHandler(&req);
  std::printf("admitted=%d\n", (int)admitted);
  std::fprintf(stderr, "aborts=%d\n", g_aborts);
  return 0;
}
"""


def cut(src, signature):
    m = re.search(r"^" + re.escape(signature) + r".*?^\}\s*$", src, re.S | re.M)
    if not m:
        raise AssertionError(signature + " not found")
    return m.group(0)


def lines(src, pattern):
    return "".join(m.group(0) + "\n" for m in re.finditer(pattern, src, re.M))


class Arms(unittest.TestCase):
    @classmethod
    def build(cls, name, body, pre, setrec):
        text = (PRELUDE.replace("@@WARN@@", cls.warn).replace("@@PRE@@", pre)
                + MAIN.replace("@@BODY@@", body).replace("@@SETREC@@", setrec))
        cpp = Path(cls.tmp.name) / (name + ".cpp")
        cpp.write_text(text, encoding="utf-8")
        exe = compile_cpp(cpp, Path(cls.tmp.name) / (name + (".exe" if os.name == "nt" else "")))
        if exe is None:
            raise unittest.SkipTest("no host C++ compiler")
        return exe

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.warn = re.search(r"#define HEAP_WARNING_THRESHOLD\s+(\d+)", HELPER_INO.read_text(encoding="utf-8")).group(1)
        new = WS_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        body = "\n".join([
            lines(new, r"^static const uint32_t WS_RECOVERY_COOLDOWN_MS\s*=.*;"),
            "static uint32_t g_wsRecoveryMs = 0;",
            cut(new, "static uint32_t wsCooldownLeftMs("),
            cut(new, "static bool wsAdmitHandshake("),
            cut(new, "static bool wsCooldownArmed("),
            cut(new, "static bool wsAbortArmed("),
            cut(new, "static bool wsHandshakeHandler("),
        ])
        setrec = "g_wsRecoveryMs = (uint32_t)strtoul(argv[4], 0, 10);"
        cls.new_exe = cls.build("new", body, "#define _VERSION_PRERELEASE 1", setrec)
        cls.rel_exe = cls.build("rel", body, "", setrec)
        try:
            old = subprocess.run(["git", "show", B1_COMMIT + ":src/OTGW-firmware/webSocketStuff.ino"], cwd=REPO,
                                 capture_output=True, text=True, check=True).stdout.replace("\r\n", "\n")
        except (OSError, subprocess.CalledProcessError):
            raise unittest.SkipTest("git or the B1 commit " + B1_COMMIT + " not available")
        old_body = cut(old, "static bool wsAdmitHandshake(") + "\n" + cut(old, "static bool wsHandshakeHandler(")
        cls.old_exe = cls.build("old", old_body, "", "")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_exe(self, exe, free, conn, now, rec, cool, abort):
        r = subprocess.run([str(exe), str(free), str(conn), str(now), str(rec), "1" if cool else "0", "1" if abort else "0"],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        # Windows text mode turns the CR LF of the log line into extra newlines: collapse every run of CR/LF to one LF
        return re.sub(r"[\r\n]+", "\n", r.stdout), int(r.stderr.strip().split("=")[1])

    GRID = [(f, c) for f in (0, 1600, 5119, 5120, 60000) for c in (0, 2, 3, 7)]
    NOW = 200000

    def test_both_off_equals_the_b1_handler_byte_for_byte(self):
        for free, conn in self.GRID:
            for rec in (0, self.NOW - 100, self.NOW - 29000):   # even with a fresh recovery stored
                with self.subTest(free=free, conn=conn, rec=rec):
                    old = self.run_exe(self.old_exe, free, conn, self.NOW, 0, True, True)
                    new = self.run_exe(self.new_exe, free, conn, self.NOW, rec, False, False)
                    self.assertEqual(new, old)
                    self.assertEqual(new[1], 0)

    def test_abort_on_cooldown_off_aborts_once_per_refusal_and_is_otherwise_the_b1_handler(self):
        for free, conn in self.GRID:
            with self.subTest(free=free, conn=conn):
                old_out, _ = self.run_exe(self.old_exe, free, conn, self.NOW, 0, True, True)
                out, aborts = self.run_exe(self.new_exe, free, conn, self.NOW, self.NOW - 100, False, True)
                self.assertEqual(out, old_out)
                self.assertEqual(aborts, 0 if "admitted=1" in out else 1)

    def test_cooldown_on_refuses_inside_30_s_with_its_own_event_and_text(self):
        for left_s, ago in ((30, 0), (1, 29001), (1, 29999)):
            with self.subTest(ago=ago):
                out, aborts = self.run_exe(self.new_exe, 60000, 0, self.NOW, self.NOW - ago, True, False)
                self.assertEqual(out, "burst=6\n[%d] WebSocket: refused at the handshake (recovery cooldown, %d s left)\nadmitted=0\n"
                                 % (self.NOW, left_s))
                self.assertEqual(aborts, 0)
        out, aborts = self.run_exe(self.new_exe, 60000, 0, self.NOW, self.NOW - 30000, True, True)
        self.assertEqual((out, aborts), ("admitted=1\n", 0))

    def test_cooldown_on_outside_the_window_is_the_b1_handler(self):
        for free, conn in self.GRID:
            with self.subTest(free=free, conn=conn):
                old = self.run_exe(self.old_exe, free, conn, self.NOW, 0, True, True)
                for rec in (0, self.NOW - 30000, self.NOW - 90000):
                    new = self.run_exe(self.new_exe, free, conn, self.NOW, rec, True, False)
                    self.assertEqual(new, old)

    def test_both_on_aborts_a_cooldown_refusal_too(self):
        out, aborts = self.run_exe(self.new_exe, 60000, 0, self.NOW, self.NOW - 100, True, True)
        self.assertIn("recovery cooldown", out)
        self.assertEqual(aborts, 1)

    def test_a_release_build_ignores_the_switches(self):
        out, aborts = self.run_exe(self.rel_exe, 60000, 0, self.NOW, self.NOW - 100, False, False)
        self.assertIn("recovery cooldown", out)
        self.assertEqual(aborts, 1)
        out, aborts = self.run_exe(self.rel_exe, 1600, 0, self.NOW, 0, False, False)
        self.assertEqual(aborts, 1)


class Wiring(unittest.TestCase):
    def test_flags_are_registered_in_prerelease_only_with_distinct_keys(self):
        flags = (SRC / "debugFlags.h").read_text(encoding="utf-8")
        pre = flags[flags.index("#if defined(_VERSION_PRERELEASE)"):]
        pre = pre[:pre.index("#endif")]
        self.assertIn('{ "wscool",    \'m\', &DebugSection::bWsCooldown', pre)
        self.assertIn('{ "wsabort",   \'t\', &DebugSection::bWsAbort', pre)
        keys = re.findall(r"\{ \"[a-z]+\",\s+'(.)'", flags)
        self.assertEqual(len(keys), len(set(keys)), "a telnet key is used twice: " + str(keys))
        handle = (SRC / "handleDebug.ino").read_text(encoding="utf-8")
        for k in ("m", "t"):
            self.assertNotIn("case '%s':" % k, handle, "key %s is also a hand-written telnet case" % k)
        stuff = (SRC / "debugStuff.h").read_text(encoding="utf-8")
        self.assertRegex(stuff, r"bool\s+bWsCooldown\s*=\s*true;")
        self.assertRegex(stuff, r"bool\s+bWsAbort\s*=\s*true;")

    def test_abort_comes_after_the_count_and_the_log_and_is_not_request_abort(self):
        body = cut(WS_INO.read_text(encoding="utf-8").replace(chr(13) + chr(10), chr(10)), "static bool wsHandshakeHandler(")
        self.assertLess(body.index("noteWebSocketBurstEvent("), body.index("->abort()"))
        self.assertLess(body.index("DebugTf("), body.index("->abort()"))
        self.assertIn("request->client()->abort()", body)   # NOT request->abort(): that sets _sent and the 401 built by the library would leak


if __name__ == "__main__":
    unittest.main()
