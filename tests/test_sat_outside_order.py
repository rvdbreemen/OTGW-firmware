#!/usr/bin/env python3
"""TASK-1501: host test for the order of the outside-temperature sources (Sergeant D entry 139, with 57 and 126).

The external outside temperature (MQTT/REST) wins for its whole valid hour. The test cuts the real
satResolveOutsideTemp() block ("// --- satOutside begin" .. "end") out of SATcontrol.ino and compiles it with a host
C++ compiler; nothing is re-implemented. OTGW_SAT_INO=<copy of the old SATcontrol.ino> runs it against other code
(old-vs-fix). Validity (the return value) must be identical on old and new code; only the returned value changes.

Run: python tests/test_sat_outside_order.py
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

SAT_INO = Path(os.environ.get("OTGW_SAT_INO", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#define PSTR(x) x
#define snprintf_P snprintf
typedef long long time_t_;
#define time_t time_t_
static long long fake_time() { return 1800000000LL; }
#define time(x) fake_time()
static bool isNTPtimeSet() { return true; }
struct Weather { bool bValid = false; uint32_t iLastUpdateMs = 0; float fTemperature = 0.0f; };
struct SatSettings { bool bSimulation = false; };
struct Settings { SatSettings sat; };
struct SatState { float fSimOutdoorTemp = 0, fExternalOutdoor = 0; bool bExternalOutdoorValid = false; uint32_t iExternalOutdoorLastMs = 0;
                  bool bRestoredOutdoorValid = false; float fRestoredOutdoor = 0; uint32_t iRestoredOutdoorMs = 0; Weather weather; };
struct OTSys { float Toutside = 0.0f; }; static OTSys OTcurrentSystemState;
struct OtBusState { uint32_t iToutsideBoilerMs = 0; };
struct State { SatState sat; OtBusState otBus; };
static Settings settings; static State state;
static uint32_t g_ms = 10000000UL;
static uint32_t millis() { return g_ms; }
static const uint32_t SAT_STALE_OUTDOOR_MS = 600000UL;   // present in the old code only; unused afterwards
@OUTSIDE@
// argv: extAgeS extVal otAgeS otVal wxAgeS wxVal restAgeS restVal   (age < 0 = source absent)
int main(int argc, char** argv) {
  auto age = [](const char* a) { return (long long)atoll(a); };
  long long ea = age(argv[1]), oa = age(argv[3]), wa = age(argv[5]), ra = age(argv[7]);
  if (ea >= 0) { state.sat.bExternalOutdoorValid = true; state.sat.iExternalOutdoorLastMs = g_ms - (uint32_t)(ea * 1000); state.sat.fExternalOutdoor = (float)atof(argv[2]); }
  if (oa >= 0) { state.otBus.iToutsideBoilerMs = g_ms - (uint32_t)(oa * 1000); OTcurrentSystemState.Toutside = (float)atof(argv[4]); }
  if (wa >= 0) { state.sat.weather.bValid = true; state.sat.weather.iLastUpdateMs = g_ms - (uint32_t)(wa * 1000); state.sat.weather.fTemperature = (float)atof(argv[6]); }
  if (ra >= 0) { state.sat.bRestoredOutdoorValid = true; state.sat.iRestoredOutdoorMs = g_ms - (uint32_t)(ra * 1000); state.sat.fRestoredOutdoor = (float)atof(argv[8]); }
  float v; uint32_t ms;
  bool ok = satResolveOutsideTemp(&v, &ms);
  printf("%d %.1f\n", (int)ok, v);
  return 0;
}
"""

_exe = None
_dir = None


def _setup():
    global _exe, _dir
    if _exe is not None or _dir is not None:
        return
    src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    i = src.index("// --- satOutside begin")
    block = src[i:src.index("// --- satOutside end", i)]
    _dir = tempfile.TemporaryDirectory()
    cpp = Path(_dir.name) / "t.cpp"
    cpp.write_text(STUBS.replace("@OUTSIDE@", block), encoding="utf-8")
    _exe = compile_cpp(cpp, Path(_dir.name) / "t.exe")


def resolve(ext=None, ot=None, wx=None, rest=None):
    """Each source is (age_s, value) or None. Returns (valid, value)."""
    _setup()
    if not _exe:
        raise unittest.SkipTest("no host C++ compiler")
    a = []
    for s in (ext, ot, wx, rest):
        a += [str(s[0]), str(s[1])] if s else ["-1", "0"]
    out = subprocess.run([str(_exe)] + a, capture_output=True, text=True, check=True).stdout.split()
    return out[0] == "1", float(out[1])


MIN = 60
HOUR = 3600


class OutsideOrder(unittest.TestCase):
    def test_a_external_30min_beats_fresh_msgid27_and_weather(self):
        self.assertEqual(resolve(ext=(30 * MIN, 3.0), ot=(10, 5.0), wx=(10, 7.0)), (True, 3.0))

    def test_b_external_30min_beats_weather(self):
        self.assertEqual(resolve(ext=(30 * MIN, 3.0), wx=(10, 7.0)), (True, 3.0))

    def test_c_boundary_at_one_hour(self):
        self.assertEqual(resolve(ext=(HOUR - 1, 3.0), ot=(10, 5.0), wx=(10, 7.0)), (True, 3.0))
        self.assertEqual(resolve(ext=(HOUR, 3.0), ot=(10, 5.0), wx=(10, 7.0)), (True, 5.0))
        self.assertEqual(resolve(ext=(HOUR, 3.0), wx=(10, 7.0)), (True, 7.0))

    def test_d_external_61min_falls_through_in_order(self):
        e = (61 * MIN, 3.0)
        self.assertEqual(resolve(ext=e, ot=(10, 5.0), wx=(10, 7.0), rest=(10, 9.0)), (True, 5.0))
        self.assertEqual(resolve(ext=e, wx=(10, 7.0), rest=(10, 9.0)), (True, 7.0))
        self.assertEqual(resolve(ext=e, rest=(10, 9.0)), (True, 9.0))

    def test_e_msgid27_zero_loses_to_weather(self):
        self.assertEqual(resolve(ot=(10, 0.0), wx=(10, 7.0)), (True, 7.0))
        self.assertEqual(resolve(ot=(10, 0.0)), (True, 0.0))

    def test_f_validity_unchanged_external_only(self):
        self.assertEqual(resolve(ext=(HOUR - 1, 3.0))[0], True)
        self.assertEqual(resolve(ext=(HOUR, 3.0))[0], False)
        self.assertEqual(resolve(ext=(11 * MIN, 3.0))[0], True)


if __name__ == "__main__":
    unittest.main()
