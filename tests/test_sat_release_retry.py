#!/usr/bin/env python3
"""TASK-1574: the SAT hand-back (CS=0, CH=1, MM=T) is repeated until the PIC queue accepted it.

Nothing is re-implemented. The real code is cut out of SATcontrol.ino: the command cache and senders (from
SAT_CMD_REFRESH_MS up to the TC= comment, which holds satRetryRelease) and satDisable(). The stubs are those of
test_sat_ot_loss.py, with an addCommandToQueue() that answers Dropped while g_drop is set (and records nothing then).

OTGW_SAT_INO points at another copy (`git show HEAD:src/OTGW-firmware/SATcontrol.ino`) for the old-vs-fix run: on the old
code a dropped CS=0 is never repaired (satRetryRelease does not exist, the stub below does nothing).

Not executed here: the loop call site (checked at source level) and the restart hand-back in satRestartStep().

Run: python tests/test_sat_release_retry.py
"""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_pwm_override import compile_cpp  # noqa: E402
import test_sat_ot_loss as ol  # noqa: E402

SRC = REPO / "src" / "OTGW-firmware"
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", SRC / "SATcontrol.ino"))

OLD_STUB = ('static CmdQueueResult addCommandToQueue(const char* cmd, int, bool, int, bool = false) '
            '{ printf("%u %s\\n", (unsigned)(g_ms / 1000), cmd); return CmdQueueResult::Queued; }')
NEW_STUB = ('static int g_drop = 0;   // 0 = accept, 1 = drop everything, 2 = drop only MM=\n'
            'static int g_attempts = 0;\n'
            'static CmdQueueResult addCommandToQueue(const char* cmd, int, bool, int, bool = false) {\n'
            '  g_attempts++;\n'
            '  if (g_drop == 1 || (g_drop == 2 && !strncmp(cmd, "MM=", 3))) return CmdQueueResult::Dropped;\n'
            '  printf("%u %s\\n", (unsigned)(g_ms / 1000), cmd); return CmdQueueResult::Queued; }')

MAIN = r"""
int main(int argc, char** argv) {
  int sc = atoi(argv[1]);
  g_ms = 10000;
  state.sat.bActive = true;
  g_drop = (sc == 2) ? 2 : 1;
  satDisable();                                     // the hand-back; dropped (all, or only MM=T)
  printf("-- after satDisable attempts=%d\n", g_attempts);
  g_attempts = 0;
  for (int pass = 1; pass <= 3; pass++) {           // SAT stays disabled while the queue refuses
    g_ms += 1000;
    if (sc == 3) state.sat.bActive = true;          // SAT takes control again: nothing is released any more
    satRetryRelease();
  }
  printf("-- while dropping attempts=%d\n", g_attempts);
  g_drop = 0;
  for (int pass = 1; pass <= 3; pass++) { g_ms += 1000; satRetryRelease(); }
  printf("-- end\n");
  return 0;
}
"""


def build_cpp(src: str) -> str:
    stubs = ol.STUBS[:ol.STUBS.index("int main(")]
    stubs = stubs.replace(OLD_STUB, NEW_STUB)
    assert NEW_STUB in stubs
    a = src.index("static const uint32_t SAT_CMD_REFRESH_MS")
    senders = src[a:src.index("// TASK-1272, Sergeant D entry 14", a)]
    d0 = src.index("void satDisable(")
    d1 = src.index("//=== Flush short-lived", d0)
    if "// --- satOtLink begin" in src:
        d1 = min(d1, src.index("// --- satOtLink begin"))
    disable = src[d0:d1]
    decl = "void satDisable(bool persistPid = true);" if "bool persistPid" in disable else ""
    if "satRetryRelease" not in senders:
        senders += "\nstatic void satRetryRelease() {}"
    return (stubs.replace("@SENDERS@", senders).replace("@DISABLE@", disable).replace("@LINK@", "")
            .replace("@DECL@", decl).replace("@SPAN@", "") + MAIN)


class ReleaseRetry(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        d = Path(cls.dir.name)
        cpp = d / "t.cpp"
        cpp.write_text(build_cpp(SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")), encoding="utf-8")
        cls.exe = compile_cpp(cpp, d / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def run_sc(self, sc):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), str(sc)], capture_output=True, text=True, check=True).stdout.splitlines()
        return [l.split(" ", 1)[1] if l[0].isdigit() else l for l in out]

    def test_dropped_hand_back_is_repeated_until_accepted_then_quiet(self):
        out = self.run_sc(1)
        self.assertEqual(out, ["-- after satDisable attempts=3", "-- while dropping attempts=9",
                               "CS=0", "CH=1", "MM=T", "-- end"])   # first pass after the queue recovers; then quiet

    def test_partial_drop_repeats_all_three_until_all_accepted(self):
        out = self.run_sc(2)
        # satDisable: CS=0 and CH=1 accepted, MM=T dropped; each retry pass sends the three again (CS= and CH= replace
        # the queued ones in the PIC queue), the first pass with MM=T accepted ends it
        self.assertEqual(out.count("CS=0"), 1 + 3 + 1)
        self.assertEqual(out[-4:], ["CS=0", "CH=1", "MM=T", "-- end"])

    def test_sat_active_again_cancels_the_pending_release(self):
        out = self.run_sc(3)
        self.assertEqual(out[-2:], ["-- while dropping attempts=0", "-- end"])
        self.assertNotIn("MM=T", out[out.index("-- after satDisable attempts=3"):])


class CallSite(unittest.TestCase):
    def test_retry_runs_before_the_enabled_gate_and_every_early_return(self):
        src = SAT_INO.read_text(encoding="utf-8")
        run = src[src.index("static void satControlLoopRun()\r\n{" if "\r\n" in src else "static void satControlLoopRun()\n{"):]
        i = run.index("satRetryRelease();")
        self.assertLess(i, run.index("if (satMqttFallbackStep()) return;"))
        self.assertLess(i, run.index("if (!satIsEnabled() || isFlashing())"))

    def test_both_hand_backs_use_the_checked_release(self):
        src = SAT_INO.read_text(encoding="utf-8")
        self.assertEqual(src.count("_sat_releasePending = true;   // TASK-1574"), 2)
        self.assertIn("satSendRelease(restore)) _sat_releasePending", src)     # TASK-1541: satDisable() restores, the restart hand-back does not
        self.assertIn("satSendRelease(false)) _sat_releasePending", src)


if __name__ == "__main__":
    unittest.main(verbosity=2)
