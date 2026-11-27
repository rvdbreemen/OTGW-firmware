"""TASK-3123: no /ws handshake is admitted for 30 s after an emergency recovery that aborted at least one /ws client.

Why (bench 2026-11-27 .76, B1 run 2): after emergencyHeapRecovery() aborts the tabs they reconnect after 5 s (v2.js:3037-3040) and on the
unit without PSRAM that drove the heap under 3 KB again within about 50 s, so the next recovery aborted them again. The handshake gate refuses
only on the heap it sees at that moment; the cooldown keeps the tabs away while the recovery's effect settles.

What this test runs on the host: the REAL wsAdmitHandshake() and wsCooldownLeftMs() cut out of webSocketStuff.ino, on a grid of time since the
recovery (0 ms, 29.9 s, 30 s), no recovery yet, a wrap of millis() across 2^32, and the old heap / client-count rules after the cooldown.
Text guards: the recovery path stores the time only inside the branch where ws_aborted > 0; the handshake handler counts and logs the
cooldown as its own reason. A mutant with a cooldown of 0 ms is turned red. Red on the code before the change: the functions do not exist.
What it cannot reach: the real recovery, the browser's reconnect timing and the heap behaviour: bench (TASK-3123 AC5).
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

MAIN = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#define MAX_WEBSOCKET_CLIENTS 3
#define HEAP_WARNING_THRESHOLD @@WARN@@
@@CONSTS@@
@@FUNCS@@
int main(int argc, char** argv) {
  if (argc < 5) return 2;
  const uint32_t free_ = (uint32_t)strtoul(argv[1], 0, 10), now = (uint32_t)strtoul(argv[3], 0, 10), last = (uint32_t)strtoul(argv[4], 0, 10);
  const uint16_t conn = (uint16_t)atoi(argv[2]);
  std::printf("%d %lu\n", (int)wsAdmitHandshake(free_, conn, now, last), (unsigned long)wsCooldownLeftMs(now, last));
  return 0;
}
"""


def cut(src, signature):
    m = re.search(r"^" + re.escape(signature) + r".*?^\}\s*$", src, re.S | re.M)
    if not m:
        raise AssertionError(signature + " not found")
    return m.group(0)


class Cooldown(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = WS_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        warn = re.search(r"#define HEAP_WARNING_THRESHOLD\s+(\d+)", HELPER_INO.read_text(encoding="utf-8")).group(1)
        consts = "".join(m.group(0) + "\n" for m in re.finditer(r"^static const uint32_t WS_RECOVERY_COOLDOWN_MS\s*=.*;", cls.src, re.M))
        funcs = cut(cls.src, "static uint32_t wsCooldownLeftMs(") + "\n" + cut(cls.src, "static bool wsAdmitHandshake(")
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = cls.build("cd", consts, funcs, warn)

    @classmethod
    def build(cls, name, consts, funcs, warn):
        cpp = Path(cls.tmp.name) / (name + ".cpp")
        cpp.write_text(MAIN.replace("@@WARN@@", warn).replace("@@CONSTS@@", consts).replace("@@FUNCS@@", funcs), encoding="utf-8")
        exe = compile_cpp(cpp, Path(cls.tmp.name) / (name + (".exe" if os.name == "nt" else "")))
        if exe is None:
            raise unittest.SkipTest("no host C++ compiler")
        return exe

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_exe(self, exe, free, conn, now, last):
        r = subprocess.run([str(exe), str(free), str(conn), str(now), str(last)], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        a, left = r.stdout.split()
        return a == "1", int(left)

    def at(self, now, last, free=60000, conn=0):
        return self.run_exe(self.exe, free, conn, now, last)

    def test_refused_at_0_and_29_9_s_admitted_at_30_s(self):
        last = 100000
        self.assertEqual(self.at(last, last), (False, 30000))
        self.assertEqual(self.at(last + 29900, last), (False, 100))
        self.assertEqual(self.at(last + 29999, last), (False, 1))
        self.assertEqual(self.at(last + 30000, last), (True, 0))
        self.assertEqual(self.at(last + 120000, last), (True, 0))

    def test_no_recovery_yet_admits(self):
        self.assertEqual(self.at(5, 0), (True, 0))
        self.assertEqual(self.at(4000000000, 0), (True, 0))

    def test_wrap_of_millis_across_2_pow_32(self):
        last = 4294967040          # 256 ms before the wrap
        self.assertEqual(self.at(100, last), (False, 30000 - 356))
        self.assertEqual(self.at((last + 29999) & 0xFFFFFFFF, last), (False, 1))
        self.assertEqual(self.at((last + 30000) & 0xFFFFFFFF, last), (True, 0))

    def test_old_rules_still_apply_after_the_cooldown(self):
        last = 1000
        self.assertEqual(self.at(last + 30000, last, free=5119)[0], False)
        self.assertEqual(self.at(last + 30000, last, free=5120)[0], True)
        self.assertEqual(self.at(last + 30000, last, conn=3)[0], False)

    def test_mutant_with_a_cooldown_of_0_ms_is_red(self):
        mutated = re.sub(r"(WS_RECOVERY_COOLDOWN_MS\s*=\s*)\d+", r"\g<1>0", self.src, count=1)
        consts = "".join(m.group(0) + "\n" for m in re.finditer(r"^static const uint32_t WS_RECOVERY_COOLDOWN_MS\s*=.*;", mutated, re.M))
        funcs = cut(mutated, "static uint32_t wsCooldownLeftMs(") + "\n" + cut(mutated, "static bool wsAdmitHandshake(")
        warn = re.search(r"#define HEAP_WARNING_THRESHOLD\s+(\d+)", HELPER_INO.read_text(encoding="utf-8")).group(1)
        exe = self.build("mut", consts, funcs, warn)
        self.assertEqual(self.run_exe(exe, 60000, 0, 100000, 100000)[0], True, "the mutant admits at +0 s: the real code must not")
        self.assertEqual(self.at(100000, 100000)[0], False)


class Wiring(unittest.TestCase):
    def test_recovery_stores_the_time_only_when_ws_clients_were_aborted(self):
        s = HELPER_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        m = re.search(r"if \(wsTrackedBefore > 0 && \(wsAborted = doWebSocketDisconnectAll\(\)\) > 0\) \{(.*?)\n  \}", s, re.S)
        self.assertIsNotNone(m, "the ws abort branch of emergencyHeapRecovery() not found")
        self.assertIn("webSocketNoteRecoveryAbort();", m.group(1))
        self.assertEqual(s.count("webSocketNoteRecoveryAbort();"), 1, "the time must be stored in that branch only")

    def test_handler_counts_and_logs_the_cooldown_as_its_own_reason(self):
        s = WS_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        h = cut(s, "static bool wsHandshakeHandler(")
        self.assertIn("WS_BURST_REJECTED_COOLDOWN", h)
        self.assertIn("recovery cooldown", h)
        self.assertIn("wsAdmitHandshake(freeHeap, connected, now, g_wsRecoveryMs)", h)


if __name__ == "__main__":
    unittest.main()
