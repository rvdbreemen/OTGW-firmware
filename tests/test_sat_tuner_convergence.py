#!/usr/bin/env python3
"""TASK-1883 AC3: closed-loop convergence test for the SAT heating curve self-tune (ADR-205, ADR-228, ADR-230, ADR-231).

The tuner is NOT re-implemented here. Like tests/test_sat_curve_selftune.py (whose extract(), STUBS and compile_cpp()
this test imports), it cuts the two satCurveTune blocks out of src/OTGW-firmware/SATcontrol.ino, plus the real
satCalcHeatingCurve() and its SAT_HC_* constants, and compiles them with a host C++ compiler. Only the plant and the
controller around the tuner are test fixtures, written in C++ in MAIN below:

  - House (1R1C, built so the correct coefficient is known by construction):
        dTin/dt = K * (Tflow - curve(c_true, Tin, Tout)) + gains(t)
    The room is in equilibrium exactly when the flow temperature equals the REAL firmware curve evaluated at c_true,
    so c_true is "the coefficient at which the curve alone holds the target". The time constant is 1/(K c_true).
  - Controller (fixture PI, not the firmware PID): Tflow = clamp(curve(c, target, Tout) + Kp e + I), I += Ki e dt with
    an anti-windup clamp. At steady state the PI terms carry curve(c_true) - curve(c), which is what the tuner's signal
    measures in the firmware (signal = clamp(pidRaw) - curve, SATcontrol.ino satControlLoopRun()).
  - Weather: outside temperature on a daily sine; an evening internal-gains pulse and a morning loss pulse of the same
    size push the room out of the 0.1 C deadband on both sides every day, so both ADR-228 phases (warm-up |e| > 0.1,
    floating |e| <= 0.1) get ticks. With the evening pulse only, the warm-up phase is biased: see
    test_one_sided_disturbance_holds_safely_on_disagreeing_phases.
  - Every control tick (60 s) feeds satCurveTuneNoteTick() with the live coefficient's signal and calls
    satCurveTuneService(); the tuner writes the coefficient through the stubbed updateSetting(), and the next tick's
    curve uses the new value. That is the closed loop.

Convergence predicate (derived from the tuner's own band, not tuned by eye): the tuner holds once |signal| <= 1.0 C
(SAT_CT_BAND_C) and one 0.1 step moves the signal by 0.1/4 * cv(Tout) (cv = curveValue of satCalcHeatingCurve at the
target). With target 20 C and Tout around 0 C, cv ~ 20, so the tuner may legitimately stop anywhere within
4 * 1.0 / cv = 0.2 of c_true; that is the tolerance. Required: the first move goes toward c_true, the
decisions never change direction, the final coefficient is within the tolerance, and nothing changes in the last 15 days.

Mutants (applied to the extracted text in memory, never to the repo source): a sign flip of the step (diverges to the
clamp) and a step 10 times too large (1.0, larger than the band can absorb: oscillates). The suite asserts that each
mutant FAILS the predicate. OTGW_TUNER_MUTANT=sign|step10 builds the whole suite from that mutant, for the red run.

Without a host compiler the tests are skipped, they do not pass silently. The window file goes to a temp directory.
Run: python tests/test_sat_tuner_convergence.py            (green on the real tuner)
     OTGW_TUNER_MUTANT=sign python tests/test_sat_tuner_convergence.py   (red)
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_curve_selftune as base  # noqa: E402  (extract(), STUBS, compile_cpp(): the real tuner blocks)

SAT_INO = base.SAT_INO

PLANT = r"""
static float satGetBaseOffset() { return SAT_HC_BASE_OFFSET_RAD; }   // radiators (SATcontrol.ino satGetBaseOffset)
"""

MAIN = r"""
static float curveAt(float c, float target, float to) {
  const float save = settings.sat.fHeatingCurveCoeff;
  settings.sat.fHeatingCurveCoeff = c;
  const float v = satCalcHeatingCurve(target, to);
  settings.sat.fHeatingCurveCoeff = save;
  return v;
}
static uint32_t g_rng = 12345u;
static float noise() { g_rng = g_rng * 1664525u + 1013904223u; return ((float)(g_rng >> 8) / 16777216.0f) - 0.5f; }

// args: c_true c0 days to_mean to_amp [evening_only]
int main(int argc, char** argv) {
  if (argc < 6) return 2;
  const bool eveningOnly = argc > 6 && atoi(argv[6]) != 0;
  const float cTrue = (float)atof(argv[1]);
  const float c0 = (float)atof(argv[2]);
  const int days = atoi(argv[3]);
  const float toMean = (float)atof(argv[4]), toAmp = (float)atof(argv[5]);
  const float PI2 = 6.2831853f;
  const float target = 20.0f, K = 0.1f, Kp = 4.0f, Ki = 0.4f, dtH = 1.0f / 60.0f;
  settings.sat.iControlInterval = 60;
  settings.sat.fHeatingCurveCoeff = c0;
  uint32_t now = 0;                    // ms; wraps after 49.7 days, as on the device
  satCurveTuneService(now);
  float tin = target, integ = 0.0f;
  unsigned long ticks = 0;
  for (int day = 0; day < days; day++) {
    for (int m = 0; m < 1440; m++) {
      now += 60000u;
      const float to = toMean + toAmp * sinf(PI2 * (float)m / 1440.0f);
      const float c = settings.sat.fHeatingCurveCoeff;            // the live coefficient (the tuner may have changed it)
      const float curve = satCalcHeatingCurve(target, to);
      const float e = target - tin;
      integ += Ki * e * dtH;
      if (integ > 15.0f) integ = 15.0f; if (integ < -15.0f) integ = -15.0f;
      float flow = curve + Kp * e + integ;
      if (flow < 10.0f) flow = 10.0f; if (flow > 75.0f) flow = 75.0f;
      // disturbances, K/h: evening internal gains 18:00-21:00 and, unless evening-only, a morning loss 06:00-09:00 of the
      // same size (airing), so warm-up ticks fall on both sides of the target
      float gains = (m >= 1080 && m < 1260) ? 0.15f : 0.0f;
      if (!eveningOnly && m >= 360 && m < 540) gains = -0.15f;
      tin += dtH * (K * (flow - curveAt(cTrue, tin, to)) + gains) + 0.002f * noise();
      SatTuneTickIn t; memset(&t, 0, sizeof(t));
      t.satActive = true; t.sunValid = true; t.sunElevation = 5.0f; t.chOn = true; t.flameOn = true;
      t.relMod = 40.0f; t.minMod = 20.0f;
      t.signal = flow - curve;                                      // clamp(pidRaw) - curve, as the firmware samples it
      t.error = e;
      satCurveTuneNoteTick(t);
      satCurveTuneService(now);
      ticks++;
      if (settings.sat.fHeatingCurveCoeff != c)
        printf("STEP day=%d from=%.2f to=%.2f\n", day, (double)c, (double)settings.sat.fHeatingCurveCoeff);
    }
  }
  printf("FINAL coeff=%.2f writes=%d ticks=%lu tin=%.2f\n", (double)settings.sat.fHeatingCurveCoeff, g_writes, ticks,
         (double)tin);
  fputs(g_log, stdout);
  return 0;
}
"""

CURVE_RX = r"(static float satCalcHeatingCurve\(float targetTemp, float outsideTemp\)\n\{.*?\n\})"
CONST_RX = r"^static const float SAT_HC_(?:BASE_OFFSET_RAD|REF_TEMP)\s*=\s*[^;]+;"

MUTANTS = {
    # sign flip: "raise" steps down, "lower" steps up -> runs away to a clamp
    "sign": [("{ target = coeff + SAT_CT_STEP; out.decision = 1; }", "{ target = coeff - SAT_CT_STEP; out.decision = 1; }"),
             ("{ target = coeff - SAT_CT_STEP; out.decision = -1; }", "{ target = coeff + SAT_CT_STEP; out.decision = -1; }")],
    # step 10x (1.0): one step moves the signal by ~5 C, more than the +-1 C band -> overshoots and hunts
    "step10": [("{ target = coeff + SAT_CT_STEP; out.decision = 1; }", "{ target = coeff + 10.0f * SAT_CT_STEP; out.decision = 1; }"),
               ("{ target = coeff - SAT_CT_STEP; out.decision = -1; }", "{ target = coeff - 10.0f * SAT_CT_STEP; out.decision = -1; }")],
}

STEP = 0.1
BAND = 1.0


def cv(target, to):
    d = to - 20.0
    return 4.0 * (target - 20.0) + 0.03 * d * d - 0.4 * d


def tolerance(to_mean):
    """How far from c_true the tuner may legitimately stop: |signal| <= BAND holds, one STEP moves it STEP/4 * cv."""
    return 4.0 * BAND / cv(20.0, to_mean)


def source(mutant=None):
    src = SAT_INO.read_text(encoding="utf-8")
    body = base.extract(src)
    if mutant:
        for old, new in MUTANTS[mutant]:
            n = body.count(old)
            if n != 1:
                raise AssertionError(f"mutant {mutant}: expected exactly one '{old}' in the tuner, found {n}")
            body = body.replace(old, new)
    curve = re.search(CURVE_RX, src, re.S)
    consts = re.findall(CONST_RX, src, re.M)
    if not curve or len(consts) != 2:
        raise AssertionError("satCalcHeatingCurve() or its SAT_HC_* constants not found in " + str(SAT_INO))
    return base.STUBS + "\n".join(consts) + "\n" + PLANT + body + "\n" + curve.group(1) + "\n" + MAIN


def build(workdir: Path, mutant=None, name="conv"):
    cpp = workdir / f"{name}.cpp"
    cpp.write_text(source(mutant), encoding="utf-8")
    return base.compile_cpp(cpp, workdir / f"{name}.exe")


class Run:
    def __init__(self, text):
        self.steps = [(int(d), float(a), float(b)) for d, a, b in re.findall(r"^STEP day=(\d+) from=(\S+) to=(\S+)", text, re.M)]
        m = re.search(r"^FINAL coeff=(\S+) writes=(\d+) ticks=(\d+)", text, re.M)
        self.final = float(m.group(1))
        self.writes = int(m.group(2))
        self.ticks = int(m.group(3))
        kv = lambda ln: dict(re.findall(r"(\w+)=(\S+)", ln))
        self.days = [kv(l) for l in text.splitlines() if l.startswith("SATTUNED ")]
        self.dirs = [1 if b > a else -1 for _, a, b in self.steps]

    def direction_changes(self):
        return sum(1 for x, y in zip(self.dirs, self.dirs[1:]) if x != y)


def converges(r: Run, c_true, c0, days, to_mean):
    """The convergence predicate. Returns a list of the reasons it fails (empty = converged)."""
    why = []
    tol = tolerance(to_mean)
    if not r.steps:
        if abs(c0 - c_true) > tol:
            why.append("never moved")
    else:
        if r.dirs[0] != (1 if c_true > c0 else -1):
            why.append(f"first move away from c_true ({r.steps[0]})")
        if r.direction_changes():
            why.append(f"{r.direction_changes()} direction changes")
        if r.steps[-1][0] >= days - 15:
            why.append(f"still moving on day {r.steps[-1][0]} of {days}")
    if abs(r.final - c_true) > tol + 1e-6:
        why.append(f"final {r.final:.2f} not within {tol:.2f} of {c_true:.2f}")
    return why


SCENARIOS = [
    # (name, c_true, c0, days, to_mean, to_amp)
    ("raise_1.0_to_1.8", 1.8, 1.0, 60, 0.0, 3.0),
    ("lower_2.6_to_1.4", 1.4, 2.6, 70, 0.0, 3.0),   # 70 days: the ms clock wraps on day 49.7
    ("already_right", 1.8, 1.8, 30, 0.0, 3.0),
]


class TunerConvergence(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.mutant = os.environ.get("OTGW_TUNER_MUTANT") or None
        cls.exe = build(Path(cls.tmp.name), cls.mutant)
        cls.mexe = {}

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        if not self.exe:
            self.skipTest("no host C++ compiler (g++, clang++ or MSVC)")

    def run_sim(self, exe, c_true, c0, days, to_mean, to_amp, tag, evening_only=False):
        env = dict(os.environ, SATTUNE_FILE=str(Path(self.tmp.name) / f"{tag}.json"))
        args = [str(exe), str(c_true), str(c0), str(days), str(to_mean), str(to_amp), "1" if evening_only else "0"]
        r = subprocess.run(args, capture_output=True,
                           text=True, check=True, env=env, cwd=self.tmp.name)
        return Run(r.stdout)

    def test_converges_without_oscillation(self):
        for name, c_true, c0, days, to_mean, to_amp in SCENARIOS:
            with self.subTest(scenario=name):
                r = self.run_sim(self.exe, c_true, c0, days, to_mean, to_amp, name)
                print(f"  {name}: final={r.final:.2f} steps={len(r.steps)} dir_changes={r.direction_changes()} "
                      f"last_step_day={r.steps[-1][0] if r.steps else '-'} tol={tolerance(to_mean):.2f}")
                self.assertEqual(r.ticks, days * 1440)
                self.assertEqual(converges(r, c_true, c0, days, to_mean), [])

    def test_both_phases_get_data_and_the_decisions_come_from_agreeing_phases(self):
        r = self.run_sim(self.exe, 1.8, 1.0, 60, 0.0, 3.0, "phases")
        decided = [d for d in r.days if d.get("decision") in ("raise", "lower")]
        self.assertTrue(decided, "the tuner never decided")
        for d in decided:
            self.assertGreater(int(d["wticks3"]), 0, d)
            self.assertGreater(int(d["fticks3"]), 60, d)     # > 1 h of floating data at 60 s ticks (ADR-230)
            self.assertEqual(d["fvote"], "1", d)
            self.assertEqual(d["reason"], "phases_agree", d)

    def test_each_mutant_fails_the_convergence_predicate(self):
        if self.mutant:
            self.skipTest("suite built from a mutant")
        for mname in MUTANTS:
            exe = build(Path(self.tmp.name), mname, name=f"m_{mname}")
            failed = []
            for name, c_true, c0, days, to_mean, to_amp in SCENARIOS[:2]:
                r = self.run_sim(exe, c_true, c0, days, to_mean, to_amp, f"m_{mname}_{name}")
                why = converges(r, c_true, c0, days, to_mean)
                print(f"  mutant {mname} {name}: final={r.final:.2f} steps={len(r.steps)} "
                      f"dir_changes={r.direction_changes()} -> {why}")
                failed.append(bool(why))
            with self.subTest(mutant=mname):
                self.assertTrue(all(failed), f"mutant {mname} passed the predicate: the test would not catch it")

    def test_tolerance_comes_from_the_band(self):
        # 4 * 1.0 / cv(20, 0) = 4 / 20 = 0.20; one 0.1 step moves the signal 0.1 / 4 * 20 = 0.5 C
        self.assertAlmostEqual(tolerance(0.0), 0.20, places=6)

    def test_one_sided_disturbance_holds_safely_on_disagreeing_phases(self):
        # Characterisation, not a verdict: with only the evening gains pulse the warm-up ticks fall mostly on the overshoot
        # side, the warm-up mean sits below the floating mean and the ADR-228 agreement rule can hold before the band does.
        # Asserted: the tuner never moves away from c_true, never changes direction, and ends no further from c_true than
        # it started. The stopping point is printed, not pinned.
        for name, c_true, c0, days, to_mean, to_amp in SCENARIOS[:2]:
            with self.subTest(scenario=name):
                r = self.run_sim(self.exe, c_true, c0, days, to_mean, to_amp, "one_sided_" + name, evening_only=True)
                last = r.days[-1]
                print(f"  one-sided {name}: final={r.final:.2f} steps={len(r.steps)} reason={last['reason']} "
                      f"wmean3={last['wmean3']} fmean3={last['fmean3']}")
                self.assertTrue(r.steps, "never moved")
                self.assertEqual(r.dirs[0], 1 if c_true > c0 else -1)
                self.assertEqual(r.direction_changes(), 0)
                self.assertLess(abs(r.final - c_true), abs(c0 - c_true))


if __name__ == "__main__":
    unittest.main(verbosity=2)
