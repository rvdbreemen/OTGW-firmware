#!/usr/bin/env python3
"""TASK-1537 (Sergeant D entry 233, Q-1537a A): the Immergas quirk follows thermo-nova.

thermo-nova (coordinator/mqtt/opentherm.py:216-219 at 7beaaf5) sends `TP=11:12={min(value, 80)}` right before every
`MM={value}`, with value = the max relative modulation, and MM itself uncapped. Before this task the firmware sent
`TP=11:12=<flow setpoint>` after CS/MM/CH on every tick and capped MM itself at 80 for Immergas.

What runs on the host: the real satComputeMM() and satSendMM(), cut out of src/OTGW-firmware/SATcontrol.ino and compiled
with a recording addCommandToQueue() stub. A structural check covers the removed tick-level TP command.
OTGW_SAT_INO points at another copy for old-vs-fix runs.
Run: python tests/test_sat_immergas_tp.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_sat_published_curve import compile_cpp  # noqa: E402

REPO = Path(__file__).resolve().parent.parent
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))

STUBS = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#define PSTR(s) (s)
#define snprintf_P snprintf
#define SAT_QUIRK_MIN_MOD_10   0x01
#define SAT_QUIRK_IMMERGAS_TP  0x02
static std::vector<std::string> g_q;
enum class CmdQueueResult : uint8_t { Queued, Replaced, Sent, Dropped };   // TASK-1574
static CmdQueueResult addCommandToQueue(const char* buf, int len, bool, int) { g_q.push_back(std::string(buf, len)); return CmdQueueResult::Queued; }
static uint8_t g_quirks = 0;
static uint8_t satGetManufacturerQuirks() { return g_quirks; }
static bool _sat_mmSentValid = false;   // TASK-1570 on-change flag (cleared per scenario below)
static const uint32_t SAT_MMCH_REPAIR_MS = 60000UL;   // TASK-1570 repair window (real value in SATcontrol.ino)
#define SEND(v) do { _sat_mmSentValid = false; satSendMM(v); } while (0)
enum { SAT_MFR_AUTO = 0, SAT_MFR_IMMERGAS = 8, SAT_MFR_SIME = 14 };
struct { struct { uint8_t iManufacturer; } sat; } settings;
static uint32_t millis() { return 1000; }
struct { struct { uint8_t iLastSentMM; uint32_t iLastSentMMMs; bool bLastSentValid; } sat; } state;
"""

MAIN = r"""
static void dump(const char* tag) {
  printf("%s", tag);
  for (auto& s : g_q) printf("|%s", s.c_str());
  printf("\n");
  g_q.clear();
}
int main() {
  settings.sat.iManufacturer = SAT_MFR_IMMERGAS;   // chosen explicitly
  g_quirks = SAT_QUIRK_IMMERGAS_TP;
  SEND(100); dump("immergas100");
  SEND(55);  dump("immergas55");
  SEND(0);   dump("immergas0");
  // Auto with MemberID 27 detected as Immergas (the quirk flags say Immergas): no TP, it may be a Sime (entry 235)
  settings.sat.iManufacturer = SAT_MFR_AUTO; g_quirks = SAT_QUIRK_IMMERGAS_TP;
  SEND(100); dump("auto27");
  settings.sat.iManufacturer = SAT_MFR_SIME; g_quirks = 0;
  SEND(100); dump("sime");
  g_quirks = 0;
  SEND(100); dump("plain100");
  printf("mm|%u|%u\n", (unsigned)satComputeMM(false, false, false, 100, SAT_QUIRK_IMMERGAS_TP),
                       (unsigned)satComputeMM(true, false, false, 100, SAT_QUIRK_IMMERGAS_TP));
  return 0;
}
"""


def cut(src, start):
    a = src.index(start)
    depth, i = 0, src.index("{", a)
    while True:
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
            if depth == 0:
                return src[a:i + 1]
        i += 1


class ImmergasTp(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = SAT_INO.read_text(encoding="utf-8")
        # TASK-1581: satSendMM() uses the MM=T marker; take its real declaration from the source (absent on older code).
        mmt = re.search(r"^static const uint8_t SAT_MM_T = \d+;", cls.src, re.M)
        body = ((mmt.group(0) + "\n" if mmt else "")
                + cut(cls.src, "static uint8_t satComputeMM(") + "\n" + cut(cls.src, "static bool satImmergasTpActive(")
                + "\n" + cut(cls.src, "static void satSendMM(uint8_t mm)"))
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(STUBS + body + MAIN, encoding="utf-8")
        exe = compile_cpp(cpp, Path(cls.tmp.name) / "t.exe")
        if exe is None:
            raise unittest.SkipTest("no host C++ compiler")
        out = subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout
        cls.rows = {ln.split("|")[0]: ln.split("|")[1:] for ln in out.splitlines()}

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_tp_with_min_mm_80_goes_right_before_every_mm(self):
        self.assertEqual(self.rows["immergas100"], ["TP=11:12=80", "MM=100"])
        self.assertEqual(self.rows["immergas55"], ["TP=11:12=55", "MM=55"])
        self.assertEqual(self.rows["immergas0"], ["TP=11:12=0", "MM=0"])   # PWM: MM=0, TP follows (as thermo-nova)

    def test_auto_detected_member_id_27_sends_no_tp(self):
        # Robert 2026-11-02 (entry 235): MemberID 27 is Immergas and Sime; only an explicit choice sends the TP.
        self.assertEqual(self.rows["auto27"], ["MM=100"])
        self.assertEqual(self.rows["sime"], ["MM=100"])

    def test_other_boilers_get_no_tp(self):
        self.assertEqual(self.rows["plain100"], ["MM=100"])

    def test_mm_itself_is_not_capped_for_immergas(self):
        self.assertEqual(self.rows["mm"], ["100", "0"])

    def test_the_control_tick_no_longer_sends_the_setpoint_as_tp(self):
        self.assertFalse('PSTR("TP=11:12=%.0f"), finalSetpoint' in self.src, "the tick still sends TP=11:12=<setpoint>")
        self.assertIsNone(re.search(r"TP=11:12=%\.0f", self.src))


if __name__ == "__main__":
    unittest.main(verbosity=2)
