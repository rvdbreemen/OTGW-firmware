#!/usr/bin/env python3
"""TASK-1527 round 21: the simulation's SAT port (scripts/sat_sim_port.py) against the REAL firmware functions.

The heat-pump simulation library runs SAT through a Python port. This test proves the port computes what the firmware
computes, minute by minute, on the same input sequences:
  1. heating curve + PID: the whole SATpid.ino and satCalcHeatingCurve (SATcontrol.ino), compiled with the harness of
     tests/test_sat_pid_cadence.py, driven like satControlLoopRun(): satPidService() then satPidNoteRoom() and
     satPidOutput() once per minute; radiators and underfloor; several room traces (inside and outside the deadband,
     sensor resolution 0.1 and 0.01 C, outdoor ramps). The port must give the same output to 0.0011 C (one float32
     rounding step at the third decimal), and the same output exactly in at least 99.5 % of the minutes;
  2. the PWM duty mapper: the real satApplyPWM block through tests/test_sat_hp_pwm.py's harness, on a duty grid, flame on
     and off, for the firmware's heat-pump periods 1800 / 2400 s with its 180 s minimum ON;
  3. satChEnableDecide (SATchEnable.h), on a grid.
Not covered (stated, not hidden): the PWM entry switch and exits, the ON-phase setpoint overrides, the room sensor
filter, presets and solar gain; the simulation models those in its own loop.

Old-vs-fix: OTGW_SATPID_INO / OTGW_SAT_INO point at other copies, as in the source harnesses. A mutant check is built in:
test_mutant_port_is_caught runs a port with the derivative skip rule changed and must see a difference.

Run: python tests/test_sat_sim_port_equivalence.py
"""
import math
import os
import random
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
sys.path.insert(0, str(HERE)); sys.path.insert(0, str(REPO / "scripts"))
import test_sat_pid_cadence as cad  # noqa: E402
import test_sat_hp_pwm as hpw  # noqa: E402
from test_sat_pwm_override import compile_cpp  # noqa: E402
import sat_sim_port as port  # noqa: E402

GATHER = r"""
static float g_room = 20.0f, g_target = 20.0f, g_outside = 5.0f;
bool satPidGatherInputs(float* room, float* target, float* curve, float* deadbandExtra) {
  *room = g_room; *target = g_target; *curve = satCalcHeatingCurve(g_target, g_outside); *deadbandExtra = 0.0f;
  return true;
}
"""

MAIN = r"""
int main(int argc, char** argv) {
  // argv: coeff underfloor tracefile ; trace lines: room target outside (one per minute)
  settings.sat.fHeatingCurveCoeff = (float)atof(argv[1]); g_ufh = atoi(argv[2]);
  FILE* f = fopen(argv[3], "r"); if (!f) return 2;
  g_base = 1000000u; g_ms = g_base; satPidReset();
  float r, tg, o; int k = 0;
  while (fscanf(f, "%f %f %f", &r, &tg, &o) == 3) {
    g_ms = g_base + (uint32_t)k * 60000u; g_room = r; g_target = tg; g_outside = o;
    satPidService(g_ms);
    const float cv = satCalcHeatingCurve(tg, o);
    satPidNoteRoom(r, g_ms);
    const float out = satPidOutput(cv);
    printf("%d %.6f %.6f\n", k, cv, out);
    k++;
  }
  fclose(f);
  return 0;
}
"""


def build_pid(workdir: Path):
    sat = cad.SAT_INO.read_text(encoding="utf-8"); pid = cad.PID_INO.read_text(encoding="utf-8")
    pid = pid[pid.index("// --- PID Constants ---"):]
    prelude = (cad.PRELUDE
               .replace("static int satGetEffectiveHeatingSystem() { return 1; }   // radiators",
                        "static int g_ufh = 0;\nstatic int satGetEffectiveHeatingSystem() { return g_ufh ? 2 : 1; }")
               .replace("static float satGetBaseOffset() { return 27.2f; }",
                        "static float satGetBaseOffset() { return g_ufh ? 20.0f : 27.2f; }"))
    assert "g_ufh" in prelude, "harness prelude changed: heating-system stubs not found"
    cpp_txt = cad.adapt(sat, prelude.replace("@TIMERS@", cad.TIMERS).replace("@ZONE@", cad.zone_source(sat)).replace("@PID@", pid)
                        .replace("@GATHER@", GATHER).replace("@SAVE@", cad.save_source(sat))
                        + "#define HAVE_SERVICE 1\n" + MAIN)
    cpp = workdir / "pidport.cpp"; cpp.write_text(cpp_txt, encoding="utf-8")
    return compile_cpp(cpp, workdir / "pidport.exe")


def make_trace(kind, n=2880, seed=1):
    rnd = random.Random(seed); out = []
    for k in range(n):
        t = k / 60.0
        outside = 5.0 + 6.0 * math.sin(2 * math.pi * k / 1440.0) + (k / n) * -8.0
        target = 20.0 if (kind != "setback" or (k % 1440) < 1380 and (k % 1440) >= 360) else 17.0
        if kind == "inside":   # small error, mostly inside the 0.1 deadband
            room = target - 0.05 + 0.08 * math.sin(t / 3.1) + rnd.uniform(-0.02, 0.02); res = 0.01
        elif kind == "outside":   # error outside the deadband, slow swings
            room = target - 0.6 + 0.9 * math.sin(t / 5.3); res = 0.1
        elif kind == "setback":
            room = 19.6 + 0.8 * math.sin(t / 4.0); res = 0.1
        else:   # "mixed": crosses the deadband, noisy sensor
            room = target - 0.2 + 0.4 * math.sin(t / 2.2) + rnd.uniform(-0.05, 0.05); res = 0.1
        out.append((round(room / res) * res, target, round(outside, 1)))
    return out


def run_port(trace, coeff, ufh, pid_cls=port.Pid):
    pid = pid_cls(coeff, ufh); res = []
    base = 1000000
    for k, (room, target, outside) in enumerate(trace):
        now = base + k * 60000
        cv = port.curve(coeff, target, outside, 20.0 if ufh else 27.2)
        pid.service(now, room, target, cv)
        pid.note_room(room, now)
        res.append((float(cv), float(pid.output(cv))))
    return res


class MutantPid(port.Pid):
    """The derivative skip rule changed from <= 60 s to < 60 s: a plausible porting slip the test must catch."""
    def _derivative(self, room, error):
        changed = self.room_changed_ms
        if abs(error) <= port.DEADBAND: self.last_deriv_ms = changed; return
        delta = port.f32(room) - self.last_room
        if delta == port.f32(0.0): self.last_deriv_ms = changed; return
        dt = port.f32(changed - self.last_deriv_ms) / port.f32(1000.0)
        if dt < port.PID_INTERVAL: return
        raw = -delta / dt
        alpha = dt / (port.PID_INTERVAL + dt)
        self.raw_d = port.f32(max(-5.0, min(5.0, alpha * raw + (1 - alpha) * self.raw_d))); self.last_deriv_ms = changed


class PortEquivalence(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.exe = build_pid(Path(cls.tmp.name))
        pwm_dir = Path(cls.tmp.name) / "pwm"; pwm_dir.mkdir()
        cls.pwm_exe = hpw.build(pwm_dir)

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def setUp(self):
        if not self.exe: self.skipTest("no host C++ compiler")

    def firmware(self, trace, coeff, ufh):
        tf = Path(self.tmp.name) / "trace.txt"
        tf.write_text("\n".join(f"{r:.4f} {t:.4f} {o:.4f}" for r, t, o in trace), encoding="utf-8")
        out = subprocess.run([str(self.exe), str(coeff), str(int(ufh)), str(tf)], capture_output=True, text=True, check=True).stdout
        return [(float(a), float(b)) for _, a, b in (l.split() for l in out.strip().splitlines())]

    def compare(self, trace, coeff, ufh, pid_cls=port.Pid):
        fw = self.firmware(trace, coeff, ufh); py = run_port(trace, coeff, ufh, pid_cls)
        self.assertEqual(len(fw), len(py))
        dcurve = max(abs(a[0] - b[0]) for a, b in zip(fw, py))
        dout = [abs(a[1] - b[1]) for a, b in zip(fw, py)]
        exact = sum(1 for d in dout if d < 1e-6) / len(dout)
        return dcurve, max(dout), exact

    def test_pid_and_curve_match_the_firmware(self):
        report = []
        for kind in ("inside", "outside", "setback", "mixed"):
            for coeff, ufh in ((0.8, True), (1.6, True), (1.8, False), (2.4, False)):
                dc, do, ex = self.compare(make_trace(kind), coeff, ufh)
                report.append((kind, coeff, ufh, dc, do, ex))
                self.assertLess(dc, 1e-4, f"curve differs: {kind} c={coeff} ufh={ufh}: {dc}")
                self.assertLessEqual(do, 0.0011, f"PID output differs: {kind} c={coeff} ufh={ufh}: max {do}")
                self.assertGreaterEqual(ex, 0.995, f"PID output exact in only {ex:.4f} of minutes: {kind} c={coeff} ufh={ufh}")
        print("\nPID/curve equivalence (trace, coeff, underfloor, max curve diff, max output diff, share exact):")
        for r in report: print("  %-8s %.1f %-5s %.2e %.4f %.4f" % r)

    def test_mutant_port_is_caught(self):
        worst = 0.0
        for kind in ("outside", "mixed"):
            _, do, _ = self.compare(make_trace(kind), 1.6, True, MutantPid); worst = max(worst, do)
        self.assertGreater(worst, 0.0011, "the test did not detect a changed derivative rule in the port")

    def test_pwm_mapper_matches_the_firmware(self):
        if not self.pwm_exe: self.skipTest("no host C++ compiler")
        worst = 0.0; n = 0; boundary = []
        for hp in (1800, 2400):
            dl = 180.0 / hp; edges = (dl / 2, dl, 1 - dl, 1 - dl / 2)
            for i in range(1, 100):
                ratio = i / 100.0
                for flame in (0, 1):
                    r = dict(kv.split("=") for kv in subprocess.run([str(self.pwm_exe), "map", "2", str(hp), "3", str(ratio), str(flame)],
                                                                    capture_output=True, text=True, check=True).stdout.split())
                    duty = float(r["duty"]); fon, foff = float(r["on"]), float(r["off"])
                    if any(abs(duty - e) < 1e-4 for e in edges):
                        # the harness prints the duty to 4 decimals; on a range edge the float32 duty can fall either side.
                        # Accept the firmware result if the port gives it for a duty just below or just above the edge.
                        cands = [port.pwm_times(duty + s, hp, 180, bool(flame)) for s in (-2e-4, 2e-4)]
                        ok = any(abs(fon - (2 * hp if c else a)) <= 1 and abs(foff - (0.0 if c else b)) <= 1 for a, b, c in cands)
                        boundary.append((hp, duty, flame, ok)); self.assertTrue(ok, f"edge case hp={hp} duty={duty} flame={flame}")
                        continue
                    on_s, off_s, cont = port.pwm_times(duty, hp, 180, bool(flame))
                    fon_exp, foff_exp = (2 * hp, 0.0) if cont else (on_s, off_s)
                    worst = max(worst, abs(fon - fon_exp), abs(foff - foff_exp)); n += 1
        print(f"\nPWM mapper: {n} cases off the range edges, worst difference {worst:.3f} s; {len(boundary)} edge cases, all on one side of the edge")
        self.assertLessEqual(worst, 1.0)

    def test_ch_enable_matches_the_firmware(self):
        src = r"""
#include <cstdio>
#include "@H@"
int main() { const float req[] = {9.f, 21.f, 21.01f, 30.f}; for (float r : req) for (int d = 0; d < 2; d++) for (int o = 0; o < 2; o++)
  printf("%.2f %d %d %d\n", r, d, o, (int)satChEnableDecide(r, 21.0f, d, o)); return 0; }
""".replace("@H@", (REPO / "src" / "OTGW-firmware" / "SATchEnable.h").as_posix())
        cpp = Path(self.tmp.name) / "ch.cpp"; cpp.write_text(src, encoding="utf-8")
        exe = compile_cpp(cpp, Path(self.tmp.name) / "ch.exe")
        if not exe: self.skipTest("no compiler")
        for line in subprocess.run([str(exe)], capture_output=True, text=True, check=True).stdout.splitlines():
            r, d, o, v = line.split()
            self.assertEqual(int(v), port.ch_enable(float(r), 21.0, bool(int(d)), bool(int(o))), line)


if __name__ == "__main__":
    unittest.main(verbosity=2)
