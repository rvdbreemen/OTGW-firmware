#!/usr/bin/env python3
"""TASK-1525 (ADR-205): host test for the SAT heating curve self-tune.

The code under test is NOT re-implemented here. The test cuts the two marked blocks out of
src/OTGW-firmware/SATcontrol.ino and compiles them as they are with a host C++ compiler (g++, clang++ or MSVC cl via
vswhere):
  - "satCurveTune begin/end (pure)": the hour accumulation, the exclusions, the day close and the decision;
  - "satCurveTune glue begin/end":   the tuner clock (satCurveTuneService), the log records, the write of the
                                     coefficient through updateSetting() and the manual-change hook.
Only the surroundings are stubbed: settings/state structs, updateSetting() (which mirrors the SATcoefficient branch of
settingStuff.ino: clamp, count a write when the value changed, call satCurveTuneNoteManualChange()), the log sinks.
Without a compiler the tests are skipped, they do not pass silently.

OTGW_SAT_INO points at another copy, e.g. `git show HEAD:src/OTGW-firmware/SATcontrol.ino`, for the old-vs-fix run. HEAD has
no tuner, so setUpClass fails there (red).

Not reachable from here: the two call sites in satControlLoopRun() (they need the whole control loop). Source-level checks at
the end pin what they pass (signal = clamp(pidRaw) - curve, the hot-water hook); the bench run covers the rest.

Run: python tests/test_sat_curve_selftune.py
"""
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SRC = REPO / "src" / "OTGW-firmware"
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", SRC / "SATcontrol.ino"))

STUBS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cmath>
#define F(x) x
#define PSTR(x) x
#define snprintf_P snprintf
typedef const char* PGM_P;
static char g_log[400000]; static size_t g_logn = 0;
static void g_append(const char* prefix, const char* fmt, va_list ap) {
  g_logn += (size_t)snprintf(g_log + g_logn, sizeof(g_log) - g_logn, "%s", prefix);
  g_logn += (size_t)vsnprintf(g_log + g_logn, sizeof(g_log) - g_logn, fmt, ap);
  if (g_logn > 0 && g_log[g_logn - 1] != '\n') { g_log[g_logn++] = '\n'; g_log[g_logn] = 0; }
}
static void g_logf(const char* fmt, ...) { va_list ap; va_start(ap, fmt); g_append("", fmt, ap); va_end(ap); }
static void satNarratef_P(PGM_P fmt, ...) { va_list ap; va_start(ap, fmt); g_append("NARR ", fmt, ap); va_end(ap); }
#define SATDebugTf(...) g_logf(__VA_ARGS__)
static size_t strlcpy(char* d, const char* s, size_t n) { size_t l = strlen(s); if (n) { size_t c = l >= n ? n - 1 : l; memcpy(d, s, c); d[c] = 0; } return l; }

struct SatSettings { bool bCurveSelfTune = true, bSimulation = false; float fHeatingCurveCoeff = 2.0f; uint32_t iControlInterval = 3600; };
struct Settings { SatSettings sat; };
struct SatState { bool bActive = true, bSafetyTripped = false; float fPidI = 7.5f, fPidD = -1.25f; };
struct State { SatState sat; };
static Settings settings; static State state;
static bool g_satEnabled = true;
static bool satIsEnabled() { return g_satEnabled; }
static int g_writes = 0;
void satCurveTuneNoteManualChange();
// The window file: a real file named by the environment, so a second run of the exe is a reboot on the same flash.
static int g_fileWrites = 0, g_fileRemoves = 0;
static const char* g_tuneFile() { const char* p = getenv("SATTUNE_FILE"); return p ? p : "sat_tune_test.json"; }
static bool satTuneFileRead(char* buf, size_t n) {
  FILE* f = fopen(g_tuneFile(), "rb"); if (!f) return false;
  size_t len = fread(buf, 1, n - 1, f); buf[len] = 0; fclose(f); return true;
}
static void satTuneFileWrite(const char* json) { FILE* f = fopen(g_tuneFile(), "wb"); if (!f) return; fputs(json, f); fclose(f); g_fileWrites++; }
static void satTuneFileRemove() { if (remove(g_tuneFile()) == 0) g_fileRemoves++; }
// Mirrors the SATcoefficient branch of settingStuff.ino updateSetting(): clamp 0.1..5.0; a changed value is one
// (debounced) settings.ini write and, unless the tuner is writing, empties the self-tune window.
static void updateSetting(const char* field, const char* v) {
  if (strcmp(field, "SATcoefficient")) return;
  float before = settings.sat.fHeatingCurveCoeff;
  float x = (float)atof(v); if (x < 0.1f) x = 0.1f; if (x > 5.0f) x = 5.0f;
  settings.sat.fHeatingCurveCoeff = x;
  if (x != before) { g_writes++; satCurveTuneNoteManualChange(); }
}
"""

MAIN = r"""
static uint32_t g_now = 0;
static SatTuneTickIn g_def() {
  SatTuneTickIn t; memset(&t, 0, sizeof(t));
  t.satActive = true; t.sunValid = true; t.sunElevation = 5.0f; t.chOn = true; t.flameOn = true;
  t.relMod = 40.0f; t.minMod = 20.0f; t.signal = 1.5f;
  return t;
}
static void tick(const char* spec) {
  SatTuneTickIn t = g_def();
  char buf[256]; strncpy(buf, spec, sizeof(buf) - 1); buf[sizeof(buf) - 1] = 0;
  for (char* p = strtok(buf, ","); p; p = strtok(nullptr, ",")) {
    char* eq = strchr(p, '='); if (!eq) continue; *eq = 0; float v = (float)atof(eq + 1);
    if (!strcmp(p, "act")) t.satActive = v != 0; else if (!strcmp(p, "dhw")) t.dhw = v != 0;
    else if (!strcmp(p, "sunv")) t.sunValid = v != 0; else if (!strcmp(p, "sun")) t.sunElevation = v;
    else if (!strcmp(p, "pwm")) t.pwm = v != 0; else if (!strcmp(p, "ch")) t.chOn = v != 0;
    else if (!strcmp(p, "flame")) t.flameOn = v != 0; else if (!strcmp(p, "rm")) t.relMod = v;
    else if (!strcmp(p, "mm")) t.minMod = v; else if (!strcmp(p, "sig")) t.signal = v;
    else if (!strcmp(p, "room")) t.roomLost = v != 0;
  }
  satCurveTuneNoteTick(t);
}
// tokens on stdin: T:k=v,.. tick | A:ms advance + service | S service | M:x manual coefficient (updateSetting) | C:x set
// coefficient silently | tune:0/1 | sim:0/1 | hourS:n | act:0/1 (state.sat.bActive) | sat:0/1 (SAT enabled)
int main() {
  char tok[300];
  while (scanf("%299s", tok) == 1) {
    if (tok[0] == 'T' && tok[1] == ':') tick(tok + 2);
    else if (!strncmp(tok, "A:", 2)) { g_now += (uint32_t)atol(tok + 2); satCurveTuneService(g_now); }
    else if (!strcmp(tok, "S")) satCurveTuneService(g_now);
    else if (!strncmp(tok, "M:", 2)) updateSetting("SATcoefficient", tok + 2);
    else if (!strncmp(tok, "C:", 2)) settings.sat.fHeatingCurveCoeff = (float)atof(tok + 2);
    else if (!strncmp(tok, "tune:", 5)) settings.sat.bCurveSelfTune = atoi(tok + 5) != 0;
    else if (!strncmp(tok, "sim:", 4)) settings.sat.bSimulation = atoi(tok + 4) != 0;
    else if (!strncmp(tok, "hourS:", 6)) satCurveTuneSetHourS((uint32_t)atol(tok + 6));
    else if (!strncmp(tok, "act:", 4)) state.sat.bActive = atoi(tok + 4) != 0;
    else if (!strncmp(tok, "sat:", 4)) g_satEnabled = atoi(tok + 4) != 0;
    else if (!strncmp(tok, "int:", 4)) settings.sat.iControlInterval = (uint32_t)atol(tok + 4);
    else if (!strcmp(tok, "NC")) satCurveTuneNoteNotControlling();
  }
  printf("RESULT coeff=%.2f writes=%d pidI=%.2f pidD=%.2f fw=%d fr=%d\n", settings.sat.fHeatingCurveCoeff, g_writes,
         state.sat.fPidI, state.sat.fPidD, g_fileWrites, g_fileRemoves);
  fputs(g_log, stdout);
  return 0;
}
"""

BLOCKS = (
    (r"// --- satCurveTune begin \(pure.*?\n(.*?)// --- satCurveTune end \(pure\)", "pure"),
    (r"// --- satCurveTune glue begin.*?\n(.*?)// --- satCurveTune glue end", "glue"),
)


def extract(src: str) -> str:
    out = []
    for rx, name in BLOCKS:
        m = re.search(rx, src, re.S)
        if not m:
            raise AssertionError(f"no satCurveTune {name} block in {SAT_INO}: the tuner is not there")
        out.append(m.group(1))
    return "\n".join(out)


def _vcvars():
    vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / \
        "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.exists():
        return None
    out = subprocess.run([str(vswhere), "-latest", "-products", "*", "-requires",
                          "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                          "-property", "installationPath"],
                         capture_output=True, text=True).stdout.strip()
    bat = Path(out) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat" if out else None
    return bat if bat and bat.exists() else None


def compile_cpp(cpp: Path, exe: Path):
    for cc in ("g++", "clang++"):
        if shutil.which(cc):
            subprocess.run([cc, "-std=c++17", "-o", str(exe), str(cpp)], check=True)
            return exe
    bat = _vcvars()
    if bat:
        cmd = f'call "{bat}" >nul && cl /nologo /std:c++17 /EHsc /Fe:"{exe}" /Fo:"{cpp.parent}\\\\" "{cpp}"'
        r = subprocess.run(cmd, shell=True, cwd=cpp.parent, capture_output=True, text=True)
        if r.returncode:
            raise AssertionError("host compile failed:\n" + r.stdout[-3000:] + r.stderr[-1000:])
        return exe
    return None


def build(workdir: Path, body: str = None, name: str = "t", prefix: str = ""):
    body = body if body is not None else extract(SAT_INO.read_text(encoding="utf-8"))
    cpp = workdir / f"{name}.cpp"
    cpp.write_text(STUBS + prefix + body + MAIN, encoding="utf-8")
    return compile_cpp(cpp, workdir / f"{name}.exe")


# --- scenario helpers -----------------------------------------------------------------------------------------------
HOUR = 3600000


def tk(**kw):
    return "T:" + ",".join(f"{k}={v}" for k, v in kw.items()) if kw else "T:"


def clean_hour(sig, n=4):
    return [tk(sig=sig)] * n + [f"A:{HOUR}"]


def excluded_hour(sig_clean, excl, n_clean=3):
    """3 clean ticks and 1 tick with the exclusion (and a big signal that must not leak into any mean)."""
    return [tk(sig=sig_clean)] * n_clean + [tk(sig=9.0, **EXCL[excl]), f"A:{HOUR}"]


EXCL = {
    "not_sat":   dict(act=0),                  # a tick while SAT is not the active controller
    "dhw":       dict(dhw=1),
    "no_sun":    dict(sunv=0),
    "sun":       dict(sun=12.0),               # boundary: 12 degrees and above is excluded
    "pwm":       dict(pwm=1),
    "flame_off": dict(ch=1, flame=0),
    "min_mod":   dict(rm=4.0, mm=0),           # MsgID 15 never seen: 5 % default, 4 % is below it
}
ALL_EXCL = list(EXCL)


def day(sig, clean=24, excl=None, excl_sig_clean=None):
    """A day of 24 hours: `clean` clean hours at signal `sig`, the rest excluded by `excl`."""
    toks = []
    for h in range(24):
        if h < clean:
            toks += clean_hour(sig)
        else:
            toks += excluded_hour(sig if excl_sig_clean is None else excl_sig_clean, excl)
    return toks


class Out:
    def __init__(self, text):
        lines = text.splitlines()
        r = lines[0]
        self.coeff = float(re.search(r"coeff=(\S+)", r).group(1))
        self.writes = int(re.search(r"writes=(\d+)", r).group(1))
        self.pidI = float(re.search(r"pidI=(\S+)", r).group(1))
        self.pidD = float(re.search(r"pidD=(\S+)", r).group(1))
        self.fw = int(re.search(r"fw=(\d+)", r).group(1))
        self.fr = int(re.search(r"fr=(\d+)", r).group(1))
        self.log = lines[1:]
        kv = lambda ln: dict(re.findall(r"(\w+)=(\S+)", ln))
        self.hours = [kv(l) for l in self.log if l.startswith("SATTUNEH ")]
        self.days = [kv(l) for l in self.log if l.startswith("SATTUNED ")]
        self.changes = [l for l in self.log if l.startswith("SATTUNE ")]
        self.narr = [l for l in self.log if l.startswith("NARR ")]


class SatCurveSelfTune(unittest.TestCase):
    runs = 0
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build(Path(cls.tmp.name))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_tokens(self, tokens, exe=None, file=None):
        # a fresh flash for every run unless the test passes the file of an earlier run (a reboot)
        SatCurveSelfTune.runs += 1
        file = file or Path(self.tmp.name) / f"run{SatCurveSelfTune.runs}.json"
        env = dict(os.environ, SATTUNE_FILE=str(file))
        pre = []
        while tokens and re.match(r"(C|sim|int):", tokens[0]):   # settings that must be in place before the tuner starts
            pre.append(tokens[0])
            tokens = tokens[1:]
        r = subprocess.run([str(exe or self.exe)], input="\n".join(pre + ["S"] + tokens), capture_output=True,
                           text=True, check=True, env=env)
        return Out(r.stdout)

    def setUp(self):
        if not self.exe:
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")

    # --- AC1: too low a curve raises by exactly 0.1 after 3 counted days -------------------------------------------
    def test_too_low_curve_raises_by_exactly_one_tenth_after_three_counted_days(self):
        o = self.run_tokens(day(1.5) * 2)
        self.assertEqual((o.coeff, o.writes, o.changes), (2.0, 0, []), "no change before the third counted day")
        self.assertEqual([d["decision"] for d in o.days], ["hold", "hold"])
        self.assertEqual([d["reason"] for d in o.days], ["few_days", "few_days"])
        o = self.run_tokens(day(1.5) * 3)
        self.assertEqual((o.coeff, o.writes), (2.1, 1))
        self.assertEqual([d["decision"] for d in o.days], ["hold", "hold", "raise"])
        self.assertEqual(o.days[2]["mean3"], "1.50")
        self.assertEqual(o.days[2]["days_in_window"], "3")
        self.assertEqual(o.days[2]["reason"], "outside_band")

    def test_too_high_curve_lowers_by_exactly_one_tenth(self):
        o = self.run_tokens(day(-1.5) * 3)
        self.assertEqual((o.coeff, o.writes), (1.9, 1))
        self.assertEqual(o.days[2]["decision"], "lower")

    def test_uncounted_day_does_not_fill_the_window(self):
        # counted, uncounted (5 clean hours, big signal), counted: still no change; the fourth day (counted) completes it
        u = day(3.0, clean=5, excl="dhw", excl_sig_clean=3.0)
        o = self.run_tokens(day(1.5) + u + day(1.5))
        self.assertEqual((o.coeff, o.writes), (2.0, 0))
        self.assertEqual(o.days[1]["counted"], "0")
        self.assertEqual(o.days[1]["reason"], "day_not_counted")
        o = self.run_tokens(day(1.5) + u + day(1.5) + day(1.5))
        self.assertEqual((o.coeff, o.writes), (2.1, 1))

    def test_hour_weighted_mean_over_the_window(self):
        # day mean would be (3 + 3 - 0.5) / 3 = 1.83 (raise); hour-weighted is (18 + 18 - 12) / 36 = 0.67 (hold)
        a = day(3.0, clean=6, excl="dhw", excl_sig_clean=3.0)
        o = self.run_tokens(a + a + day(-0.5))
        self.assertEqual((o.coeff, o.writes), (2.0, 0))
        self.assertEqual(o.days[2]["mean3"], "0.67")

    # --- AC2: every exclusion keeps an hour out -------------------------------------------------------------------
    def test_each_exclusion_makes_the_hour_not_count(self):
        for ex in ALL_EXCL:
            with self.subTest(exclusion=ex):
                o = self.run_tokens(excluded_hour(0.5, ex) + clean_hour(0.5))
                self.assertEqual((o.hours[0]["counted"], o.hours[0]["reason"]), ("0", ex))
                self.assertEqual((o.hours[1]["counted"], o.hours[1]["reason"]), ("1", "none"))

    def test_service_marks_an_hour_not_sat_while_sat_is_not_active_or_tripped(self):
        o = self.run_tokens([tk(sig=0.5), "act:0", "S", "act:1", f"A:{HOUR}", tk(sig=0.5), f"A:{HOUR}"])
        self.assertEqual([(h["counted"], h["reason"]) for h in o.hours], [("0", "not_sat"), ("1", "none")])

    def test_minimum_modulation_equal_to_the_minimum_is_excluded(self):
        o = self.run_tokens([tk(rm=20.0, mm=20), f"A:{HOUR}", tk(rm=20.1, mm=20), f"A:{HOUR}",
                             tk(rm=5.0, mm=0), f"A:{HOUR}", tk(rm=5.1, mm=0), f"A:{HOUR}"])
        self.assertEqual([h["counted"] for h in o.hours], ["0", "1", "0", "1"])
        self.assertEqual([h["reason"] for h in o.hours], ["min_mod", "none", "min_mod", "none"])

    def test_sun_boundary(self):
        o = self.run_tokens([tk(sun=11.9), f"A:{HOUR}", tk(sun=12.0), f"A:{HOUR}"])
        self.assertEqual([h["counted"] for h in o.hours], ["1", "0"])

    def test_flame_off_counts_only_while_ch_is_enabled(self):
        o = self.run_tokens([tk(ch=0, flame=0), f"A:{HOUR}", tk(ch=1, flame=0), f"A:{HOUR}"])
        self.assertEqual([(h["counted"], h["reason"]) for h in o.hours], [("1", "none"), ("0", "flame_off")])

    def test_hour_without_ticks_does_not_count(self):
        o = self.run_tokens([f"A:{HOUR}"])
        self.assertEqual((o.hours[0]["counted"], o.hours[0]["reason"], o.hours[0]["ticks"], o.hours[0]["mean"]),
                         ("0", "no_ticks", "0", "na"))

    def test_first_reason_of_the_hour_is_reported(self):
        o = self.run_tokens([tk(sunv=0), tk(dhw=1), f"A:{HOUR}"])
        self.assertEqual(o.hours[0]["reason"], "no_sun")

    def test_excluded_hours_never_reach_the_decision(self):
        # 6 clean hours at +0.5 and 18 excluded hours at +9 each day, three days: inside the band, no change.
        # With any one exclusion removed the +9 hours count and the coefficient moves (see the mutation test).
        for ex in ALL_EXCL:
            with self.subTest(exclusion=ex):
                o = self.run_tokens(day(0.5, clean=6, excl=ex) * 3)
                self.assertEqual((o.coeff, o.writes), (2.0, 0))
                self.assertEqual(o.days[2]["clean"], "6")
                self.assertEqual((o.days[2]["mean3"], o.days[2]["decision"], o.days[2]["reason"]),
                                 ("0.50", "hold", "inside_band"))

    def test_a_day_needs_six_clean_hours(self):
        o = self.run_tokens(day(1.5, clean=5, excl="dhw") + day(1.5, clean=6, excl="dhw"))
        self.assertEqual([(d["clean"], d["counted"]) for d in o.days], [("5", "0"), ("6", "1")])

    def test_coefficient_change_in_the_hour_excludes_it(self):
        o = self.run_tokens([tk(sig=0.5), "M:2.5", tk(sig=0.5), f"A:{HOUR}"])
        self.assertEqual((o.hours[0]["counted"], o.hours[0]["reason"]), ("0", "coeff_changed"))

    # --- AC3: band, step, once a day, clamp, manual change, PID ---------------------------------------------------
    def test_inside_the_band_nothing_changes(self):
        for sig in (1.0, 0.99, -1.0, -0.99, 0.0):
            with self.subTest(signal=sig):
                o = self.run_tokens(day(sig) * 6)
                self.assertEqual((o.coeff, o.writes, o.changes), (2.0, 0, []))
                self.assertTrue(all(d["decision"] == "hold" for d in o.days))

    def test_just_outside_the_band_changes(self):
        self.assertEqual(self.run_tokens(day(1.01) * 3).coeff, 2.1)
        self.assertEqual(self.run_tokens(day(-1.01) * 3).coeff, 1.9)

    def test_at_most_one_step_a_day_and_the_window_empties_after_a_change(self):
        o = self.run_tokens(day(3.0) * 8)
        changes = [i for i, d in enumerate(o.days) if d["decision"] == "raise"]
        self.assertEqual(changes, [2, 5], "a change on day 3 and day 6 only: three new counted days after each change")
        self.assertEqual((o.coeff, o.writes), (2.2, 2))
        self.assertEqual([d["days_in_window"] for d in o.days][:6], ["1", "2", "3", "1", "2", "3"])

    def test_clamp_at_both_ends(self):
        o = self.run_tokens(["C:5.0"] + day(3.0) * 3)
        self.assertEqual((o.coeff, o.writes), (5.0, 0))
        self.assertEqual((o.days[2]["decision"], o.days[2]["reason"]), ("hold", "at_limit"))
        o = self.run_tokens(["C:0.1"] + day(-3.0) * 3)
        self.assertEqual((o.coeff, o.writes), (0.1, 0))
        self.assertEqual(o.days[2]["reason"], "at_limit")

    def test_step_that_would_pass_the_limit_lands_on_it(self):
        o = self.run_tokens(["C:4.95"] + day(3.0) * 3)
        self.assertEqual((o.coeff, o.writes), (5.0, 1))
        o = self.run_tokens(["C:0.15"] + day(-3.0) * 3)
        self.assertEqual((o.coeff, o.writes), (0.1, 1))

    def test_manual_change_empties_the_window(self):
        # two counted days at +3, then a manual change in the first hour of day 3: without the reset day 3 would raise
        o = self.run_tokens(day(3.0) * 2 + ["M:2.5"] + day(3.0) * 2)
        self.assertEqual([d["decision"] for d in o.days], ["hold", "hold", "hold", "hold"])
        self.assertEqual((o.coeff, o.writes), (2.5, 1))
        o = self.run_tokens(day(3.0) * 2 + ["M:2.5"] + day(3.0) * 3)
        self.assertEqual([d["decision"] for d in o.days][-1], "raise")
        self.assertEqual((o.coeff, o.writes), (2.6, 2))
        self.assertEqual(o.days[2]["days_in_window"], "1", "the day of the manual change is the first of the new window")

    def test_manual_change_to_the_same_value_does_nothing(self):
        o = self.run_tokens(day(3.0) * 2 + ["M:2.0"] + day(3.0))
        self.assertEqual(o.days[2]["decision"], "raise")
        self.assertEqual(o.writes, 1)   # only the tuner's own write

    def test_tuner_step_leaves_the_pid_terms_alone(self):
        o = self.run_tokens(day(3.0) * 3)
        self.assertEqual(o.coeff, 2.1)
        self.assertEqual((o.pidI, o.pidD), (7.5, -1.25))

    # --- AC4: settings.ini written only on a change -----------------------------------------------------------------
    def test_coefficient_written_only_when_it_changes(self):
        o = self.run_tokens(day(0.5) * 5 + day(-0.5) * 5)
        self.assertEqual((o.coeff, o.writes), (2.0, 0))
        o = self.run_tokens(day(3.0) * 9)
        self.assertEqual((o.coeff, o.writes), (2.3, 3))
        self.assertEqual(len(o.changes), 3)

    def test_tuner_change_does_not_empty_its_own_window_twice_or_flag_a_manual_change(self):
        # the tuner's change at day 3 must not count as a manual change: day 4 is the first day of the new window
        o = self.run_tokens(day(3.0) * 4)
        self.assertEqual(o.days[3]["days_in_window"], "1")
        self.assertEqual(o.days[3]["counted"], "1")
        self.assertEqual([h["reason"] for h in o.hours[72:96]], ["none"] * 24, "no hour after the change is flagged")

    # --- AC5 (core): on/off ---------------------------------------------------------------------------------------
    def test_switched_off_keeps_the_coefficient_and_logs_nothing(self):
        o = self.run_tokens(["tune:0"] + day(3.0) * 5)
        self.assertEqual((o.coeff, o.writes, o.hours, o.days, o.changes), (2.0, 0, [], [], []))

    def test_switched_on_again_starts_with_an_empty_window(self):
        o = self.run_tokens(day(3.0) * 2 + ["tune:0"] + day(3.0) + ["tune:1", "S"] + day(3.0) * 2)
        self.assertEqual(o.coeff, 2.0, "2 + 2 counted days, but never 3 in a row after the restart")
        o = self.run_tokens(day(3.0) * 2 + ["tune:0"] + day(3.0) + ["tune:1", "S"] + day(3.0) * 3)
        self.assertEqual((o.coeff, o.writes), (2.1, 1))

    def test_sat_off_stops_the_tuner(self):
        o = self.run_tokens(["sat:0"] + day(3.0) * 4)
        self.assertEqual((o.coeff, o.hours), (2.0, []))

    # --- AC6: records -----------------------------------------------------------------------------------------------
    def test_record_formats(self):
        o = self.run_tokens(day(1.5) * 3)
        self.assertRegex(o.log[0], r"^SATTUNEH hour=0 mean=1\.50 ticks=4 counted=1 reason=none$")
        daily = [l for l in o.log if l.startswith("SATTUNED ")]
        self.assertRegex(daily[0], r"^SATTUNED day=0 clean=24 counted=1 days_in_window=1 mean3=na decision=hold reason=few_days$")
        self.assertRegex(daily[2], r"^SATTUNED day=2 clean=24 counted=1 days_in_window=3 mean3=1\.50 decision=raise reason=outside_band$")
        self.assertEqual(o.changes, ["SATTUNE old=2.0 new=2.1 mean3=1.50"])
        self.assertEqual(o.narr, ["NARR Heating curve self-tune: coefficient 2.0 -> 2.1 (3-day mean +1.50 C above the curve)"])
        o = self.run_tokens(day(-1.5) * 3)
        self.assertEqual(o.narr, ["NARR Heating curve self-tune: coefficient 2.0 -> 1.9 (3-day mean -1.50 C below the curve)"])

    def test_hour_record_of_an_excluded_hour_keeps_its_mean(self):
        o = self.run_tokens([tk(sig=2.0), tk(sig=4.0, pwm=1), f"A:{HOUR}"])
        self.assertEqual((o.hours[0]["mean"], o.hours[0]["ticks"], o.hours[0]["reason"]), ("3.00", "2", "pwm"))

    # --- review round: not controlling, few ticks, simulation switch, window file, manual day counters, no_room -----------
    def test_early_return_paths_make_the_hour_not_count(self):
        o = self.run_tokens([tk(sig=0.5), "NC", tk(sig=0.5), f"A:{HOUR}", tk(sig=0.5), f"A:{HOUR}"])
        self.assertEqual([(h["counted"], h["reason"]) for h in o.hours], [("0", "not_controlling"), ("1", "none")])

    def test_first_reason_is_first_in_time(self):
        o = self.run_tokens([tk(pwm=1), "NC", f"A:{HOUR}", "NC", tk(pwm=1), f"A:{HOUR}"])
        self.assertEqual([h["reason"] for h in o.hours], ["pwm", "not_controlling"])

    def test_hour_needs_75_percent_of_the_expected_ticks(self):
        # control interval 900 s: 4 ticks expected in 3600 s, 3 needed (ceil(4 * 0.75))
        o = self.run_tokens(["int:900"] + [tk(sig=0.5)] * 3 + [f"A:{HOUR}"] + [tk(sig=0.5)] * 2 + [f"A:{HOUR}"])
        self.assertEqual([(h["counted"], h["reason"]) for h in o.hours], [("1", "none"), ("0", "few_ticks")])
        o = self.run_tokens(["int:30"] + [tk(sig=0.5)] * 89 + [f"A:{HOUR}"] + [tk(sig=0.5)] * 90 + [f"A:{HOUR}"])
        self.assertEqual([(h["counted"], h["reason"]) for h in o.hours], [("0", "few_ticks"), ("1", "none")],
                         "120 expected at 30 s, 90 needed")

    def test_few_ticks_does_not_hide_an_earlier_reason(self):
        o = self.run_tokens(["int:900", tk(pwm=1), f"A:{HOUR}"])
        self.assertEqual(o.hours[0]["reason"], "pwm")

    def test_simulation_switch_restarts_the_tuner_with_an_empty_window(self):
        o = self.run_tokens(day(3.0) * 2 + ["sim:1", "S"] + day(3.0))
        self.assertEqual((o.coeff, o.writes, o.changes), (2.0, 0, []), "2 real days + 1 simulated day never make 3")
        self.assertEqual(o.days[2]["days_in_window"], "1")
        o = self.run_tokens(day(3.0) * 2 + ["sim:1", "S", "sim:0", "S"] + day(3.0))
        self.assertEqual((o.coeff, o.writes), (2.0, 0))
        self.assertEqual(o.days[2]["days_in_window"], "1")

    def test_window_file_is_written_at_the_daily_decision_only(self):
        o = self.run_tokens(day(1.5) * 2)
        self.assertEqual((o.fw, o.fr), (2, 0), "one write per day, none on an ordinary hour")
        f = Path(self.tmp.name) / "daily.json"
        o = self.run_tokens(day(1.5) + [tk(sig=1.5), f"A:{HOUR}"] * 10, file=f)
        self.assertEqual(o.fw, 1, "ten ordinary hours after the day write nothing")
        self.assertRegex(f.read_text(), r'^\{"coeff":2\.00,"n":1,"c0":24,"s0":36\.000\}$')

    def test_window_is_restored_after_a_reboot(self):
        f = Path(self.tmp.name) / "x.json"
        self.run_tokens(day(1.5) * 2, file=f)
        self.assertRegex(f.read_text(), r'"n":2')
        o = self.run_tokens(day(1.5), file=f)          # second run = reboot: one more counted day completes the window
        self.assertEqual((o.coeff, o.writes), (2.1, 1))
        self.assertEqual(o.days[0]["days_in_window"], "3")
        self.assertEqual(o.days[0]["mean3"], "1.50")
        self.assertTrue(any(l.startswith("SATTUNER restored=2") for l in o.log))

    def test_window_is_dropped_when_the_coefficient_differs(self):
        f = Path(self.tmp.name) / "y.json"
        self.run_tokens(day(1.5) * 2, file=f)
        o = self.run_tokens(["C:2.5"] + day(1.5), file=f)
        self.assertEqual((o.coeff, o.writes), (2.5, 0))
        self.assertEqual(o.days[0]["days_in_window"], "1")
        self.assertTrue(any(l.startswith("SATTUNER restored=0") for l in o.log))

    def test_window_file_is_not_restored_in_simulation_or_when_switched_on_later(self):
        f = Path(self.tmp.name) / "z.json"
        self.run_tokens(day(1.5) * 2, file=f)
        o = self.run_tokens(["sim:1", "S"] + day(1.5), file=f)
        self.assertEqual(o.days[0]["days_in_window"], "1")
        self.assertEqual(o.fw, 0, "simulation never writes the file")

    def test_manual_change_and_switch_off_remove_the_file(self):
        f = Path(self.tmp.name) / "w.json"
        self.run_tokens(day(1.5), file=f)
        self.assertTrue(f.exists())
        o = self.run_tokens(["M:2.5", f"A:{HOUR}"], file=f)
        self.assertEqual(o.fr, 1)
        self.assertFalse(f.exists())

    def test_empty_window_has_no_file(self):
        f = Path(self.tmp.name) / "v.json"
        o = self.run_tokens(day(3.0) * 3, file=f)       # the change empties the window
        self.assertEqual((o.coeff, o.fw, o.fr), (2.1, 2, 1))
        self.assertFalse(f.exists())

    def test_manual_change_resets_the_day_counters(self):
        # 20 clean hours at +3, a manual change, then 4 clean hours: without the day-counter reset the day would have 24
        # clean hours (one the changed hour is excluded -> 23); with it only the 4 clean hours after the change remain and
        # the day does not count (< 6)
        toks = []
        for _ in range(20):
            toks += clean_hour(3.0)
        toks += ["M:2.5"] + clean_hour(3.0)          # the hour of the change: coeff_changed
        for _ in range(3):
            toks += clean_hour(3.0)
        o = self.run_tokens(toks)
        self.assertEqual(o.days[0]["clean"], "3")
        self.assertEqual(o.days[0]["counted"], "0")

    def test_room_lost_hours_count_by_default(self):
        o = self.run_tokens([tk(sig=0.0, room=1), f"A:{HOUR}"])
        self.assertEqual((o.hours[0]["counted"], o.hours[0]["reason"]), ("1", "none"))

    def test_room_lost_hours_are_excluded_when_the_constant_is_set(self):
        exe = build(Path(self.tmp.name), name="room1", prefix="#define SAT_CT_EXCLUDE_ROOM_LOST 1\n")
        o = self.run_tokens([tk(sig=0.5), tk(sig=0.0, room=1), f"A:{HOUR}", tk(sig=0.5), f"A:{HOUR}"], exe=exe)
        self.assertEqual([(h["counted"], h["reason"]) for h in o.hours], [("0", "no_room"), ("1", "none")])

    # --- simulation-only hour length -----------------------------------------------------------------------------
    def test_hour_is_3600_s_outside_simulation(self):
        o = self.run_tokens(["hourS:60", "S", tk(), "A:60000", "A:60000"])
        self.assertEqual(o.hours, [], "hourS is ignored while simulation is off")
        o = self.run_tokens(["hourS:60", "S", tk(), "A:3600000"])
        self.assertEqual(len(o.hours), 1)

    def test_hour_can_be_shortened_in_simulation_down_to_60_s(self):
        o = self.run_tokens(["sim:1", "hourS:60", "S", tk(), "A:60000", tk(), "A:60000"])
        self.assertEqual(len(o.hours), 2)
        o = self.run_tokens(["sim:1", "hourS:10", "S", tk(), "A:30000", "A:30000"])   # clamped up to 60 s
        self.assertEqual(len(o.hours), 1)
        o = self.run_tokens(["sim:1", "hourS:0", "S", tk(), "A:60000"])               # 0 restores 3600 s
        self.assertEqual(o.hours, [])

    def test_four_simulated_days_in_a_few_minutes_show_a_change(self):
        # 60 s hours: 4 days = 96 min. Four days of +3: the change comes after day 3, days 4 starts a new window.
        toks = ["sim:1", "hourS:60", "S"]
        for _ in range(4 * 24):
            toks += [tk(sig=3.0)] * 2 + ["A:60000"]
        o = self.run_tokens(toks)
        self.assertEqual(len(o.hours), 96)
        self.assertEqual(len(o.days), 4)
        self.assertEqual(len(o.changes), 1)
        self.assertEqual(o.coeff, 2.1)


class ExclusionMutation(unittest.TestCase):
    """Removing any one exclusion from the real source must make the per-exclusion decision test fail (red)."""

    LINES = {
        "not_sat":   r"  if \(!in\.satActive\)\s+return SATT_NOT_SAT;",
        "dhw":       r"  if \(in\.dhw\)\s+return SATT_DHW;",
        "no_sun":    r"  if \(!in\.sunValid\)\s+return SATT_NO_SUN;",
        "sun":       r"  if \(in\.sunElevation >= SAT_CT_SUN_MAX_DEG\)\s+return SATT_SUN;",
        "pwm":       r"  if \(in\.pwm\)\s+return SATT_PWM;",
        "flame_off": r"  if \(in\.chOn && !in\.flameOn\)\s+return SATT_FLAME_OFF;",
        "min_mod":   r"  if \(!\(in\.relMod > minMod\)\)\s+return SATT_MIN_MOD;[^\n]*",
    }

    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.body = extract(SAT_INO.read_text(encoding="utf-8"))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_removing_one_exclusion_is_caught(self):
        for ex, rx in self.LINES.items():
            with self.subTest(removed=ex):
                self.assertEqual(len(re.findall(rx, self.body)), 1, f"line of {ex} not found once")
                exe = build(Path(self.tmp.name), re.sub(rx, "", self.body), name=f"mut_{ex}")
                if not exe:
                    self.skipTest("no host C++ compiler")
                tokens = ["S"] + day(0.5, clean=6, excl=ex) * 3
                out = Out(subprocess.run([str(exe)], input="\n".join(tokens), capture_output=True, text=True,
                                         check=True).stdout)
                # a tick of dhw / not_sat carries no signal, so its removal shows in the clean-hour count, the others in the mean
                caught = out.coeff != 2.0 or out.days[2]["clean"] != "6"
                self.assertTrue(caught, f"removing the {ex} exclusion went unnoticed")


class SourceChecks(unittest.TestCase):
    """What the host build cannot reach: the call sites in the control loop and the settings path."""

    @classmethod
    def setUpClass(cls):
        cls.src = SAT_INO.read_text(encoding="utf-8")
        cls.settings = (SRC / "settingStuff.ino").read_text(encoding="utf-8")

    def test_signal_is_the_clamped_praw_minus_the_curve(self):
        self.assertRegex(self.src, r"float sigPid = pidRaw;")
        self.assertRegex(self.src, r"if \(sigPid < SAT_MIN_SETPOINT\) sigPid = SAT_MIN_SETPOINT;")
        self.assertRegex(self.src, r"if \(sigPid > sysMax\) sigPid = sysMax;")
        self.assertRegex(self.src, r"ti\.signal = sigPid - curveValue;")

    def test_tick_passes_every_exclusion_input(self):
        for frag in ("ti.sunValid = state.sat.bSunElevationValid", "ti.sunElevation = state.sat.fSunElevation",
                     "ti.pwm = pwmMode", "ti.chOn = (state.sat.iLastSentCH != 0)", "ti.flameOn = satIsFlameOn()",
                     "ti.relMod = OTcurrentSystemState.RelModLevel",
                     "OTcurrentSystemState.MaxCapacityMinModLevel & 0xFF"):
            self.assertIn(frag, self.src)

    def test_hot_water_tick_tells_the_tuner(self):
        m = re.search(r"// DHW has priority - don't adjust CH setpoint(.*?)\n    return;\n  \}", self.src, re.S)
        self.assertIsNotNone(m)
        self.assertIn("ti.dhw = true;", m.group(1))
        self.assertIn("satCurveTuneNoteTick(ti);", m.group(1))

    def test_service_runs_before_every_early_return(self):
        m = re.search(r"static void satControlLoopRun\(\)\s*\{\s*\n\s*satApplyControlStateReset\(\);[^\n]*\n\s*satCurveTuneService\(millis\(\)\);", self.src)
        self.assertIsNotNone(m)

    def test_every_early_return_path_marks_the_hour(self):
        # the gates (disabled / flashing, MQTT fallback, safety, OT link, restart) are covered by one pass flag
        self.assertIn("if (satRestartStep()) return;\n  _sat_tuneGatesPassed = true;", self.src)
        self.assertIn("if (!_sat_tuneGatesPassed) satCurveTuneNoteNotControlling();", self.src)
        self.assertRegex(self.src, r"_sat_tuneGatesPassed = false;\r?\n  satControlLoopRun\(\);")
        # boiler fault, thermal-safe fallback, summer simmer, valves closed: each marks before it returns
        self.assertGreaterEqual(self.src.count("satCurveTuneNoteNotControlling();   // TASK-1525"), 4)
        self.assertIn("ti.roomLost = !roomValid;", self.src)

    def test_window_file_and_simulation_guards(self):
        self.assertIn('"/sat_tune.json"', self.src)
        self.assertIn("if (settings.sat.bSimulation) return;", self.src)
        self.assertIn("settings.sat.bSimulation != _sat_tuneSim", self.src)
        self.assertRegex(self.src, r"static volatile bool _sat_tuneApplying")
        self.assertRegex(self.src, r"#define SAT_CT_MIN_TICK_FRACTION\s+0\.75f")

    def test_tuner_never_touches_the_pid(self):
        block = extract(self.src)
        for word in ("satPid", "satResetControlState", "fPidI", "fPidD", "fPidP", "satFlush"):
            self.assertNotIn(word, block)
        m = re.search(r'strcasecmp_P\(field, PSTR\("SATcoefficient"\)\) == 0\) \{(.*?)\n  \}', self.settings, re.S)
        self.assertIsNotNone(m, "SATcoefficient branch of updateSetting()")
        branch = m.group(1)
        self.assertIn("satCurveTuneNoteManualChange()", branch)
        self.assertNotIn("satResetControlState", branch)
        self.assertNotIn("satPid", branch)

    def test_setting_is_wired_everywhere(self):
        types = (SRC / "SATtypes.h").read_text(encoding="utf-8")
        self.assertRegex(types, r"bool\s+bCurveSelfTune\s*=\s*true;")
        self.assertIn('writeJsonBoolKV(file, F("SATcurveselftune")', self.settings)
        self.assertIn('PSTR("SATcurveselftune")', self.settings)
        rest = (SRC / "restAPI.ino").read_text(encoding="utf-8")
        for frag in ('PSTR("curve_self_tune")', 'addBool(F("satcurveselftune")', '"satcurveselftune"'):
            self.assertIn(frag, rest)
        self.assertIn('{ "curve_self_tune",        "SATcurveselftune"', (SRC / "MQTTstuff.ino").read_text(encoding="utf-8"))
        self.assertIn("-sat-curve-self-tune-enable", (SRC / "MQTTHaDiscovery.cpp").read_text(encoding="utf-8"))
        self.assertIn("SAT_HA_SWITCH_COUNT = 13", (SRC / "MQTTstuff.h").read_text(encoding="utf-8"))
        self.assertIn("sat/curve_self_tune_enable", self.src)
        self.assertIn("satcurveselftune:", (SRC / "data" / "v2.js").read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
