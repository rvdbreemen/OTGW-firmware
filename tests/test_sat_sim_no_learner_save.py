#!/usr/bin/env python3
"""TASK-1900: in SAT simulation neither satSavePidState() nor satHCRSaveState() writes its file.

Found in the TASK-1895 analysis: while settings.sat.bSimulation is on, the 5-minute PID save (/sat_pid_state.json)
and the heating-curve recommendation day commit (/sat/sat_hcr.json) still wrote LittleFS, so the next boot restored a
PID integral and a curve recommendation learned on simulated rooms. Same rule as TASK-1869 (thermal coefficient).

REAL code: satSavePidState() (SATcontrol.ino) and satHCRSaveState() (SATcycles.ino), cut from the source and compiled
on the host. STUBBED: LittleFS (counts opens for write), File, time, the PID state builders. Old-vs-fix:
OTGW_SAT_CONTROL / OTGW_SAT_CYCLES point at older copies.
Run: python tests/test_sat_sim_no_learner_save.py
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

FW = REPO / "src" / "OTGW-firmware"
CONTROL = Path(os.environ.get("OTGW_SAT_CONTROL", FW / "SATcontrol.ino"))
CYCLES = Path(os.environ.get("OTGW_SAT_CYCLES", FW / "SATcycles.ino"))


def cut_braced(src, signature):
    at = src.find(signature)
    if at < 0:
        raise AssertionError(signature + " not found")
    start = src.find("{", at)
    depth = 0
    i = start
    while i < len(src):
        c = src[i]
        if c in "'\"":   # skip a char or string literal ('}' in buf[n] = '}' must not count)
            j = i + 1
            while src[j] != c:
                j += 2 if src[j] == "\\" else 1
            i = j + 1
            continue
        if src.startswith("//", i):
            i = src.index("\n", i)
            continue
        depth += {"{": 1, "}": -1}.get(c, 0)
        if depth == 0:
            return src[at:i + 1]
        i += 1
    raise AssertionError("unbalanced braces in " + signature)


PRE = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#define F(s) (s)
#define PSTR(s) (s)
#define FPSTR(s) (s)
#define snprintf_P snprintf
#define SATDebugTf(...) ((void)0)
static int g_writes = 0;
struct File {
  bool ok = false;
  explicit operator bool() const { return ok; }
  void print(const char*) {}
  void close() {}
};
struct FS { File open(const char*, const char* mode, bool = false) { File f; f.ok = true; if (mode[0] == 'w') g_writes++; return f; } };
static FS LittleFS;
// TASK-1989: satSavePidState() hands the file to the fsWriter queue; here it writes at once, so g_writes still counts it.
static bool fsWriterSubmit(const char* p, const char* d, size_t) { File f = LittleFS.open(p, "w"); if (!f) return false; f.print(d); f.close(); return true; }
static uint32_t millis() { return 1000; }
static char* dtostrf(double v, int, int, char* b) { sprintf(b, "%.2f", v); return b; }
struct { struct { bool bSimulation = false; } sat; } settings;
static const char SAT_PID_STATE_FILE[] = "/sat_pid_state.json";
static const char SAT_HCR_FILE[] = "/sat/sat_hcr.json";
static uint32_t _pidLastSaveMs = 0;
static int satPidStateBuild(char* buf, size_t size, time_t) { return snprintf(buf, size, "{\"i\":0.0"); }
static int satPidStateAppendOutside(char*, int n, size_t, time_t) { return n; }
#define HCR_DAYS 7
#define HCR_INTRADAY_SIZE 16
static float _hcr_dailyMedian[HCR_DAYS];
static uint8_t _hcr_head = 0, _hcr_count = 0;
static float _hcr_samples[HCR_INTRADAY_SIZE];
static uint16_t _hcr_sHead = 0, _hcr_sCount = 0;
"""

MAIN = r"""
int main(int argc, char** argv) {
  settings.sat.bSimulation = argv[1][0] == '1';
  satSavePidState(); const int pid = g_writes;
  satHCRSaveState(); const int hcr = g_writes - pid;
  printf("pid=%d hcr=%d\n", pid, hcr);
  return 0;
}
"""


class SimNoLearnerSave(unittest.TestCase):
    tmp = None
    exe = None

    @classmethod
    def setUpClass(cls):
        ctl = CONTROL.read_text(encoding="utf-8").replace("\r\n", "\n")
        cyc = CYCLES.read_text(encoding="utf-8").replace("\r\n", "\n")
        body = cut_braced(ctl, "void satSavePidState()\n{") + "\n" + cut_braced(cyc, "void satHCRSaveState()\n{")
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(PRE + body + MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / ("t.exe" if os.name == "nt" else "t"))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_case(self, sim):
        if not self.exe:
            self.skipTest("no host compiler")
        out = subprocess.run([str(self.exe), sim], capture_output=True, text=True, check=True).stdout
        print("sim=" + sim, out.strip())
        return dict(kv.split("=") for kv in out.split())

    def test_simulation_writes_neither_file(self):
        self.assertEqual(self.run_case("1"), {"pid": "0", "hcr": "0"})

    def test_outside_simulation_both_still_save(self):
        self.assertEqual(self.run_case("0"), {"pid": "1", "hcr": "1"})


if __name__ == "__main__":
    unittest.main(verbosity=2)
