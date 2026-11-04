#!/usr/bin/env python3
"""TASK-1412.15 / LL-022: SAT telemetry against independent, timestamped OpenTherm frames.

The oracle is a list of timestamped PIC-format frames ("B" + 8 hex digits, parity included), the same line format as the
PIC stream and tests/webui/fixtures/sat-pwm-suppression-replay.txt. Cycle, off-gap and duty figures are derived from the
frame times in Python, with no use of SAT's own variables, and compared with what the REAL SATcycles.ino / SATcontrol.ino code
stores and publishes when it is fed the same frames. Nothing of the code under test is re-implemented:
  * cycle metrics: satCycleInit, satCycleOnFlameChange, satCycleSample, satGetWindow4hStats, satGetWindow24hStats,
    _cycleRecord, _hourCount*, _dailyRecord cut out of SATcycles.ino with the cuts of tests/test_sat_cycle_classifier.py;
  * PWM seconds and duty: satApplyPWM / satChEnableDecide through tests/test_sat_hp_pwm.build;
  * MM value: satComputeMM cut out of SATcontrol.ino.
The thin adapter in the cycle harness feeds one status frame per second and calls satCycleOnFlameChange on a flame-bit edge, the
way SATcontrol.ino:6803-6814 does. OTGW-Core.ino's frame decode and the control loop around that call are NOT run here.

Decisions applied here (docs/sat/sergeant-d-corrections.md): entry 266 (Q-TW-1) an unknown first off gap is excluded from the averages
and the duty EMA, entry 267 (Q-TW-2) mod_suppressed follows the PWM suppression, entry 268 (Q-TW-3) the 4 h / 24 h duty is flame-on
seconds / window length, entry 269 (Q-TW-4) current_modulation is 100 while MM=T goes out.

Scenarios: clean cycles with a known first gap (S1), a boiler already burning at boot (S2), hot-water overlap with and without a
flame edge (S3), a flicker and a missing frame (S4), window expiry at 4 h and 24 h (S5), PWM seconds and duty against the CH-enable
and flame bits of the frame stream (P1), the MM value and its unit (M1), and the three telemetry values that used to differ from the wire
(O1 mod_suppressed, O2 current_modulation at the maximum PWM duty, O3 the first-cycle off gap), now contract tests.

Run: python tests/test_sat_telemetry_wire.py
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent
REPO = TESTS.parent
FW = REPO / "src" / "OTGW-firmware"
sys.path.insert(0, str(TESTS))
import test_sat_cycle_classifier as tcc  # noqa: E402
import test_sat_hp_pwm as hp  # noqa: E402
from test_sat_pwm_override import compile_cpp  # noqa: E402

CYCLES_INO = Path(os.environ.get("OTGW_SATCYCLES_INO", FW / "SATcycles.ino"))
SAT_INO = Path(os.environ.get("OTGW_SAT_INO", FW / "SATcontrol.ino"))
H4 = 4 * 3600 * 1000
EMA_ALPHA = 0.15   # SATcycles.ino EMA_ALPHA; asserted against the source in test_oracle_constants_match_the_source


# ---- frames: the independent oracle ---------------------------------------------------------------------------------------
def mk_frame(msg_type, msg_id, data):
    v = (msg_type << 28) | (msg_id << 16) | (data & 0xFFFF)
    if bin(v).count("1") % 2:
        v |= 1 << 31
    return "B%08X" % v


def status_frame(master_hb, flame, ch_mode=True, dhw=False):
    lb = (0x08 if flame else 0) | (0x02 if ch_mode else 0) | (0x04 if dhw else 0)
    return mk_frame(4, 0, (master_hb << 8) | lb)


def flow_frame(temp):
    return mk_frame(4, 25, int(round(temp * 256)))


def decode(line):
    v = int(line[1:], 16)
    assert bin(v).count("1") % 2 == 0, "odd parity in " + line
    return (v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF


def one_hz(t0, seconds, flame, ch_enable=True, dhw=False, flow=50.0):
    """(t_ms, frame) at 1 Hz from t0 for `seconds` seconds."""
    out = []
    for s in range(seconds):
        t = t0 + s * 1000
        out.append((t, flow_frame(flow)))
        out.append((t, status_frame(1 if ch_enable else 0, flame, ch_mode=flame, dhw=dhw)))
    return out


def flame_cycles(frames):
    """Cycles from the status frames alone. on = first flame frame to first no-flame frame; gap = previous no-flame edge to
    this flame edge, None when no flame-off edge was seen before (flame already on at the first frame)."""
    out, prev, on_t, last_off = [], None, None, None
    first = True
    for t, line in frames:
        mid, hb, lb = decode(line)
        if mid != 0:
            continue
        flame = bool(lb & 0x08)
        if first:
            first = False
            if flame:   # burning at the first observation: its start and its gap are unknown
                on_t = t
                prev = True
                out_open = {"start": t, "gap": None, "samples": 1, "start_known": False}
                cur = out_open
                continue
            last_off = t   # flame off since the first frame: observation start is the (lower bound of the) off edge
            prev = False
            cur = None
            continue
        if flame and not prev:
            cur = {"start": t, "gap": (t - last_off) if last_off is not None else None, "samples": 1, "start_known": True}
        elif flame and prev:
            cur["samples"] += 1
        elif (not flame) and prev:
            cur["end"] = t
            cur["on"] = t - cur["start"]
            cur["samples"] += 1   # the flame-off sample is taken at the edge too (harness takes the sample after the edge)
            out.append(cur)
            last_off = t
        prev = flame
    return out


def recorded(cycles, min_samples=3):
    """tracker.py:98-100: a cycle with fewer than 3 samples is not recorded (SAT_CYCLE_MIN_SAMPLES, SATcycles.ino:30)."""
    return [c for c in cycles if c["samples"] >= min_samples]


def oracle_window(cycles, now_ms, span_ms):
    """Cycles in the window and the figures a reader can derive from the frames: average on, average known gap, duty =
    sum(on of every cycle ending in the window) / window length, clamped to 1 (thermo-nova cycles/history.py:209-216, entry 268).
    Unknown gaps are not a number of seconds: they stay out of the average gap (entry 266) but their flame counts in the duty."""
    inwin = [c for c in cycles if now_ms - c["end"] <= span_ms]
    known = [c for c in inwin if c["gap"] is not None]
    r = {"n": len(inwin), "n_known_gap": len(known)}
    r["avg_on_s"] = (sum(c["on"] for c in inwin) / len(inwin) / 1000.0) if inwin else 0.0
    r["avg_off_s"] = (sum(c["gap"] for c in known) / len(known) / 1000.0) if known else None
    r["duty"] = min(1.0, sum(c["on"] for c in inwin) / float(span_ms))
    return r


def oracle_ema(cycles):
    """The per-cycle duty EMA, on / (on + gap) per cycle with a known gap, started at 0."""
    e = 0.0
    for c in cycles:
        if c["gap"] is None:
            continue
        e = EMA_ALPHA * (c["on"] / float(c["on"] + c["gap"])) + (1.0 - EMA_ALPHA) * e
    return e


# ---- the cycle harness: real SATcycles.ino, frames in -------------------------------------------------------------------
MAIN2 = r"""
static void apply(float flow, unsigned flags) {
  g_flow = flow;
  OTcurrentSystemState.Statusflags = (uint16_t)flags;
  OTcurrentSystemState.MasterStatus = (uint8_t)(flags >> 8);
  OTcurrentSystemState.SlaveStatus = (uint8_t)(flags & 0xFF);
  state.sat.bDhwActive = (flags & 0x04) != 0;
  state.sat.eControlMode = (SATControlMode)1;
  state.sat.bPwmFlameRequested = false;
}
int main(int argc, char** argv) {
  FILE* f = fopen(argv[1], "r");
  if (!f) return 2;
  char op; bool prevFlame = false;   // _sat_prevFlameState starts false (SATcontrol.ino:6803)
  settings.sat.fOvershootMargin = 2.0f;
  state.sat.fFinalSetpoint = 50.0f; _sat_requestedSetpoint = 50.0f; state.sat.fPidOutput = 50.0f;
  while (fscanf(f, " %c", &op) == 1) {
    unsigned long t;
    if (op == 'I') { if (fscanf(f, "%lu", &t) != 1) return 3; g_ms = (uint32_t)t; satCycleInit(); }
    else if (op == 'S') {   // S t_ms flags flow
      unsigned flags; float flow;
      if (fscanf(f, "%lu %i %f", &t, &flags, &flow) != 3) return 3;
      g_ms = (uint32_t)t; apply(flow, flags);
      const bool flame = (flags & 0x08) != 0;
      if (flame != prevFlame) { satCycleOnFlameChange(flame); prevFlame = flame; }
      satCycleSample();
    } else if (op == 'Q') {   // Q t_ms : print the published window figures
      if (fscanf(f, "%lu", &t) != 1) return 3;
      g_ms = (uint32_t)t;
      satGetWindow4hStats(); satGetWindow24hStats();
      printf("Q t=%lu n4=%d on4=%.9g off4=%.9g duty4=%.9g n24=%d duty24=%.9g ema=%.9g cycles=%lu win=%d\n", t, (int)state.sat.i4hCycles,
             (double)state.sat.f4hAvgOnSec, (double)state.sat.f4hAvgOffSec, (double)state.sat.f4hDutyRatio, (int)state.sat.i24hCycles,
             (double)state.sat.f24hDutyRatio, (double)state.sat.fDutyRatio, (unsigned long)state.sat.iCycleCount, (int)_win4hCount);
      for (int i = 0; i < _win4hCount; i++) {
        const SATWindowRecord& w = _win4h[(_win4hHead + SAT_WIN4H_SIZE - _win4hCount + i) % SAT_WIN4H_SIZE];
        printf("R end=%lu on=%lu off=%lu\n", (unsigned long)w.endMs, (unsigned long)w.onDurationMs, (unsigned long)w.offDurationMs);
      }
    }
  }
  return 0;
}
"""


def build_cycle_exe(wd: Path):
    cycles = CYCLES_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    types_src = tcc.SATTYPES_H.read_text(encoding="utf-8").replace("\r\n", "\n")
    base = tcc.extract_cycles(cycles)
    # what satCycleInit, the hour counter and the 24 h daily window need on top of the classifier cut
    consts = re.findall(r"^static const \w+\s+(?:HCR_\w+)\s*=[^;\n]*;", cycles, re.M)
    daily_struct = tcc._grab(r"^struct SATDailyBucket\s*\{.*?^\};", cycles, "struct SATDailyBucket").group(0)
    globs = re.findall(r"^static (?:const )?\w+\s+(?:_hourCycle|_daily|_hcr)\w*(?:\[[^\]]*\])?[^;\n(]*;", cycles, re.M)
    funcs = [tcc._grab(p, cycles, n).group(0) for p, n in (
        (r"^void satCycleInit\(\)\s*\{.*?^\}", "satCycleInit"),
        (r"^static void _hourCountRecord\(.*?^\}", "_hourCountRecord"),
        (r"^static uint8_t _hourCountGet\(.*?^\}", "_hourCountGet"),
        (r"^static void _dailyRecord\(.*?^\}", "_dailyRecord"),
        (r"^void satGetWindow24hStats\(\)\s*\{.*?^\}", "satGetWindow24hStats"))]
    n_const = len(re.findall(r"^static const \w+\s+(?:SAT_\w+|EMA_ALPHA)\s*=[^;\n]*;", cycles, re.M))
    parts = base.split("\n", n_const)
    base_consts, base_rest = parts[:n_const], parts[n_const]
    fwd = ("static uint8_t _hourCountGet(uint32_t);\nstatic void _hourCountRecord(uint32_t);\n"
           "static void _dailyRecord(uint32_t, uint32_t, uint32_t, SATCycleClass, float);\n")
    body = ("\n".join(base_consts) + "\n" + "\n".join(consts) + "\n" + daily_struct + "\n" + "\n".join(globs) + "\n" + fwd + base_rest
            + "\n" + "\n".join(funcs) + "\n")
    stubs = tcc.STUBS.replace("@SETTINGS@", tcc.SETTINGS_REAL).replace("@IDLE_RET@", tcc.idle_return_type(SAT_INO.read_text(encoding="utf-8")))
    for line in ("static void _hourCountRecord(uint32_t) {}\n", "static uint8_t _hourCountGet(uint32_t) { return 1; }\n",
                 "static int g_dailyCalls = 0;\n", "static void _dailyRecord(uint32_t, uint32_t, uint32_t, SATCycleClass, float) { g_dailyCalls++; }\n"):
        assert line in stubs, "stub not found in the classifier harness: " + line
        stubs = stubs.replace(line, "")
    stubs += "static size_t strlcpy(char* d, const char* s, size_t n) { snprintf(d, n, \"%s\", s); return strlen(s); }\n"
    cpp = wd / "cyc.cpp"
    cpp.write_text(tcc.PRELUDE + tcc.extract_types(types_src, True) + stubs + body + MAIN2, encoding="utf-8")
    return compile_cpp(cpp, wd / "cyc.exe")


class Rig:
    exe = None
    dir = None

    @classmethod
    def get(cls):
        if cls.exe is None and cls.dir is None:
            cls.dir = tempfile.TemporaryDirectory()
            cls.exe = build_cycle_exe(Path(cls.dir.name))
        return cls.exe


def run_script(frames, boot_ms, queries):
    """Feed the frames (status frames drive the harness; flow frames set the flow temperature) and return the Q results."""
    exe = Rig.get()
    if exe is None:
        raise unittest.SkipTest("no host C++ compiler")
    lines = ["I %d" % boot_ms]
    flow = 50.0
    pending = []
    for t, line in sorted(frames, key=lambda x: x[0]):
        mid, hb, lb = decode(line)
        if mid == 25:
            flow = ((hb << 8) | lb) / 256.0
        elif mid == 0:
            pending.append((t, (hb << 8) | lb, flow))
    qs = sorted(queries)
    qi = 0
    for t, flags, fl in pending:
        while qi < len(qs) and qs[qi] <= t:
            lines.append("Q %d" % qs[qi]); qi += 1
        lines.append("S %d %d %.3f" % (t, flags, fl))
    while qi < len(qs):
        lines.append("Q %d" % qs[qi]); qi += 1
    p = Path(Rig.dir.name) / ("script_%d.txt" % abs(hash(tuple(lines))))
    p.write_text("\n".join(lines) + "\n")
    out = subprocess.run([str(exe), str(p)], capture_output=True, text=True, timeout=60).stdout
    res, cur = {}, None
    for ln in out.splitlines():
        kv = dict(tok.split("=") for tok in ln.split()[1:])
        if ln.startswith("Q"):
            cur = {k: float(v) for k, v in kv.items()}
            cur["records"] = []
            res[int(cur["t"])] = cur
        elif ln.startswith("R"):
            cur["records"].append({k: int(v) for k, v in kv.items()})
    return res


def cycle(on_s, off_s_after, flow=50.0, ch=True, dhw=False):
    return (on_s, off_s_after, flow, ch, dhw)


def build_stream(t0, off_first_s, plan, burning_at_start=False):
    """Frames: off_first_s seconds without flame, then each (on_s, off_after_s) cycle."""
    fr, t = [], t0
    if off_first_s:
        fr += one_hz(t, off_first_s, False, ch_enable=True); t += off_first_s * 1000
    for on_s, off_s in plan:
        fr += one_hz(t, on_s, True); t += on_s * 1000
        if off_s:
            fr += one_hz(t, off_s, False); t += off_s * 1000
    return fr, t


class CycleMetricsVsFrames(unittest.TestCase):
    def check(self, res, q, cycles, name):
        """The published window figures (state.sat.f4h*, fDutyRatio) against the frame-derived oracle."""
        o = oracle_window(cycles, q, H4)
        r = res[q]
        self.assertEqual(int(r["n4"]), o["n"], name + ": cycles in the 4 h window")
        self.assertAlmostEqual(r["on4"], o["avg_on_s"], places=2, msg=name + ": average on-time (s)")
        self.assertAlmostEqual(r["off4"], o["avg_off_s"] if o["avg_off_s"] is not None else 0.0, places=2,
                               msg=name + ": average off-gap (s), the n is the known gaps only")
        self.assertAlmostEqual(r["duty4"], o["duty"], places=6, msg=name + ": 4 h duty = flame-on seconds / 4 h")
        return o

    def test_s1_clean_cycles_match_the_frames(self):
        frames, end = build_stream(1_000_000, 60, [(300, 600), (400, 500), (300, 0)])
        frames += one_hz(end, 5, False)
        end += 5000
        res = run_script(frames, 1_000_000, [end])
        cyc = recorded(flame_cycles(frames))
        self.assertEqual(len(cyc), 3)
        o = self.check(res, end, cyc, "S1")
        self.assertEqual([(int(c["gap"]), int(c["on"])) for c in cyc], [(60000, 300000), (600000, 400000), (500000, 300000)])
        recs = res[end]["records"]
        self.assertEqual([(r["on"], r["off"]) for r in recs], [(c["on"], c["gap"]) for c in cyc], "per-cycle on/off in the window ring")
        self.assertAlmostEqual(res[end]["ema"], oracle_ema(cyc), places=5, msg="S1: duty EMA (per cycle on / (on + gap))")
        o24 = oracle_window(cyc, end, 24 * 3600 * 1000)
        self.assertAlmostEqual(res[end]["duty24"], o24["duty"], places=6, msg="S1: 24 h duty = flame-on seconds / 24 h")
        self.assertAlmostEqual(res[end]["duty4"], 1000.0 / 14400.0, places=6, msg="S1: 1000 s of flame in 4 h, history shorter than the window")
        self.assertEqual(int(res[end]["n24"]), 3)

    def test_s3_hot_water_overlap_without_a_flame_edge_is_one_cycle_with_the_real_duration(self):
        # CH 300 s, a draw takes over for 120 s with the flame never going out, CH again 200 s, then off 600 s, one more CH cycle.
        fr, t = [], 2_000_000
        fr += one_hz(t, 30, False); t += 30000
        fr += one_hz(t, 300, True); t += 300000
        fr += one_hz(t, 120, True, ch_enable=False, dhw=True); t += 120000
        fr += one_hz(t, 200, True); t += 200000
        fr += one_hz(t, 600, False); t += 600000
        fr += one_hz(t, 250, True); t += 250000
        fr += one_hz(t, 5, False); t += 5000
        res = run_script(fr, 2_000_000, [t])
        cyc = recorded(flame_cycles(fr))
        self.assertEqual([c["on"] for c in cyc], [620000, 250000], "the overlap does not split or shorten the first burn")
        self.check(res, t, cyc, "S3")
        self.assertEqual([(r["on"], r["off"]) for r in res[t]["records"]], [(620000, 30000), (250000, 600000)])

    def test_s3b_a_hot_water_only_burn_is_a_cycle_of_its_own_and_counts_in_the_window(self):
        # The 4 h and 24 h figures count every flame cycle, hot water ones included (entry 268, as thermo-nova); the frames decide the numbers.
        fr, t = [], 3_000_000
        fr += one_hz(t, 30, False); t += 30000
        fr += one_hz(t, 150, True, ch_enable=False, dhw=True); t += 150000
        fr += one_hz(t, 300, False); t += 300000
        fr += one_hz(t, 400, True); t += 400000
        fr += one_hz(t, 5, False); t += 5000
        res = run_script(fr, 3_000_000, [t])
        cyc = recorded(flame_cycles(fr))
        self.assertEqual(len(cyc), 2)
        self.check(res, t, cyc, "S3b")

    def test_s4_a_flicker_is_not_recorded_and_its_off_edge_starts_the_next_gap(self):
        fr, t = [], 4_000_000
        fr += one_hz(t, 20, False); t += 20000
        fr += one_hz(t, 300, True); t += 300000
        fr += one_hz(t, 100, False); t += 100000
        fr += one_hz(t, 1, True); t += 1000      # a 1 s flame: 2 samples with the off edge, below the minimum of 3
        fr += one_hz(t, 200, False); t += 200000
        fr += one_hz(t, 300, True); t += 300000
        fr += one_hz(t, 5, False); t += 5000
        res = run_script(fr, 4_000_000, [t])
        allc = flame_cycles(fr)
        cyc = recorded(allc)
        self.assertEqual(len(allc), 3)
        self.assertEqual(len(cyc), 2, "the flicker is no cycle")
        self.assertEqual(cyc[1]["gap"], 200000, "the gap of the next cycle counts from the flicker's off edge")
        self.check(res, t, cyc, "S4")
        self.assertEqual(int(res[t]["cycles"]), 2)

    def test_s4b_a_missing_frame_inside_a_burn_does_not_shorten_it(self):
        fr, t = [], 5_000_000
        fr += one_hz(t, 20, False); t += 20000
        on = one_hz(t, 300, True)
        on = [x for x in on if not (t + 100000 <= x[0] < t + 130000)]   # 30 s without frames mid-burn
        fr += on; t += 300000
        fr += one_hz(t, 300, False); t += 300000
        fr += one_hz(t, 5, True); t += 5000
        res = run_script(fr, 5_000_000, [t])
        first = recorded(flame_cycles(fr))[0]
        self.assertEqual(first["on"], 300000)
        self.assertEqual(res[t]["records"][0]["on"], 300000, "duration comes from the edge times, not from a sample count")

    def test_s5_window_expiry_at_4h_and_24h(self):
        # cycle A ends at E, cycle B 5 h later, cycle C 25 h after A
        t = 6_000_000
        fr = one_hz(t, 10, False); t += 10000
        fr += one_hz(t, 300, True); t += 300000
        endA = t
        fr += one_hz(t, 5 * 3600, False); t += 5 * 3600 * 1000
        fr += one_hz(t, 300, True); t += 300000
        endB = t
        fr += one_hz(t, 5, False); t += 5000
        day = 24 * 3600 * 1000
        queries = [endA + H4, endA + H4 + 1, endB + 10, endA + day + 3600 * 1000, endB + day + 3600 * 1000]
        res = run_script(fr, 6_000_000, queries)
        cyc = recorded(flame_cycles(fr))
        # the ring is filled at the edge; the oracle ends are the edge times (frame time of the first no-flame frame)
        self.assertEqual(cyc[0]["end"], endA)
        # exactly 4 h after A: still inside (SATcycles.ino excludes only entries OLDER than the span), 1 ms later: gone
        self.assertEqual(int(res[endA + H4]["n4"]), 1, "A at exactly 4 h is in the window")
        self.assertEqual(int(res[endA + H4 + 1]["n4"]), 0, "A 1 ms older than 4 h is out")
        self.assertEqual(res[endA + H4 + 1]["on4"], 0.0)
        self.assertEqual(res[endA + H4 + 1]["duty4"], 0.0, "an empty window reports 0 together with n = 0, never the last value")
        # B is the only cycle in the 4 h window at endB + 10 s, and A and B are both in the 24 h one
        o = oracle_window(cyc, endB + 10, H4)
        self.assertEqual(int(res[endB + 10]["n4"]), o["n"])
        self.assertEqual(o["n"], 1)
        self.assertAlmostEqual(res[endB + 10]["on4"], 300.0, places=2)
        self.assertEqual(int(res[endB + 10]["n24"]), 2)
        # 25 h after A only B is left in the 24 h window (hour buckets: A's bucket is 25 h old); 25 h after B nothing is left and the
        # figures read 0 together with n = 0
        self.assertEqual(int(res[endA + day + 3600 * 1000]["n24"]), 1, "A is out of the 24 h window, B is in")
        self.assertEqual(int(res[endB + day + 3600 * 1000]["n24"]), 0)
        self.assertEqual(res[endB + day + 3600 * 1000]["duty24"], 0.0)

    def test_oracle_constants_match_the_source(self):
        src = CYCLES_INO.read_text(encoding="utf-8")
        self.assertRegex(src, r"EMA_ALPHA\s*=\s*0\.15f")
        self.assertRegex(src, r"SAT_CYCLE_MIN_SAMPLES\s*=\s*3;")
        self.assertRegex(src, r"SAT_WIN4H_SPAN_MS\s*=\s*4UL \* 3600UL \* 1000UL")


# ---- decisions 266-269: the telemetry follows the wire ---------------------------------------------------------------------------
UNKNOWN_GAP = 0xFFFFFFFF   # SAT_OFF_GAP_UNKNOWN (SATcycles.ino)


def check_first_cycle_gap_is_not_fabricated_when_the_flame_burns_at_boot():
    """O3 / entry 266 (Q-TW-1). The boiler already burns at the first observation, 20 s after SAT started. The frames show no
    flame-off edge, so no off gap is known (thermo-nova None, tracker.py:144-145): the first cycle is stored with an unknown gap and
    the 4 h average off gap and the duty EMA are built from the real gaps only (n counts them)."""
    fr, t = [], 7_000_000 + 20000
    fr += one_hz(t, 300, True); t += 300000
    fr += one_hz(t, 600, False); t += 600000
    fr += one_hz(t, 300, True); t += 300000
    fr += one_hz(t, 5, False); t += 5000
    res = run_script(fr, 7_000_000, [t])
    cyc = recorded(flame_cycles(fr))
    assert cyc[0]["gap"] is None and cyc[1]["gap"] == 600000
    r = res[t]
    assert [x["off"] for x in r["records"]] == [UNKNOWN_GAP, 600000], \
        "first cycle stores an off gap of %d ms for a flame that was already burning" % r["records"][0]["off"]
    o = oracle_window(cyc, t, H4)
    assert int(r["n4"]) == 2 and abs(r["off4"] - 600.0) < 0.01, "average off gap %.2f s over the real gaps only" % r["off4"]
    assert abs(r["on4"] - o["avg_on_s"]) < 0.01
    assert abs(r["duty4"] - 600.0 / 14400.0) < 1e-6, "both flames count in the duty: %.6f" % r["duty4"]
    assert abs(r["ema"] - oracle_ema(cyc)) < 1e-5, "the unknown gap is out of the duty EMA: %.6f vs %.6f" % (r["ema"], oracle_ema(cyc))


def check_mod_suppressed_follows_the_suppression_state():
    """O1 / entry 267 (Q-TW-2). state.sat.bModSuppressed (published as mod_suppressed) is true while satApplyPWM() returns the
    suppression CS (PWM ON phase, protection on, flame lit, hold elapsed) and false in every other phase; the real satApplyPWM
    runs in the PWM harness (PwmSecondsVsWire.rows)."""
    PwmSecondsVsWire.setUpClass()
    rows = PwmSecondsVsWire.rows
    try:
        assert rows, "no host C++ compiler"
        assert any(r["sup"] == 1 for r in rows), "bModSuppressed never true during the PWM ON phase after the hold"
        for r in rows:
            if r["sup"] == 1:
                assert r["flame"] == 1 and r["ch"] == 1, "suppressed while the flame is out or CH is off: %r" % r
            if r["ch"] == 0:
                assert r["sup"] == 0, "suppressed in the PWM OFF phase: %r" % r
    finally:
        PwmSecondsVsWire.tearDownClass()


def check_published_modulation_equals_the_mm_on_the_wire():
    """O2 / entry 269 (Q-TW-4). At the maximum PWM duty SAT sends MM=T (entries 242, 243, 246); the published current_modulation
    is 100 then, with the predicate of mmTarget, and satComputeMM()'s value otherwise."""
    src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
    fn = re.search(r"^static uint8_t satReportedModulation\([^\n]*\}$", src, re.M)
    assert fn, "no satReportedModulation(): the published current_modulation never shows the MM=T that went out"
    assert re.search(r"iCurrentModulation\s*=\s*satReportedModulation\(\s*!_sat_coldCut && satPwmAtMaxDuty\(\)", src), \
        "the control tick does not publish satReportedModulation() with the mmTarget predicate"
    assert re.search(r"mmTarget = \(!coldCutoff && satPwmAtMaxDuty\(\)\) \? SAT_MM_T", src)
    with tempfile.TemporaryDirectory() as d:
        cpp = Path(d) / "rm.cpp"
        cpp.write_text("#include <cstdint>\n#include <cstdio>\n#include <cstdlib>\n" + fn.group(0) + r"""
int main(int, char** a) { printf("%u\n", (unsigned)satReportedModulation(atoi(a[1]) != 0, (uint8_t)atoi(a[2]))); }
""", encoding="utf-8")
        exe = compile_cpp(cpp, Path(d) / "rm.exe")
        if not exe:
            raise unittest.SkipTest("no host C++ compiler")

        def out(m, v):
            return int(subprocess.run([str(exe), str(m), str(v)], capture_output=True, text=True).stdout)
        assert out(1, 0) == 100 and out(1, 37) == 100, "MM=T goes out: 100"
        assert out(0, 0) == 0 and out(0, 37) == 37 and out(0, 100) == 100, "otherwise the satComputeMM() value"


class TelemetryDecisions(unittest.TestCase):
    def test_o3_first_cycle_gap_of_a_flame_that_burned_at_boot(self):
        check_first_cycle_gap_is_not_fabricated_when_the_flame_burns_at_boot()

    def test_o3b_status_word_zero_before_the_first_frame_is_no_observation_of_the_flame(self):
        """Statusflags is 0 until the first MsgID 0 frame arrives: samples taken then must not make the gap 'known'."""
        fr, t = [], 7_500_000 + 40000
        fr += [(7_500_000 + i * 1000, status_frame(0, False, ch_mode=False)) for i in range(30)]   # an all-zero status word
        fr += one_hz(t, 300, True); t += 300000
        fr += one_hz(t, 5, False); t += 5000
        res = run_script(fr, 7_500_000, [t])
        self.assertEqual([x["off"] for x in res[t]["records"]], [UNKNOWN_GAP])
        self.assertEqual(res[t]["off4"], 0.0, "no real gap: the average is 0 with n = 0")

    def test_o1_mod_suppressed_follows_the_suppression_state(self):
        check_mod_suppressed_follows_the_suppression_state()

    def test_o2_current_modulation_equals_the_mm_on_the_wire(self):
        check_published_modulation_equals_the_mm_on_the_wire()

    def test_q3_duty_is_flame_seconds_over_the_window_and_counts_every_flame(self):
        """Entry 268: 2 cycles of 300 s in 4 h, one of them hot water only: duty = 600 / 14400, whatever the gaps are."""
        fr, t = [], 3_100_000
        fr += one_hz(t, 30, False); t += 30000
        fr += one_hz(t, 300, True, ch_enable=False, dhw=True); t += 300000
        fr += one_hz(t, 3000, False); t += 3000000
        fr += one_hz(t, 300, True); t += 300000
        fr += one_hz(t, 5, False); t += 5000
        res = run_script(fr, 3_100_000, [t])
        self.assertAlmostEqual(res[t]["duty4"], 600.0 / 14400.0, places=6)
        self.assertAlmostEqual(res[t]["duty24"], 600.0 / 86400.0, places=6)


# ---- the boot-time off gap that IS a measurement ----------------------------------------------------------------------
class BootGap(unittest.TestCase):
    def test_boot_with_the_flame_off_stores_the_observed_off_time(self):
        """Boot with the flame off, first flame 60 s later: the frames show 60 s of no flame, the code stores 60 s (S1 covers the
        numbers; this pins the boundary: the gap is measured from the first observation, never longer)."""
        fr, end = build_stream(8_000_000, 60, [(300, 0)])
        fr += one_hz(end, 5, False)
        end += 5000
        res = run_script(fr, 8_000_000, [end])
        self.assertEqual(res[end]["records"][0]["off"], 60000)


# ---- PWM seconds and duty against the CH-enable and flame bits ---------------------------------------------------------------
PWM_MAIN = hp.MAIN[:hp.MAIN.index("int main")] + r'''
int main(int argc, char** argv) {
  reset(); g_source=SAT_SRC_GAS_BOILER; settings.sat.iCyclesPerHour=3;
  g_flame=false; g_flameOnStart=g_ms-4000000UL; g_flameOffStart=g_ms-3000000UL;
  _pwm_lastUpdateMs=g_ms-3000000UL; _pwm_idle=false; _pwm_entryPending=false; state.sat.bPwmFlameRequested=false;
  const float request=38.6f;
  for(int i=0;i<200;i++) {
    const float cs=satApplyPWM(request);
    const unsigned ch=satChEnableDecide(request,g_cold,satPwmDrivesSetpoint(request),state.sat.bPwmFlameRequested);
    printf("t=%d flame=%d ch=%u cs=%.1f on_ms=%lu off_ms=%lu duty=%.4f sup=%d\n", i*30, (int)g_flame, ch, cs,
           (unsigned long)_pwm_lastOnTimeMs, (unsigned long)_pwm_lastOffTimeMs, state.sat.fPwmDutyCycle, (int)state.sat.bModSuppressed);
    g_ms+=30000UL;
    const bool want = state.sat.bPwmFlameRequested;   // the boiler lights / goes out on the tick after the request
    if (want && !g_flame) { g_flame=true; g_flameOnStart=g_ms; }
    if (!want && g_flame) { g_flame=false; g_flameOffStart=g_ms; }
  }
}
'''


class PwmSecondsVsWire(unittest.TestCase):
    TICK = 30

    @classmethod
    def setUpClass(cls):
        cls.dir = tempfile.TemporaryDirectory()
        old = hp.MAIN
        hp.MAIN = PWM_MAIN
        try:
            exe = hp.build(Path(cls.dir.name))
        finally:
            hp.MAIN = old
        cls.rows = None
        if exe:
            out = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60).stdout
            cls.rows = [{k: float(v) for k, v in (p.split("=") for p in ln.split())} for ln in out.splitlines()]

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def setUp(self):
        if self.rows is None:
            self.skipTest("no host C++ compiler")

    def frames(self):
        fr = []
        for r in self.rows:
            t = 10_000_000 + int(r["t"]) * 1000
            fr.append((t, status_frame(int(r["ch"]), bool(r["flame"]), ch_mode=bool(r["flame"]))))
        return fr

    def test_p1_reported_on_off_seconds_and_duty_match_the_flame_and_ch_bits(self):
        fr = self.frames()
        flame, ch = [], []
        for t, line in fr:
            _, hb, lb = decode(line)
            flame.append((t, bool(lb & 0x08)))
            ch.append((t, bool(hb & 1)))

        def runs(series, value):
            out, start = [], None
            for t, v in series:
                if v == value and start is None:
                    start = t
                if v != value and start is not None:
                    out.append((start, t)); start = None
            return out
        on_runs = runs(flame, True)           # complete flame-on runs: a start and an end both seen
        low_runs = runs(ch, False)            # complete CH-low runs = the OFF phase as the boiler sees it
        self.assertGreaterEqual(len(on_runs), 3)
        self.assertGreaterEqual(len(low_runs), 3)
        rep_on = (self.rows[0]["on_ms"] + 500) // 1000    # satPwmNarrateEdge rounds ms to whole seconds
        rep_off = (self.rows[0]["off_ms"] + 500) // 1000
        for a, b in on_runs:
            self.assertLessEqual(abs((b - a) / 1000.0 - rep_on), self.TICK, "flame-on seconds on the wire vs pwm_on")
        for a, b in low_runs[:3]:
            self.assertLessEqual(abs((b - a) / 1000.0 - rep_off), self.TICK, "CH-low seconds on the wire vs pwm_off")
        # duty over complete periods (CH rise to CH rise): measured flame fraction vs the published duty
        rises = [t for (t, v), (_, pv) in zip(ch[1:], ch[:-1]) if v and not pv]
        self.assertGreaterEqual(len(rises), 2)
        period = (rises[1] - rises[0]) / 1000.0
        flame_in = sum(min(b, rises[1]) - max(a, rises[0]) for a, b in on_runs if b > rises[0] and a < rises[1]) / 1000.0
        self.assertLessEqual(abs(flame_in / period - self.rows[0]["duty"]), 0.05, "flame fraction of a period vs fPwmDutyCycle")

    def test_p1b_cs_during_off_is_the_minimum_with_ch_low(self):
        for r in self.rows:
            if r["ch"] == 0:
                self.assertEqual(r["cs"], 10.0)


# ---- MM value and unit --------------------------------------------------------------------------------------------------
class MmValue(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        src = SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        fn = re.search(r"^static uint8_t satComputeMM\(.*?^\}", src, re.S | re.M).group(0)
        quirk = re.search(r"^(?:static )?const \w+ SAT_QUIRK_MIN_MOD_10\s*=[^;]*;|^#define SAT_QUIRK_MIN_MOD_10[^\n]*|SAT_QUIRK_MIN_MOD_10\s*=\s*[^,;}]*", src, re.M)
        decl = "static const uint8_t SAT_QUIRK_MIN_MOD_10 = 1;\n" if not quirk else ""
        cls.dir = tempfile.TemporaryDirectory()
        cpp = Path(cls.dir.name) / "mm.cpp"
        cpp.write_text("#include <cstdint>\n#include <cstdio>\n#include <cstdlib>\n" + decl + fn + r'''
int main(int, char** a) { printf("%u\n", (unsigned)satComputeMM(atoi(a[1]), atoi(a[2]), atoi(a[3]), atoi(a[4]), atoi(a[5]))); }
''', encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.dir.name) / "mm.exe")

    @classmethod
    def tearDownClass(cls):
        cls.dir.cleanup()

    def mm(self, pwm, hp_, hyb, maxrel, quirks=0):
        if not self.exe:
            self.skipTest("no host C++ compiler")
        return int(subprocess.run([str(self.exe), str(int(pwm)), str(int(hp_)), str(int(hyb)), str(maxrel), str(quirks)],
                                  capture_output=True, text=True).stdout)

    def test_m1_value_is_a_whole_percentage_never_the_mm_t_marker(self):
        """The published current_modulation is satComputeMM()'s result: 0-100, a percentage (the OT MaxRelModLevelSetting, MsgID 14,
        carries the same percentage in its high byte). SAT_MM_T (255) is a separate marker that only the sender sees."""
        for pwm in (0, 1):
            for hpv in (0, 1):
                for maxrel in (0, 30, 100):
                    v = self.mm(pwm, hpv, hpv, maxrel)
                    self.assertLessEqual(v, 100, "pwm=%d hp=%d max=%d" % (pwm, hpv, maxrel))
        self.assertEqual(self.mm(1, 0, 0, 100), 0, "gas boiler in PWM: MM 0 in both phases")
        self.assertEqual(self.mm(1, 1, 1, 100), 100, "heat pump in PWM keeps its configured maximum")
        self.assertEqual(self.mm(1, 1, 1, 0), 100, "heat pump never gets 0")

    def test_m1b_units_are_percent_in_every_publisher(self):
        src = SAT_INO.read_text(encoding="utf-8")
        self.assertRegex(src, r'je\.field\(F\("current_modulation"\),\s*\(int32_t\)snap->st\.sat\.iCurrentModulation\)')
        self.assertRegex(src, r'publishIfChangedI\(F\("sat/current_modulation"\)')
        rest = (FW / "restAPI.ino").read_text(encoding="utf-8")
        self.assertRegex(rest, r'je\.field\(F\("state\.sat\.current_mod"\),\s*\(int32_t\)snap->st\.sat\.iCurrentModulation\)')

    def test_m1c_the_wire_string_is_mm_percent_or_mm_t(self):
        src = SAT_INO.read_text(encoding="utf-8")
        self.assertIn('PSTR("MM=%u")', src)
        self.assertIn('PSTR("MM=T")', src)


if __name__ == "__main__":
    unittest.main(verbosity=2)
