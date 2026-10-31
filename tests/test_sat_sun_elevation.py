#!/usr/bin/env python3
"""TASK-1410: the sun elevation the firmware computes from latitude, longitude and UTC time.

The test cuts the real satSunElevationDeg() out of src/OTGW-firmware/SATcontrol.ino ("// --- satSunElevation begin"
.. "end") and compiles it with a host C++ compiler. The expected values do not come from the same formula:
  - at local solar noon the elevation is 90 - |latitude - declination|, with the declination at the solstices
    +-23.44 degrees and about 0 at the equinox (geometry, independent of NOAA's series);
  - at the equinox, 6 hours from solar noon, the sun is on the horizon (hour angle 90 degrees, declination ~0);
  - below the horizon at local midnight in winter.
Solar noon in UTC = 12:00 - 4 min per degree east - equation of time; the equation of time on those dates
(-1.6 min on 21 June, +1.9 min on 21 December, -7.4 min on 20 March) is from published almanac tables, again
independent of the code under test. Tolerance 0.5 degree (AC #1).

Not reachable here: the precedence of an external MQTT value (satSunElevationStep) - covered by the source checks below
and the bench.

Run: python tests/test_sat_sun_elevation.py
"""
import calendar
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

SAT_INO = Path(os.environ.get("OTGW_SAT_INO", REPO / "src" / "OTGW-firmware" / "SATcontrol.ino"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#ifdef _WIN32
static struct tm* gmtime_r(const time_t* t, struct tm* out) { return gmtime_s(out, t) == 0 ? out : nullptr; }
#endif
"""
MAIN = r"""
int main(int argc, char** argv) {
  for (int i = 1; i + 2 < argc + 1 && i + 2 <= argc; i += 3) {
    printf("%.3f\n", satSunElevationDeg((float)atof(argv[i]), (float)atof(argv[i + 1]), (int64_t)atoll(argv[i + 2])));
  }
  return 0;
}
"""


def utc(y, mo, d, h, mi):
    return calendar.timegm((y, mo, d, h, mi, 0))


def noon_utc(y, mo, d, lon, eot_min):
    minutes = 12 * 60 - 4 * lon - eot_min
    return utc(y, mo, d, 0, 0) + int(round(minutes * 60))


class SunElevation(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        a, b = src.find("// --- satSunElevation begin"), src.find("// --- satSunElevation end")
        cls.missing = a < 0 or b < 0
        cls.tmp = tempfile.TemporaryDirectory()
        if cls.missing:
            cls.exe = None
            return
        cpp = Path(cls.tmp.name) / "t.cpp"
        cpp.write_text(STUBS + src[a:b] + MAIN, encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / "t.exe")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def elev(self, lat, lon, epoch):
        self.assertFalse(self.missing, "satSunElevationDeg not found in SATcontrol.ino")
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), str(lat), str(lon), str(epoch)], capture_output=True, text=True, check=True)
        return float(out.stdout.split()[0])

    def test_amsterdam_noon_summer_solstice(self):
        lat, lon = 52.37, 4.90
        self.assertAlmostEqual(self.elev(lat, lon, noon_utc(2026, 6, 21, lon, -1.6)), 90 - lat + 23.44, delta=0.5)

    def test_amsterdam_noon_winter_solstice(self):
        lat, lon = 52.37, 4.90
        self.assertAlmostEqual(self.elev(lat, lon, noon_utc(2026, 12, 21, lon, 1.9)), 90 - lat - 23.44, delta=0.5)

    def test_amsterdam_noon_equinox(self):
        lat, lon = 52.37, 4.90
        self.assertAlmostEqual(self.elev(lat, lon, noon_utc(2026, 3, 20, lon, -7.4)), 90 - lat, delta=0.5)

    def test_equinox_six_hours_from_noon_is_on_the_horizon(self):
        lat, lon = 52.37, 4.90
        n = noon_utc(2026, 3, 20, lon, -7.4)
        self.assertAlmostEqual(self.elev(lat, lon, n - 6 * 3600), 0.0, delta=0.5)
        self.assertAlmostEqual(self.elev(lat, lon, n + 6 * 3600), 0.0, delta=0.5)

    def test_southern_hemisphere_and_west_longitude(self):
        lat, lon = -33.87, -70.0   # December noon, sun near the zenith: 90 - |lat - 23.44|
        self.assertAlmostEqual(self.elev(lat, lon, noon_utc(2026, 12, 21, lon, 1.9)), 90 - abs(lat - (-23.44)), delta=0.5)

    def test_winter_midnight_below_the_horizon(self):
        self.assertLess(self.elev(52.37, 4.90, utc(2026, 1, 15, 23, 20)), -50.0)

    def test_twelve_degree_threshold_crossing_in_a_november_morning(self):
        # entry 160: sun below 12 degrees counts; on 31 Oct 2026 in Amsterdam the sun stays below 25 degrees
        noon = noon_utc(2026, 10, 31, 4.90, 16.4)
        top = self.elev(52.37, 4.90, noon)
        self.assertTrue(20.0 < top < 26.0, top)
        self.assertLess(self.elev(52.37, 4.90, noon - 4 * 3600), 12.0)


STEP_MAIN = r"""
struct SatS { float fWeatherLat = 0, fWeatherLon = 0; }; struct Sett { SatS sat; };
struct SatT { float fSunElevation = 0; bool bSunElevationValid = false, bSunElevationLocal = false; uint32_t iSunElevLastMs = 0, iSunElevExtMs = 0; };
struct Stt { SatT sat; };
static Sett settings; static Stt state;
// usage: t.exe lat lon  then steps: E:now:elev (external value) | S:now:ntp:epoch (satSunElevationStepAt)
int main(int argc, char** argv) {
  settings.sat.fWeatherLat = (float)atof(argv[1]); settings.sat.fWeatherLon = (float)atof(argv[2]);
  for (int i = 3; i < argc; i++) {
    unsigned long now; int ntp; long long ep; float el;
    if (sscanf(argv[i], "E:%lu:%f", &now, &el) == 2) {
      state.sat.fSunElevation = el; state.sat.bSunElevationValid = true; state.sat.iSunElevLastMs = (uint32_t)now;
      state.sat.iSunElevExtMs = now ? (uint32_t)now : 1; state.sat.bSunElevationLocal = false;
    } else if (sscanf(argv[i], "S:%lu:%d:%lld", &now, &ntp, &ep) == 3) satSunElevationStepAt((uint32_t)now, ntp != 0, ep);
  }
  printf("valid=%d local=%d elev=%.1f\n", (int)state.sat.bSunElevationValid, (int)state.sat.bSunElevationLocal, state.sat.fSunElevation);
  return 0;
}
"""


class SunElevationStep(unittest.TestCase):
    """TASK-1525 review: the staleness rule of an external value lives in satSunElevationStep, not in satUpdateSolarGain."""

    @classmethod
    def setUpClass(cls):
        src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        a, b = src.find("// --- satSunElevation begin"), src.find("// --- satSunElevation end")
        c, d = src.find("// --- satSunElevationStep begin"), src.find("// --- satSunElevationStep end")
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = None
        cls.missing = min(a, b, c, d) < 0
        if cls.missing:
            return
        cpp = Path(cls.tmp.name) / "s.cpp"
        decl, _, rest = STEP_MAIN.partition("// usage")
        cpp.write_text(STUBS + decl + src[a:b] + src[c:d] + "// usage" + rest, encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / "s.exe")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_steps(self, lat, lon, *steps):
        self.assertFalse(self.missing, "satSunElevationStepAt block not found in SATcontrol.ino")
        if not self.exe:
            self.skipTest("no host C++ compiler")
        out = subprocess.run([str(self.exe), str(lat), str(lon), *steps], capture_output=True, text=True, check=True).stdout
        return dict(kv.split("=") for kv in out.split())

    H = 3600000

    def test_stale_external_value_without_local_source_becomes_invalid(self):
        r = self.run_steps(0, 0, "E:1000:30.0", f"S:{1000 + self.H - 1}:0:0")
        self.assertEqual((r["valid"], r["elev"]), ("1", "30.0"), "still fresh just under 1 h")
        r = self.run_steps(0, 0, "E:1000:30.0", f"S:{1000 + self.H + 1}:0:0")
        self.assertEqual(r["valid"], "0")

    def test_stale_external_value_without_ntp_becomes_invalid(self):
        r = self.run_steps(52.37, 4.90, "E:1000:30.0", f"S:{1000 + self.H + 1}:0:0")
        self.assertEqual(r["valid"], "0")

    def test_stale_external_value_is_replaced_by_the_local_one(self):
        r = self.run_steps(52.37, 4.90, "E:1000:30.0", f"S:{1000 + self.H + 1}:1:{utc(2026, 6, 21, 12, 0) - 3600}")
        self.assertEqual((r["valid"], r["local"]), ("1", "1"))
        self.assertNotEqual(r["elev"], "30.0")

    def test_external_value_wins_while_fresh_even_with_a_location(self):
        r = self.run_steps(52.37, 4.90, "E:1000:30.0", f"S:{1000 + self.H - 1}:1:{utc(2026, 6, 21, 12, 0)}")
        self.assertEqual((r["valid"], r["local"], r["elev"]), ("1", "0", "30.0"))



class SourceChecks(unittest.TestCase):
    def test_external_value_wins_while_fresh(self):
        src = SAT_INO.read_text(encoding="utf-8")
        i = src.find("static void satSunElevationStepAt(")
        self.assertGreater(i, 0)
        body = src[i:src.index("\n}\n", i)]
        self.assertRegex(body, r"iSunElevExtMs != 0 && \(uint32_t\)\(now - state\.sat\.iSunElevExtMs\) < 3600000UL\) return;")
        self.assertIn("satSunElevationStep();", src)


if __name__ == "__main__":
    unittest.main()
