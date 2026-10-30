#!/usr/bin/env python3
"""TASK-1336: float settings must survive a write/read round trip in settings.ini.

writeSettings() formatted every float with dtostrf(value, 1, 2, ...), so weather
lat/lon (52.1355 / 6.1827) came back as 52.14 / 6.18 after any settings save.

This test takes the REAL functions out of src/OTGW-firmware/settingStuff.ino
(writeJsonFloatKV, settingsJsonSeparator, parseJsonKVLine, clampSettingFloat),
compiles them with a host compiler behind tiny Arduino stubs (File::printf_P,
dtostrf) and drives them the way writeSettings()/readSettings() do. The decimals
passed per key come from the real writeJsonFloatKV(...) calls in writeSettings();
the clamp range per key comes from the real updateSetting() lines.

For every float key it round-trips the clamp bounds (lo, hi) and a value inside
the range. The bounds matter: a range such as 0.005..0.3 needs 3 decimals.

Point OTGW_SETTINGSTUFF_INO at another copy to check an older revision, e.g.
  git show HEAD:src/OTGW-firmware/settingStuff.ino > old.ino
  OTGW_SETTINGSTUFF_INO=old.ino python tests/test_settings_float_roundtrip.py
(an old copy has no decimals parameter; the driver then calls the 3-argument form).

Needs a host C++ compiler: g++/clang++ on PATH, or MSVC Build Tools (vcvars64.bat).
Skipped when none is found.
Run: python tests/test_settings_float_roundtrip.py
"""
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
INO = Path(os.environ.get("OTGW_SETTINGSTUFF_INO",
                          REPO / "src" / "OTGW-firmware" / "settingStuff.ino"))

# Decimals each float key needs, from the clamp range and the UI/REST input (audit
# table in TASK-1336). Everything not listed is a 2-decimal key.
NEEDED_DECIMALS = {
    "SATweatherlat": 5,    # 52.13550 = ~1 m; the Web UI sends toFixed(4), users type more
    "SATweatherlon": 5,
    "SATthermalcoeff": 4,  # range 0.005..0.3, learned as an EMA
}

HARNESS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <string>
struct __FlashStringHelper;
typedef const char* PGM_P;
#define PSTR(s) (s)
#define F(s) (reinterpret_cast<const __FlashStringHelper*>(s))
#define constrain(a,lo,hi) ((a)<(lo)?(lo):((a)>(hi)?(hi):(a)))
#define DebugTf(...) do{}while(0)
static char* dtostrf(double v, int w, int p, char* out) { snprintf(out, 64, "%*.*f", w, p, v); return out; }
struct File {
  std::string buf;
  void printf_P(const char* fmt, ...) {
    std::string f(fmt);
    for (size_t i = 0; (i = f.find("%S", i)) != std::string::npos; ) f[i + 1] = 's';
    char tmp[256]; va_list ap; va_start(ap, fmt);
    vsnprintf(tmp, sizeof tmp, f.c_str(), ap); va_end(ap); buf += tmp;
  }
};
@@FUNCS@@
int main() {
  char key[64]; double val; int dec; double lo, hi;
  while (scanf("%63s %lf %d %lf %lf", key, &val, &dec, &lo, &hi) == 5) {
    File f; settingsJsonFirstEntry = true;
    @@CALL@@
    char k[64], v[64];
    // writeSettings() puts one key per line; parse that line back like readSettings().
    bool ok = parseJsonKVLine(f.buf.c_str(), k, sizeof k, v, sizeof v);
    float got = ok ? clampSettingFloat(v, 0.0f, (float)lo, (float)hi) : NAN;
    printf("%s %s %.9g\n", key, ok ? v : "PARSE_FAIL", got);
  }
  return 0;
}
"""


def extract(src, start_pat):
    m = re.search(start_pat, src, re.M)
    if not m:
        raise AssertionError("not found in settingStuff.ino: " + start_pat)
    end = src.index("\n}\n", m.start()) + 3
    return src[m.start():end]


def find_compiler():
    for exe in ("g++", "clang++"):
        if shutil.which(exe):
            return ("gnu", exe)
    vc = r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    if os.name == "nt" and os.path.exists(vc):
        return ("msvc", vc)
    return None


class FloatRoundTrip(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cc = find_compiler()
        if not cc:
            raise unittest.SkipTest("no host C++ compiler found")
        src = INO.read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")

        float_fn = extract(src, r"^static void writeJsonFloatKV\(")
        cls.has_decimals = "decimals" in float_fn.split("\n", 1)[0] or "decimals" in float_fn.split("{", 1)[0]
        funcs = "\n".join([
            extract(src, r"^static bool parseJsonKVLine\("),
            "static bool settingsJsonFirstEntry = true;\n"
            + extract(src, r"^static const char\* settingsJsonSeparator\("),
            float_fn,
            extract(src, r"^static float clampSettingFloat\("),
        ])

        # key -> decimals actually passed in writeSettings()
        cls.decimals = {}
        for m in re.finditer(r'writeJsonFloatKV\(file,\s*F\("(\w+)"\),\s*([^;]*?)\);', src):
            tail = re.search(r',\s*(\d+)\s*$', m.group(2))
            cls.decimals[m.group(1)] = int(tail.group(1)) if (tail and cls.has_decimals) else 2
        # key -> clamp range, from updateSetting()
        cls.rng = {}
        for m in re.finditer(r'PSTR\("(\w+)"\)\) == 0\)\s+settings\.[\w.\[\]]+ = clampSettingFloat\(newValue,[^,]+,\s*(-?[\d.]+)f?,\s*(.+?)\);\s*$', src, re.M):
            hi = m.group(3).strip().rstrip("f")
            try:
                hi = float(hi)
            except ValueError:
                hi = 1000.0  # symbolic bound (SAT_DHW_SETPOINT_MAX)
            cls.rng[m.group(1)] = (float(m.group(2)), hi)
        # block form: `else if (... PSTR("key")) == 0) { ...; settings.x = clampSettingFloat(newValue, ...); ... }`
        # (SATtargettemp since TASK-1487). The block ends at the first line that is just `  }`.
        for m in re.finditer(r'PSTR\("(\w+)"\)\) == 0\) \{\n((?:(?!\n  \}).)*)', src, re.S):
            c = re.search(r'settings\.[\w.\[\]]+ = clampSettingFloat\(newValue,[^,]+,\s*(-?[\d.]+)f?,\s*(.+?)\);', m.group(2))
            if not c:
                continue
            hi = c.group(2).strip().rstrip("f")
            try:
                hi = float(hi)
            except ValueError:
                hi = 1000.0
            cls.rng[m.group(1)] = (float(c.group(1)), hi)
        # helper form: `settings.x = parseMaxTSetUser(newValue, ...)` (TASK-1373). It accepts 0 ("not chosen")
        # and values up to SAT_MAXTSET_MAX; the helper's own clampSettingFloat(-1..MAX) lower bound is a
        # guard against negatives, not an accepted value, so the round trip uses 0..SAT_MAXTSET_MAX.
        mx = re.search(r'^#define\s+SAT_MAXTSET_MAX\s+(\d+)', (REPO / "src" / "OTGW-firmware" / "SATtypes.h").read_text(encoding="utf-8", errors="replace"), re.M)
        for m in re.finditer(r'PSTR\("(\w+)"\)\) == 0\)\s+settings\.[\w.\[\]]+ = parseMaxTSetUser\(newValue,', src):
            cls.rng[m.group(1)] = (0.0, float(mx.group(1)))

        call = ("writeJsonFloatKV(f, F(key), (float)val, (uint8_t)dec);" if cls.has_decimals
                else "writeJsonFloatKV(f, F(key), (float)val);")
        code = HARNESS.replace("@@FUNCS@@", funcs).replace("@@CALL@@", call)
        cls.tmp = Path(tempfile.mkdtemp(prefix="otgw_f_"))
        cpp = cls.tmp / "h.cpp"
        cpp.write_text(code)
        exe = cls.tmp / "h.exe"
        if cc[0] == "gnu":
            r = subprocess.run([cc[1], "-std=c++17", str(cpp), "-o", str(exe)],
                               capture_output=True, text=True)
        else:
            bat = cls.tmp / "b.bat"
            bat.write_text('@call "%s" >nul 2>&1\r\ncl /nologo /std:c++17 /EHsc /Fe:h.exe h.cpp\r\n' % cc[1])
            r = subprocess.run(["cmd", "/c", str(bat)], capture_output=True, text=True, cwd=cls.tmp)
        if r.returncode != 0 or not exe.exists():
            raise RuntimeError("harness build failed:\n" + r.stdout + r.stderr)
        cls.exe = exe

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def run_cases(self, cases):
        stdin = "".join("%s %.9g %d %.9g %.9g\n" % c for c in cases)
        r = subprocess.run([str(self.exe)], input=stdin, capture_output=True, text=True, check=True)
        return [l.split() for l in r.stdout.strip().splitlines()]

    def test_weather_coordinates_keep_four_decimals(self):
        out = self.run_cases([
            ("SATweatherlat", 52.1355, self.decimals["SATweatherlat"], -90, 90),
            ("SATweatherlon", 6.1827, self.decimals["SATweatherlon"], -180, 180),
        ])
        print("\n  key, text in file, value read back:", out)
        self.assertAlmostEqual(float(out[0][2]), 52.1355, places=4, msg="lat written as " + out[0][1])
        self.assertAlmostEqual(float(out[1][2]), 6.1827, places=4, msg="lon written as " + out[1][1])

    def test_every_float_key_keeps_its_range_bounds_and_a_midpoint(self):
        self.assertGreater(len(self.decimals), 50, "float key scan found too few keys")
        cases = []
        for key, dec in self.decimals.items():
            lo, hi = self.rng.get(key, (None, None))
            self.assertIsNotNone(lo, key + ": no clamp range found in updateSetting()")
            need = NEEDED_DECIMALS.get(key, 2)
            mid = round(lo + (hi - lo) * 0.3137, need) if hi < 1000 else round(lo + 7.31, need)
            for v in (lo, mid, hi if hi < 1000 else lo):
                cases.append((key, v, dec, lo, hi))
        bad = []
        for (key, v, dec, _, _), (_, written, got) in zip(cases, self.run_cases(cases)):
            if abs(float(got) - v) > 1e-6 + abs(v) * 1e-6:
                bad.append("%s: %s -> file '%s' -> %s (decimals=%d)" % (key, v, written, got, dec))
        self.assertEqual(bad, [], "values changed across write/read:\n  " + "\n  ".join(bad))


if __name__ == "__main__":
    unittest.main(verbosity=2)
