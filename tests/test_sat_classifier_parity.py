#!/usr/bin/env python3
"""TASK-1412.14 / LL-021: input and verdict parity of the end-of-cycle classifier, thermo-nova versus the firmware port.

Both sides run the REAL code on the SAME timestamped samples:
  upstream  the pinned thermo-nova files (commit 7beaaf5, other-projects/SAT-releases-thermo-nova/custom_components/sat):
            cycles/tracker.py (CycleTracker.update -> _build_cycle_state -> _build_metrics), cycles/classifier.py,
            cycles/history.py, cycles/const.py, cycles/types.py, and the package files const.py, types.py, helpers.py
            they import (percentile_interpolated is helpers.py:187). The originals are never edited: the package is COPIED
            byte for byte into a temporary directory, its sha256 is compared with the pinned value below, and only
            homeassistant (core.State, core.HomeAssistant, util.dt) is a stub module. DeviceState, PWMState and
            ControlLoopSample are duck-typed namespaces (device.py / pwm.py / heating_control.py import Home Assistant
            entities and are not part of the classifier path); the attributes the classifier path reads are listed in
            sample_for().
  port      the real SATcycles.ino functions, cut out and compiled on the host exactly as tests/test_sat_cycle_classifier.py
            does (satCycleOnFlameChange, satCycleSample, _cycleClassify, the tail rings, the kind counters).

Driver order per tick equals the production loop (SATcontrol.ino:6592 then :6599): the flame edge is processed first, then
satCycleSample(), so the flame-on tick and the flame-off tick take no tail sample in the port, while thermo-nova's
CycleTracker.update appends the flame-on sample and not the flame-off one (tracker.py:71-72). A scenario is a list of
timestamped ticks t = 0 .. N (flame on for t < N, the tick at t = N is the flame-off edge). Seconds in `gaps` have no tick on
either side (missing samples).

Two configurations, because the port has decided deviations built in:
  parity    overshoot margin 3.0 (cycles/const.py:10) and cold setpoint 22 (const.py:34), upstream untouched
  firmware  overshoot margin 2.0 (entries 15, 37) and cold setpoint 28.2 (entries 7, 11, 200); the upstream module globals
            OVERSHOOT_MARGIN_CELSIUS and COLD_SETPOINT of classifier.py are set to those values for the run and restored
            (the only change to the loaded code; no file is modified)

Every verdict difference between the two sides must appear in DIFFERENCES with its classification:
  (a) already a RECORDED deviation: a decision of Sergeant D in docs/sat/sergeant-d-corrections.md (entry number given);
  (b) UNRECORDED: not decided by him; listed as an open question (Q-PARITY-n); behaviour is not changed here.
A difference that is not listed fails the run, and a listed difference that no longer occurs fails too (remove it when fixed).
The last test, TestParityGate.test_no_unrecorded_difference_remains, is an expectedFailure: it turns into an unexpected success
(a failure) the day class (b) is empty, which is the signal that TASK-1331 may resume.

Run: python tests/test_sat_classifier_parity.py   (skips with a reason when the upstream cannot be imported, or no host compiler)
"""
import hashlib
import math
import os
import re
import subprocess
import sys
import tempfile
import types as pytypes
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
import test_sat_cycle_classifier as base  # noqa: E402  (PRELUDE, STUBS, extract_*, compile_cpp, paths)

TN = base.TN
UPSTREAM_COMMIT = "7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb"

# sha256 of the pinned upstream files, over the content with CRLF normalised to LF (the checkout may have either).
PINNED_SHA256 = {
    "cycles/classifier.py": "a2a97462ee56f0dd918927c862bc02f635064a1fc0ae62c789c170eb8682fbb3",
    "cycles/tracker.py": "2aa1f4fb6a368a076ff0e27b98542b69ad7d33cc0080df6cd53212e2da76188f",
    "cycles/history.py": "3bb9056cb03be480c8de703052234b00b4690d03320325a0fb46971e116deb1b",
    "cycles/const.py": "1fb9b31400194b3464b0cafbf374b6bcbd0a3687d42e17aa4cb07731ad62b755",
    "cycles/types.py": "c1d1c651df9bf3f2cbd400f8b3971ad286d15605f52fe4907221d301114bff94",
    "cycles/__init__.py": "57ab179d63e9e5e3cdfa9a45839228f14cb78e1222ec0dce5b0f9ccb05b58dc6",
    "const.py": "e7104c9b51b2394a8b251331f9d0d6ca24bbc501daf675c75628eb859833ea9e",
    "types.py": "4ef5f4676793a01ca9e10d75a64777de70586bbb3900345e7cd7e04070a87184",
    "helpers.py": "aa1645d5e95efefe614b15b7005a8acbc75e5e4a9ca17448d94f93a1a582721c",
}

CH = 0x0100 | 0x02 | 0x08          # master CH enable + slave CH mode + flame
DHW = 0x04 | 0x08                  # slave DHW mode + flame, no master CH enable
CHDHW = CH | 0x04                  # CH enabled but the boiler is on DHW
NONE_FLAGS = 0x08                  # flame only: neither CH nor DHW
CONT, PWM = 1, 2

CONFIGS = {
    "parity": {"margin": 3.0, "cold": 22.0},
    "firmware": {"margin": 2.0, "cold": 28.2},
}


def lf_sha(data: bytes) -> str:
    return hashlib.sha256(data.replace(b"\r\n", b"\n")).hexdigest()


# --- upstream loader --------------------------------------------------------------------------------------------------------
class UpstreamUnavailable(Exception):
    pass


_UP = {}


def load_upstream():
    """Copy the pinned package to a temp dir, verify the hashes, import it with a stubbed homeassistant. Returns a namespace."""
    if _UP:
        return _UP["ns"]
    if sys.version_info < (3, 11):
        raise UpstreamUnavailable("thermo-nova needs Python >= 3.11 (enum.StrEnum); running %s" % sys.version.split()[0])
    if not (TN / "cycles" / "classifier.py").exists():
        raise UpstreamUnavailable("other-projects/SAT-releases-thermo-nova is not checked out")
    tmp = tempfile.TemporaryDirectory(prefix="tn_pkg_")
    root = Path(tmp.name) / "tnsat"
    (root / "cycles").mkdir(parents=True)
    hashes = {}
    for rel, want in PINNED_SHA256.items():
        data = (TN / rel).read_bytes()
        got = lf_sha(data)
        if got != want:
            raise UpstreamUnavailable("%s sha256 %s differs from the pinned %s (commit %s): the local copy is not the pinned code"
                                      % (rel, got, want, UPSTREAM_COMMIT[:7]))
        (root / rel).write_bytes(data)
        hashes[rel] = got
        if lf_sha((root / rel).read_bytes()) != want:
            raise UpstreamUnavailable("copy of %s differs from the original" % rel)
    (root / "__init__.py").write_text("")   # replaces sat/__init__.py (the Home Assistant integration entry point, not on the classifier path)

    # homeassistant stubs: helpers.py imports core.State and util.dt, tracker.py core.HomeAssistant
    stubs = {}
    ha = pytypes.ModuleType("homeassistant"); ha.__path__ = []
    core = pytypes.ModuleType("homeassistant.core")
    core.State = type("State", (), {})
    core.HomeAssistant = type("HomeAssistant", (), {})
    util = pytypes.ModuleType("homeassistant.util"); util.__path__ = []
    dt = pytypes.ModuleType("homeassistant.util.dt")
    util.dt = dt
    ha.core, ha.util = core, util
    for name, mod in (("homeassistant", ha), ("homeassistant.core", core), ("homeassistant.util", util), ("homeassistant.util.dt", dt)):
        stubs[name] = sys.modules.get(name)
        sys.modules[name] = mod
    for name in [n for n in sys.modules if n == "tnsat" or n.startswith("tnsat.")]:
        del sys.modules[name]
    sys.path.insert(0, tmp.name)
    old = sys.dont_write_bytecode
    sys.dont_write_bytecode = True
    try:
        import importlib
        tracker = importlib.import_module("tnsat.cycles.tracker")
        classifier = importlib.import_module("tnsat.cycles.classifier")
        history = importlib.import_module("tnsat.cycles.history")
        types_ = importlib.import_module("tnsat.types")
        helpers = importlib.import_module("tnsat.helpers")
    except Exception as e:   # noqa: BLE001  (report why the upstream cannot run)
        raise UpstreamUnavailable("importing the pinned thermo-nova copy failed: %r" % (e,))
    finally:
        sys.dont_write_bytecode = old
        sys.path.remove(tmp.name)
        for name, mod in stubs.items():
            if mod is None:
                sys.modules.pop(name, None)
            else:
                sys.modules[name] = mod
    ns = pytypes.SimpleNamespace(tracker=tracker, classifier=classifier, history=history, types=types_, helpers=helpers,
                                 hashes=hashes, tmp=tmp,
                                 orig=(classifier.OVERSHOOT_MARGIN_CELSIUS, classifier.COLD_SETPOINT))
    _UP["ns"] = ns
    return ns


# --- scenarios ----------------------------------------------------------------------------------------------------------------
DEFAULTS = dict(flow=50.0, flags=CH, mode=CONT, on=0, ctrl=50.0, req=50.0)


class Scenario:
    """N seconds of flame; f(t) overrides the DEFAULTS per second (None = value missing); gaps = seconds without any tick."""

    def __init__(self, name, desc, n, f=None, gaps=(), on_time=0, idle=False, cfgs=("parity", "firmware")):
        self.name, self.desc, self.n, self.f = name, desc, n, f or (lambda t: {})
        self.gaps, self.on_time, self.idle, self.cfgs = set(gaps), on_time, idle, cfgs

    def ticks(self):
        out = []
        for t in range(0, self.n + 1):
            if t in self.gaps:
                continue
            d = dict(DEFAULTS)
            d.update(self.f(t))
            d["t"] = t
            d["flame"] = 1 if t < self.n else 0
            out.append(d)
        return out


def window(a, b, **kw):
    """f-helper: kw for a <= t < b."""
    return lambda t: kw if a <= t < b else {}


def combine(*fs):
    def f(t):
        d = {}
        for g in fs:
            d.update(g(t))
        return d
    return f


def build_scenarios():
    S = []
    add = lambda *a, **k: S.append(Scenario(*a, **k))   # noqa: E731

    add("normal_burn", "300 s, flow = control = requested = 50", 300)
    add("short_burn_continuous", "40 s continuous burn, no error", 40)
    add("short_burn_pwm_on", "100 s burn ending in a PWM ON phase, mapped on-time 300 s (< 0.8 x 300 = 240)", 100,
        lambda t: dict(mode=PWM, on=1), on_time=300)
    add("overshoot_burn", "300 s, flow 54 over control 50", 300, lambda t: dict(flow=54.0))
    add("underheat_burn", "300 s, flow 45 under control 50", 300, lambda t: dict(flow=45.0))
    add("underheat_cold_setpoint", "300 s, flow 15, control = requested = 25 (below 28.2, above 22)", 300,
        lambda t: dict(flow=15.0, ctrl=25.0, req=25.0))

    # CS change mid-burn (control setpoint per sample)
    add("cs_change_before_tail", "control 50 -> 54 at t=100, flow 54 throughout (tail 120..299 has error 0)", 300,
        lambda t: dict(flow=54.0, ctrl=54.0 if t >= 100 else 50.0))
    add("cs_change_inside_tail", "control 50 -> 54 at t=200, flow 54 (tail errors +4 x80, 0 x100)", 300,
        lambda t: dict(flow=54.0, ctrl=54.0 if t >= 200 else 50.0))
    add("cs_lowered_inside_tail", "control 54 -> 50 at t=240, flow 50 (tail errors -4 x120, 0 x60)", 300,
        lambda t: dict(flow=50.0, ctrl=50.0 if t >= 240 else 54.0))

    # mode change mid-burn
    add("mode_cont_to_pwm_good", "continuous until t=150, then PWM ON (on-time 300 s), flow 50", 300,
        lambda t: dict(mode=PWM, on=1) if t >= 150 else {}, on_time=300)
    add("mode_cont_to_pwm_underheat", "continuous until t=150, then PWM ON, flow 45 / requested 50", 300,
        lambda t: dict(mode=PWM, on=1, flow=45.0) if t >= 150 else dict(flow=45.0), on_time=300)
    add("mode_pwm_to_cont_overshoot", "PWM until t=150, then continuous, flow 54 / control 50", 300,
        lambda t: dict(flow=54.0) if t >= 150 else dict(mode=PWM, on=1, flow=54.0), on_time=300)
    add("mode_and_cs_change", "continuous until t=150 (control 50), PWM after with control 54 / requested 54, flow 54", 300,
        lambda t: dict(mode=PWM, on=1, flow=54.0, ctrl=54.0, req=54.0) if t >= 150 else dict(flow=54.0), on_time=300)

    # missing data
    add("control_missing_all", "control setpoint missing for the whole burn", 300, lambda t: dict(ctrl=None))
    add("control_missing_part_of_tail", "control missing t>=200 (flow 58 there); t<200 error 0", 300,
        lambda t: dict(ctrl=None, flow=58.0) if t >= 200 else {})
    add("flow_missing_continuous", "flow missing t=250..299 (50 of the 180 tail seconds), else error 0", 300,
        window(250, 300, flow=None))
    add("flow_missing_pwm_underheat", "PWM ON, flow 46 (error -4), flow missing t=250..299", 300,
        combine(lambda t: dict(mode=PWM, on=1, flow=46.0), window(250, 300, flow=None)), on_time=300)
    add("requested_missing_all_pwm", "PWM ON, requested setpoint missing for the whole burn", 300,
        lambda t: dict(mode=PWM, on=1, req=None), on_time=300)
    add("requested_missing_last_third_pwm", "PWM ON, requested missing t>=200, flow 50 = requested 50 before", 300,
        lambda t: dict(mode=PWM, on=1, req=None) if t >= 200 else dict(mode=PWM, on=1), on_time=300)
    add("sample_gap_in_tail", "N=400, no sample t=250..349, flow 62 (error +12) only for t<220", 400,
        lambda t: dict(flow=62.0) if t < 220 else {}, gaps=range(250, 350))

    # warm-up exclusion
    add("warmup_high_flow_short_cycle", "N=100: flow 62 (error +12) for t<20, 50 after (tracker warm-up = 25 s)", 100,
        window(0, 20, flow=62.0))
    add("warmup_dhw_out", "N=100: DHW flag at t=5 (inside the 25 s warm-up)", 100, window(5, 6, flags=CHDHW))
    add("warmup_dhw_in", "N=100: DHW flag at t=30 (inside the tail)", 100, window(30, 31, flags=CHDHW))
    add("dhw_exactly_at_tail_start", "N=500: DHW flag at t=320 = N-180 (tail starts there, >=)", 500, window(320, 321, flags=CHDHW))
    add("dhw_just_before_tail_start", "N=500: DHW flag at t=319 (one second before the tail)", 500, window(319, 320, flags=CHDHW))
    add("dhw_only_cycle", "300 s on hot water", 300, lambda t: dict(flags=DHW))
    add("kind_unknown", "300 s flame with neither CH enable nor DHW", 300, lambda t: dict(flags=NONE_FLAGS))
    add("kind_ch_exactly_80pct", "N=10: samples t=0 and t=9 neither, t=1..8 CH (upstream 8 of 10 = 0.8, not > 0.8; port sees 8 of 9)", 10,
        lambda t: dict(flags=NONE_FLAGS) if t in (0, 9) else {})
    add("pwm_idle_at_end", "PWM mode, status IDLE at flame-off", 300, lambda t: dict(mode=PWM, on=0), on_time=300, idle=True)

    # sample count
    add("three_second_cycle", "N=3: thermo-nova has samples t=0,1,2 (3, minimum 3); the port takes t=1,2", 3)
    add("two_second_cycle", "N=2: thermo-nova has 2 samples (< 3, cycle dropped); the port records INSUFFICIENT", 2)

    # percentile interpolation (180-sample tail, N=240: tail t=60..239, warm-up 60 s)
    def p90_case(low, high):
        # 18 highest of the 180 tail samples at error `high`, the other 162 at error `low`
        return lambda t: dict(flow=50.0 + high) if t >= 222 else dict(flow=50.0 + low)
    add("interp_p90_over_margin3", "N=240: tail errors 2.5 x162, 8.0 x18; interpolated p90 = 3.05, floor p90 = 2.5", 240, p90_case(2.5, 8.0))
    add("interp_p90_over_margin2", "N=240: tail errors 1.5 x162, 8.0 x18; interpolated p90 = 2.15, floor p90 = 1.5", 240, p90_case(1.5, 8.0))
    add("interp_p50_underheat", "N=240: tail errors -5 x90 (first), 0 x90; interpolated p50 = -2.5, floor p50 = -5", 240,
        lambda t: dict(flow=45.0) if 60 <= t < 150 else {})

    # boundary margins (error = flow - control 50; the exact value, one representable step and 0.004 either side)
    for cfg, m in (("parity", 3.0), ("firmware", 2.0)):
        for tag, off in (("below_025", -0.25), ("below_0004", -0.004), ("exact", 0.0), ("above_025", 0.25)):
            add("overshoot_boundary_%s_%s" % (cfg, tag), "error %+.3f at margin %.1f (all 180 tail seconds)" % (m + off, m), 300,
                (lambda fl: (lambda t: dict(flow=fl)))(50.0 + m + off), cfgs=(cfg,))
    for tag, off in (("below_025", -0.25), ("below_0004", -0.004), ("exact", 0.0), ("above_0004", 0.004), ("above_025", 0.25)):
        add("underheat_boundary_cont_%s" % tag, "continuous, error %+.3f against -3.0" % (-3.0 + off), 300,
            (lambda fl: (lambda t: dict(flow=fl)))(50.0 - 3.0 + off))
        add("underheat_boundary_pwm_%s" % tag, "PWM ON, requested error %+.3f against -3.0" % (-3.0 + off), 300,
            (lambda fl: (lambda t: dict(flow=fl, mode=PWM, on=1)))(50.0 - 3.0 + off), on_time=300)
    add("cold_boundary_exact_22", "continuous underheat with control = requested = 22 exactly (cold 22: not below)", 300,
        lambda t: dict(flow=10.0, ctrl=22.0, req=22.0), cfgs=("parity",))
    add("cold_boundary_exact_28_2", "continuous underheat with control = requested = 28.2 exactly", 300,
        lambda t: dict(flow=10.0, ctrl=28.2, req=28.2), cfgs=("firmware",))
    add("pwm_short_boundary_exact", "PWM ON, duration 240 = 0.8 x 300 exactly: not short", 240, lambda t: dict(mode=PWM, on=1), on_time=300)
    add("pwm_short_boundary_below", "PWM ON, duration 239 < 240: short", 239, lambda t: dict(mode=PWM, on=1), on_time=300)

    # requested setpoint is per sample upstream, one value at flame-off in the port
    add("requested_raised_inside_tail_pwm", "PWM ON, flow 50, requested 50 until t=199 then 60 (per sample p90 error 0; last value -10)", 300,
        lambda t: dict(mode=PWM, on=1, req=60.0 if t >= 200 else 50.0), on_time=300)
    add("requested_lowered_below_cold_continuous", "continuous underheat (flow 45, control 50), requested 50 until t=249 then 20 (per sample p50 = 50)", 300,
        lambda t: dict(flow=45.0, req=20.0 if t >= 250 else 50.0))
    add("control_zero_value", "control setpoint is a real 0.0 (not missing), flow 50: error +50", 300, lambda t: dict(ctrl=0.0))
    add("device_setpoint_fallback_cold", "continuous underheat, requested missing, control 30 at flame-on then 20 from t=100 (device setpoint 20 at the end)", 300,
        lambda t: dict(flow=15.0, req=None, ctrl=20.0 if t >= 100 else 30.0))
    return S


SCENARIOS = build_scenarios()


# --- upstream run ---------------------------------------------------------------------------------------------------------------
def sample_for(up, tk, on_time, idle):
    """One thermo-nova ControlLoopSample. Attributes read on the classifier path: sample.timestamp, .control_setpoint,
    .requested_setpoint, .pwm (enabled, status, on_time_seconds), .device_state (flame_active, hot_water_active,
    central_heating, flow_temperature, return_temperature, relative_modulation_level, setpoint)."""
    flags = tk["flags"]
    status = up.types.PWMStatus.IDLE if idle else (up.types.PWMStatus.ON if tk["on"] else up.types.PWMStatus.OFF)
    dev = pytypes.SimpleNamespace(
        flame_active=bool(tk["flame"]),
        hot_water_active=bool(flags & 0x04),
        central_heating=bool(flags & 0x0100),
        flow_temperature=tk["flow"], return_temperature=30.0, relative_modulation_level=None,
        setpoint=tk["ctrl"])
    pwm = pytypes.SimpleNamespace(enabled=tk["mode"] == PWM, status=status, on_time_seconds=on_time)
    return pytypes.SimpleNamespace(timestamp=float(tk["t"]), device_state=dev, pwm=pwm,
                                   control_setpoint=tk["ctrl"], requested_setpoint=tk["req"])


VERDICT = {"INSUFFICIENT_DATA": "INSUFFICIENT", "SHORT_CYCLING": "SHORT"}


def run_upstream(sc, cfg):
    up = load_upstream()
    clf = up.classifier
    clf.OVERSHOOT_MARGIN_CELSIUS = CONFIGS[cfg]["margin"]
    clf.COLD_SETPOINT = CONFIGS[cfg]["cold"]
    try:
        events = []
        hass = pytypes.SimpleNamespace(bus=pytypes.SimpleNamespace(fire=lambda name, data: events.append((name, data))))
        tracker = up.tracker.CycleTracker(hass, up.history.CycleHistory())
        last = None
        for tk in sc.ticks():
            last = tk
            tracker.update(sample_for(up, tk, sc.on_time, sc.idle))
        cycles = [d["cycle"] for n, d in events if "cycle" in d]
    finally:
        clf.OVERSHOOT_MARGIN_CELSIUS, clf.COLD_SETPOINT = up.orig
    if not cycles:
        return dict(verdict="DROPPED", kind="-", detail="no Cycle emitted (tracker.py:98-100, fewer than 3 samples)")
    c = cycles[-1]
    name = c.classification.name
    raw = VERDICT.get(name, name)
    v = raw
    if v == "UNDERHEAT" and last["mode"] == PWM:
        v = "UNDERHEAT_PWM"     # the port's label for the PWM branch (classifier.py:54 returns UNDERHEAT for both)
    t = c.tail
    kind = {"central_heating": "CH", "domestic_hot_water": "DHW", "mixed": "MIXED", "unknown": "UNKNOWN"}[c.kind.value]

    def f(x):
        return "-" if x is None else "%.4g" % x
    return dict(verdict=v, raw=raw, kind=kind, n=c.sample_count,
                detail="n=%d ctrlErr p50/p90=%s/%s reqErr p50/p90=%s/%s" % (
                    c.sample_count, f(t.flow_control_setpoint_error.p50), f(t.flow_control_setpoint_error.p90),
                    f(t.flow_requested_setpoint_error.p50), f(t.flow_requested_setpoint_error.p90)))


# --- port run -------------------------------------------------------------------------------------------------------------------
MAIN = r"""
static void applyTick(float flow, unsigned flags, int mode, int on) {
  g_flow = flow;
  OTcurrentSystemState.Statusflags = (uint16_t)flags;
  OTcurrentSystemState.MasterStatus = (uint8_t)(flags >> 8);
  OTcurrentSystemState.SlaveStatus = (uint8_t)(flags & 0xFF);
  state.sat.bDhwActive = (flags & 0x04) != 0;
  state.sat.eControlMode = (SATControlMode)mode;
  state.sat.bPwmFlameRequested = on != 0;
}
// argv: sizes | margin cold scenario-file ; scenario line: t flame flow|nan flags mode on ctrl req idle onTimeSec
int main(int argc, char** argv) {
  if (argc > 1 && strcmp(argv[1], "sizes") == 0) {
    printf("SAT_TAIL_SAMPLE_SIZE=%d tail_samples=%zu tail_ctrlErr=%zu flow_samples=%zu\n", (int)SAT_TAIL_SAMPLE_SIZE,
           sizeof(_tail_samples), sizeof(_tail_ctrlErr), sizeof(_flow_samples));
    return 0;
  }
  if (argc < 4) return 2;
  settings.sat.fOvershootMargin = (float)atof(argv[1]);
  g_cold = (float)atof(argv[2]);
  settings.sat.bSimulation = false;
  FILE* fp = fopen(argv[3], "r");
  if (!fp) return 3;
  char line[256];
  bool prevFlame = false;
  while (fgets(line, sizeof(line), fp)) {
    long t; int flame, mode, on, idle; char flowS[40]; unsigned flags; float ctrl, req, onTime;
    if (sscanf(line, "%ld %d %39s %u %d %d %f %f %d %f", &t, &flame, flowS, &flags, &mode, &on, &ctrl, &req, &idle, &onTime) != 10) return 4;
    g_ms = 1000000UL + (uint32_t)t * 1000UL;
    const float flow = (strcmp(flowS, "nan") == 0) ? NAN : (float)atof(flowS);
    applyTick(flow, flags, mode, on);
    state.sat.fFinalSetpoint = ctrl;
    _sat_requestedSetpoint = req;
    state.sat.fPidOutput = req;
    g_idle = idle != 0;
    g_onTimeMs = (uint32_t)(onTime * 1000.0 + 0.5);
    if (!flame && prevFlame) {
      float a = 0, b = 0;
      const bool known = _tailCtrlErrPercentiles(a, b);
      printf("diag tailN=%u ctrlKnown=%d ctrlP50=%.4g ctrlP90=%.4g tailP50=%.4g tailP90=%.4g samples=%lu\n", (unsigned)_tail_sampleCount,
             known ? 1 : 0, (double)a, (double)b, (double)_tailPercentile(50), (double)_tailPercentile(90), (unsigned long)_cycle_totalSamples);
    }
    if ((flame != 0) != prevFlame) { satCycleOnFlameChange(flame != 0); prevFlame = flame != 0; }
    satCycleSample();     // production order: SATcontrol.ino:6592 flame edge, :6599 sample
  }
  fclose(fp);
  printf("cls=%d kind=%d\n", (int)state.sat.eLastCycleClass, (int)state.sat.eLastCycleKind);
  return 0;
}
"""

_PORT = {}


def port_exe():
    if "exe" in _PORT:
        return _PORT["exe"]
    tmp = tempfile.TemporaryDirectory(prefix="port_parity_")
    wd = Path(tmp.name)
    cycles = base.CYCLES_INO.read_text(encoding="utf-8")
    sat = base.SAT_INO.read_text(encoding="utf-8")
    types_src = base.SATTYPES_H.read_text(encoding="utf-8").replace("\r\n", "\n")
    body = base.extract_cycles(cycles)
    last_err = None
    for real in (True, False):
        stubs = (base.STUBS.replace("@SETTINGS@", base.SETTINGS_REAL if real else base.SETTINGS_STUB)
                 .replace("@IDLE_RET@", base.idle_return_type(sat)))
        cpp = wd / ("p_real.cpp" if real else "p_stub.cpp")
        cpp.write_text(base.PRELUDE + base.extract_types(types_src, real) + stubs + body + MAIN, encoding="utf-8")
        try:
            exe = base.compile_cpp(cpp, wd / ("p_real.exe" if real else "p_stub.exe"))
            _PORT.update(exe=exe, tmp=tmp, variant="real SATSection" if real else "settings stub")
            return exe
        except (AssertionError, subprocess.CalledProcessError) as e:
            last_err = e
    raise AssertionError("host compile failed:\n%s" % last_err)


def run_port(sc, cfg):
    exe = port_exe()
    lines = []
    for tk in sc.ticks():
        flow = "nan" if tk["flow"] is None else repr(float(tk["flow"]))
        ctrl = 0.0 if tk["ctrl"] is None else tk["ctrl"]
        req = 0.0 if tk["req"] is None else tk["req"]
        lines.append("%d %d %s %d %d %d %r %r %d %r" % (tk["t"], tk["flame"], flow, tk["flags"], tk["mode"], tk["on"], float(ctrl),
                                                          float(req), 1 if sc.idle else 0, float(sc.on_time)))
    path = Path(_PORT["tmp"].name) / ("%s.txt" % sc.name)
    path.write_text("\n".join(lines) + "\n")
    r = subprocess.run([str(exe), repr(CONFIGS[cfg]["margin"]), repr(CONFIGS[cfg]["cold"]), str(path)], capture_output=True, text=True, timeout=30)
    if r.returncode != 0:
        raise AssertionError("port harness exit %d: %s%s" % (r.returncode, r.stdout, r.stderr[-300:]))
    m = re.search(r"cls=(\d+) kind=(\d+)", r.stdout)
    if not m:
        raise AssertionError(r.stdout)
    d = re.search(r"diag (.*)", r.stdout)
    return dict(verdict=base.CLS_NAME[int(m.group(1))], kind=base.KIND_NAME[int(m.group(2))], detail=d.group(1) if d else "")


# --- classification of every difference -------------------------------------------------------------------------------------------
# key (scenario, config) -> (class, upstream verdict, port verdict, why, upstream citation, port citation, entry or open question)
# Citations: upstream relative to custom_components/sat at 7beaaf5; port in src/OTGW-firmware/SATcycles.ino at this revision.
W = "tracker.py:113-116 (warm-up min(60, 0.25 x duration), tail_start = max(.., end - 180)), :235-241 (tail = samples with timestamp >= tail_start)"
PW = "SATcycles.ino:1009-1024 (tail ring: the last 180 1 Hz samples, no warm-up), :909-911 (percentiles from that ring)"
INTERP_UP = "helpers.py:187-203 percentile_interpolated, used by tracker.py:252-254"
INTERP_PORT = "SATcycles.ino:633 and :662 (index = pct x (n-1) / 100, integer floor, no interpolation)"
REQ_UP = "tracker.py:273-277, :249-250 (flow - requested_setpoint per sample; samples without one are left out)"
REQ_PORT = "SATcycles.ino:918 (requestedSp = _sat_requestedSetpoint at flame-off), :790, :798 (tailP90 - requestedSp)"
QUANT_PORT = "SATcycles.ino:1019-1022 (error stored as int16 hundredths, lroundf), :662"
MIN3_UP = "tracker.py:96-100 (fewer than 3 samples: no Cycle is built or emitted)"

DIFFERENCES = {}


def diff(scenario, cfgs, klass, up, port, why, up_cite, port_cite, ref):
    for c in cfgs:
        DIFFERENCES[(scenario, c)] = dict(klass=klass, up=up, port=port, why=why, up_cite=up_cite, port_cite=port_cite, ref=ref)


BOTH = ("parity", "firmware")
# Q-PARITY-1 percentile rule
diff("interp_p90_over_margin3", ("parity",), "b", "OVERSHOOT", "GOOD", "p90 of 180 samples sits between two sorted values: interpolated 3.05, floor 2.5",
     INTERP_UP, INTERP_PORT, "Q-PARITY-1")
diff("interp_p90_over_margin2", ("firmware",), "b", "OVERSHOOT", "GOOD", "same at the 2.0 margin: interpolated 2.15, floor 1.5",
     INTERP_UP, INTERP_PORT, "Q-PARITY-1")
diff("interp_p50_underheat", BOTH, "b", "GOOD", "UNDERHEAT", "p50 of 180 samples: interpolated -2.5, floor -5.0",
     INTERP_UP, INTERP_PORT, "Q-PARITY-1")
# Q-PARITY-2 hundredth-degree quantisation of the control error
for _c, _s in (("parity", "overshoot_boundary_parity_below_0004"), ("firmware", "overshoot_boundary_firmware_below_0004")):
    diff(_s, (_c,), "b", "GOOD", "OVERSHOOT", "error 0.004 under the margin is stored as int16 hundredths and rounds up to the margin",
         "classifier.py:77 (p90 >= OVERSHOOT_MARGIN_CELSIUS on the unrounded float)", QUANT_PORT, "Q-PARITY-2")
diff("underheat_boundary_cont_above_0004", BOTH, "b", "GOOD", "UNDERHEAT", "error -2.996 is stored as -300 hundredths and reaches -3.0",
     "classifier.py:78 (p50 <= UNDERSHOOT_MARGIN_CELSIUS on the unrounded float)", QUANT_PORT, "Q-PARITY-2")
# Q-PARITY-3 tail warm-up
diff("warmup_high_flow_short_cycle", BOTH, "b", "GOOD", "OVERSHOOT", "the first 25 s (warm-up of a 100 s cycle) are outside thermo-nova's tail, inside the port's ring",
     W, PW, "Q-PARITY-3")
# Q-PARITY-4 count-based tail
diff("sample_gap_in_tail", BOTH, "b", "GOOD", "OVERSHOOT", "after a 100 s gap the port's ring still holds the 130 samples before it; thermo-nova's tail is the last 180 s only",
     W, "SATcycles.ino:104-115, :1009-1030 (ring of the last 180 samples, not the last 180 s)", "Q-PARITY-4")
# Q-PARITY-5 requested setpoint per sample
diff("requested_raised_inside_tail_pwm", BOTH, "b", "GOOD", "UNDERHEAT_PWM", "requested 50 for 80 tail samples, 60 for 100: per sample p90 error 0; the port subtracts the last value 60",
     REQ_UP, REQ_PORT, "Q-PARITY-5")
diff("requested_missing_last_third_pwm", BOTH, "b", "GOOD", "UNCERTAIN", "requested missing for the last 100 s: thermo-nova leaves those samples out; the port reads 0 at flame-off and calls the percentiles missing",
     REQ_UP, "SATcycles.ino:918, :790 (requestedSp <= 0 = missing)", "Q-PARITY-5")
diff("requested_lowered_below_cold_continuous", BOTH, "b", "UNDERHEAT", "UNCERTAIN", "effective setpoint is the p50 of the per-sample requested setpoints (50); the port uses the value at flame-off (20, below cold)",
     "classifier.py:81-86 (tail_metrics.requested_setpoint.p50 against COLD_SETPOINT), tracker.py:279-283", "SATcycles.ino:813-816 (effSp = requestedSp at flame-off)", "Q-PARITY-5")
# Q-PARITY-6 fallback setpoint
diff("device_setpoint_fallback_cold", BOTH, "b", "UNCERTAIN", "UNDERHEAT", "requested missing: thermo-nova falls back to device_state.setpoint at the end (20); the port to the control setpoint at flame-on (30)",
     "classifier.py:81-85 (device_state.setpoint of the flame-off sample)", "SATcycles.ino:814 (controlSp = _cycle_setpointAtStart, set at flame-on :842)", "Q-PARITY-6")
# Q-PARITY-7 / 8 sample count
diff("kind_ch_exactly_80pct", BOTH, "b", "UNCERTAIN", "GOOD", "the flame-on sample is a kind sample upstream (8 CH of 10 = 0.8, not > 0.8: kind UNKNOWN); the port counts 8 of 9",
     "tracker.py:71-72 (the sample of the OFF->ON transition is appended), :314-324", "SATcycles.ino:1009 (the flame-on pass takes no sample: _tail_lastSampleMs = now at :869), :1044-1049", "Q-PARITY-7")
diff("three_second_cycle", BOTH, "b", "GOOD", "INSUFFICIENT", "3 seconds of flame is 3 samples upstream (t=0,1,2, minimum 3), 2 in the port (the flame-on pass takes none)",
     MIN3_UP + "; tracker.py:71-72", "SATcycles.ino:774 (sampleCount < 3), :869, :1009", "Q-PARITY-7")
diff("two_second_cycle", BOTH, "b", "DROPPED", "INSUFFICIENT", "thermo-nova emits no Cycle (not counted in any window); the port records an INSUFFICIENT cycle",
     MIN3_UP, "SATcycles.ino:774 (INSUFFICIENT), :687-710 (_cycleRecord stores it)", "Q-PARITY-8")
# Q-PARITY-9 non-finite flow in the tail ring
diff("flow_missing_pwm_underheat", BOTH, "b", "UNDERHEAT_PWM", "GOOD", "50 of 180 tail flows missing: thermo-nova leaves them out (p90 error -4); the port sorts NaN into the float tail ring and tailP90 becomes NaN",
     "tracker.py:243-250 (None values skipped), :273-277", "SATcycles.ino:1017 (NaN stored in _tail_samples), :612-636 (_tailPercentile sorts it), :1019-1022 (only the control error ring skips it)", "Q-PARITY-9")
# Q-PARITY-11 stand-ins
diff("control_zero_value", BOTH, "b", "OVERSHOOT", "UNCERTAIN", "a control setpoint of 0.0 is a value upstream (error +50); the port reads fFinalSetpoint <= 0 as missing",
     "tracker.py:267-271 (None only), heating_control.py:89, :233 (the control setpoint is a float, MINIMUM_SETPOINT when idle)", "SATcycles.ino:1019-1022 (sp > 0 required)", "Q-PARITY-11")


def observe():
    """Run every scenario on both sides. Returns {(scenario, config): (upstream result, port result)}."""
    out = {}
    for sc in SCENARIOS:
        for cfg in sc.cfgs:
            out[(sc.name, cfg)] = (run_upstream(sc, cfg), run_port(sc, cfg))
    return out


_OBS = {}


def observed():
    if not _OBS:
        _OBS.update(observe())
    return _OBS


def skip_reason():
    try:
        load_upstream()
    except UpstreamUnavailable as e:
        return "thermo-nova upstream not runnable: %s" % e
    try:
        if port_exe() is None:
            return "no host C++ compiler (g++, clang++ or MSVC)"
    except AssertionError as e:
        return "port harness did not build: %s" % e
    return None


class Base(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        why = skip_reason()
        if why:
            raise unittest.SkipTest(why)


class TestUpstreamIdentity(Base):
    def test_loaded_files_are_the_pinned_ones(self):
        """The copies that were imported are byte-identical to the pinned originals (sha256 over LF-normalised content)."""
        up = load_upstream()
        self.assertEqual(sorted(up.hashes), sorted(PINNED_SHA256))
        for rel, h in up.hashes.items():
            self.assertEqual(h, PINNED_SHA256[rel], rel)
            self.assertEqual(lf_sha((TN / rel).read_bytes()), h, "%s: the original in other-projects changed" % rel)
        pin = (TN.parent.parent / "UPSTREAM_COMMIT").read_text(encoding="utf-8")
        self.assertIn(UPSTREAM_COMMIT, pin)

    def test_unpatched_upstream_constants(self):
        up = load_upstream()
        self.assertEqual(up.orig, (3.0, 22))      # cycles/const.py:10, const.py:34 (what the parity configuration runs)
        self.assertEqual(up.tracker.IN_BAND_MARGIN_CELSIUS, 1.0)

    def test_upstream_percentile_is_interpolated(self):
        up = load_upstream()
        self.assertAlmostEqual(up.helpers.percentile_interpolated([0.0] * 162 + [8.0] * 18, 0.9), 0.8)
        self.assertAlmostEqual(up.helpers.percentile_interpolated([-5.0] * 90 + [0.0] * 90, 0.5), -2.5)


class TestParity(Base):
    def test_every_difference_is_catalogued_and_nothing_else_differs(self):
        obs = observed()
        unexpected, stale, wrong = [], [], []
        for key, (u, p) in sorted(obs.items()):
            same = u["verdict"] == p["verdict"]
            entry = DIFFERENCES.get(key)
            if same and entry:
                stale.append("%s/%s: listed as a difference but both give %s" % (key + (u["verdict"],)))
            elif not same and not entry:
                unexpected.append("%s/%s: upstream %s, port %s  [%s | %s]" % (key + (u["verdict"], p["verdict"], u["detail"], p["detail"])))
            elif entry and (entry["up"], entry["port"]) != (u["verdict"], p["verdict"]):
                wrong.append("%s/%s: catalogue says %s/%s, observed %s/%s" % (key + (entry["up"], entry["port"], u["verdict"], p["verdict"])))
        missing = sorted(k for k in DIFFERENCES if k not in obs)
        self.assertEqual((unexpected, stale, wrong, missing), ([], [], [], []))

    def test_every_difference_has_a_class_citations_and_a_reference(self):
        for key, e in DIFFERENCES.items():
            with self.subTest(key=key):
                self.assertIn(e["klass"], ("a", "b"))
                self.assertTrue(e["up_cite"] and e["port_cite"] and e["why"])
                if e["klass"] == "a":
                    self.assertRegex(e["ref"], r"^entr(y|ies) \d+")
                else:
                    self.assertRegex(e["ref"], r"^Q-PARITY-\d+")

    def test_verdicts_that_must_stay_equal(self):
        """Rows that match today and are the core of the rule order: they must not start to differ."""
        obs = observed()
        for name in ("normal_burn", "short_burn_continuous", "short_burn_pwm_on", "overshoot_burn", "underheat_burn",
                     "underheat_cold_setpoint", "cs_change_before_tail", "cs_change_inside_tail", "cs_lowered_inside_tail",
                     "mode_cont_to_pwm_good", "mode_cont_to_pwm_underheat", "mode_pwm_to_cont_overshoot", "control_missing_all",
                     "warmup_dhw_out", "warmup_dhw_in", "dhw_exactly_at_tail_start", "dhw_just_before_tail_start", "dhw_only_cycle",
                     "kind_unknown", "pwm_idle_at_end"):
            for cfg in ("parity", "firmware"):
                u, p = obs[(name, cfg)]
                with self.subTest(name=name, cfg=cfg):
                    self.assertEqual(u["verdict"], p["verdict"], "%s | %s" % (u["detail"], p["detail"]))

    def test_mid_burn_changes_reach_the_verdict(self):
        """AC1: the CS change and the mode change are not neutral: the verdict depends on them, on both sides alike."""
        obs = observed()
        for name, want in (("cs_change_before_tail", "GOOD"), ("cs_change_inside_tail", "OVERSHOOT"), ("cs_lowered_inside_tail", "UNDERHEAT"),
                           ("mode_cont_to_pwm_good", "GOOD"), ("mode_cont_to_pwm_underheat", "UNDERHEAT_PWM"),
                           ("mode_pwm_to_cont_overshoot", "OVERSHOOT")):
            u, p = obs[(name, "parity")]
            with self.subTest(name=name):
                self.assertEqual((u["verdict"], p["verdict"]), (want, want))

    def test_recorded_configuration_deviations_change_the_verdict(self):
        """The decided deviations (margin 2.0, cold 28.2) are real: the same trace flips between the two configurations,
        identically on both sides."""
        obs = observed()
        u3, p3 = obs[("underheat_cold_setpoint", "parity")]
        u2, p2 = obs[("underheat_cold_setpoint", "firmware")]
        self.assertEqual((u3["verdict"], p3["verdict"]), ("UNDERHEAT", "UNDERHEAT"))      # 25 >= COLD 22
        self.assertEqual((u2["verdict"], p2["verdict"]), ("UNCERTAIN", "UNCERTAIN"))      # 25 < 28.2 (entries 7, 11, 200)
        for cfg, flow_off in (("parity", 3.0), ("firmware", 2.0)):
            u, p = obs[("overshoot_boundary_%s_exact" % cfg, cfg)]
            self.assertEqual((u["verdict"], p["verdict"]), ("OVERSHOOT", "OVERSHOOT"), "error exactly at the margin counts (>=, entry 37)")


# AC2: the input deviations, each verified by named scenarios (outcome per side) with both citations.
AC2 = [
    ("per-sample control setpoint (entry 36, TASK-1405)", "recorded and implemented: no difference",
     ["cs_change_before_tail", "cs_change_inside_tail", "cs_lowered_inside_tail", "mode_and_cs_change"], None,
     "tracker.py:267-271", "SATcycles.ino:1019-1022"),
    ("per-sample requested setpoint", "difference, Q-PARITY-5",
     ["requested_raised_inside_tail_pwm", "requested_missing_last_third_pwm", "requested_lowered_below_cold_continuous"], "Q-PARITY-5",
     "tracker.py:273-277", "SATcycles.ino:918, :790"),
    ("warm-up exclusion", "difference, Q-PARITY-3 (the DHW-in-tail test already has it: no difference)",
     ["warmup_high_flow_short_cycle", "warmup_dhw_out", "warmup_dhw_in"], "Q-PARITY-3", "tracker.py:113-116", "SATcycles.ino:925-927 (DHW only), :1009-1024"),
    ("percentile interpolation", "difference, Q-PARITY-1",
     ["interp_p90_over_margin3", "interp_p90_over_margin2", "interp_p50_underheat"], "Q-PARITY-1", "helpers.py:187-203", "SATcycles.ino:633, :662"),
    ("missing-data treatment", "differences, Q-PARITY-5 / 6 / 9 / 11; control missing for the whole burn: no difference",
     ["control_missing_all", "control_missing_part_of_tail", "flow_missing_continuous", "flow_missing_pwm_underheat", "control_zero_value",
      "device_setpoint_fallback_cold"], "Q-PARITY-9", "tracker.py:243-250, classifier.py:42-43, :71-72", "SATcycles.ino:790, :1017-1022"),
    ("boundary margin", "exact margin agrees (>=, entry 37); 0.004 either side differs, Q-PARITY-2",
     ["overshoot_boundary_parity_exact", "overshoot_boundary_firmware_exact", "overshoot_boundary_parity_below_0004",
      "overshoot_boundary_firmware_below_0004", "underheat_boundary_cont_exact", "underheat_boundary_cont_above_0004",
      "underheat_boundary_pwm_exact", "underheat_boundary_pwm_above_0004"], "Q-PARITY-2", "classifier.py:77-78, :45", "SATcycles.ino:809, :817-818, :798"),
]


class TestAc2Inventory(Base):
    def test_each_ac2_item_is_covered_by_observed_scenarios_with_both_citations(self):
        obs = observed()
        names = {k[0] for k in obs}
        for item, status, scenarios, ref, up_cite, port_cite in AC2:
            with self.subTest(item=item):
                self.assertTrue(up_cite and port_cite and status)
                self.assertTrue(set(scenarios) <= names, set(scenarios) - names)
                if ref:
                    self.assertTrue(any(e["ref"] == ref for e in DIFFERENCES.values()), "%s has no catalogued difference" % ref)

    def test_label_deviation_underheat_pwm(self):
        """AC2 label: classifier.py:54 returns UNDERHEAT for the PWM branch too; the port returns its own UNDERHEAT_PWM class.
        Same decision, other label (Q-PARITY-10, label only: the verdict columns normalise it)."""
        obs = observed()
        u, p = obs[("mode_cont_to_pwm_underheat", "parity")]
        self.assertEqual(u["raw"], "UNDERHEAT")
        self.assertEqual(p["verdict"], "UNDERHEAT_PWM")
        u2, p2 = obs[("underheat_burn", "parity")]
        self.assertEqual((u2["raw"], p2["verdict"]), ("UNDERHEAT", "UNDERHEAT"))


class TestRam(Base):
    def test_tail_sample_buffer_cost_is_counted_from_the_real_declarations(self):
        """AC3 (static part): bytes of the port's tail buffers, compiled from the real declarations (float tail ring 4 B,
        int16 control-error ring 2 B, 180 slots); the on-device measurement is the bench part."""
        exe = port_exe()
        out = subprocess.run([str(exe), "sizes"], capture_output=True, text=True).stdout
        v = dict(tok.split("=") for tok in out.split())
        boards = (REPO / "src" / "libraries" / "Platform" / "src" / "boards.h").read_text(encoding="utf-8")
        sizes = {int(x) for x in re.findall(r"#define\s+SAT_TAIL_SAMPLE_SIZE\s+(\d+)", boards)}
        self.assertEqual(sizes, {180}, "boards.h changed: the host figure below is for 180 slots")
        self.assertEqual(int(v["SAT_TAIL_SAMPLE_SIZE"]), 180)
        self.assertEqual(int(v["tail_samples"]), 180 * 4)       # float
        self.assertEqual(int(v["tail_ctrlErr"]), 180 * 2)       # int16_t hundredths
        self.assertEqual(int(v["flow_samples"]), 256 * 4)       # per-cycle flow ring (SAT_FLOW_SAMPLE_SIZE = 256), not a tail buffer
        tail_total = int(v["tail_samples"]) + int(v["tail_ctrlErr"])
        self.assertEqual(tail_total, 1080)
        # Estimates for closing deviations (arithmetic from the same element sizes, not measurements):
        #   per-sample requested setpoint as int16 hundredths: + 180 x 2 B = 360 B (tail would be 1440 B);
        #   per-sample timestamp so the tail is time based (gaps): + 180 x 4 B = 720 B (uint32) or 360 B (uint16 seconds).
        self.assertEqual(180 * 2, 360)


class TestParityGate(Base):
    @unittest.expectedFailure
    def test_no_unrecorded_difference_remains(self):
        """TASK-1331 may resume only when every difference is class (a): this expected failure turns into an unexpected
        success then, and the marker comes off together with an updated catalogue."""
        open_ = sorted(k for k, e in DIFFERENCES.items() if e["klass"] == "b")
        self.assertEqual(open_, [])


def report():
    """Print the scenario table (python tests/test_sat_classifier_parity.py --table)."""
    why = skip_reason()
    if why:
        print("SKIP:", why)
        return
    up = load_upstream()
    print("upstream %s; sha256 (LF-normalised):" % UPSTREAM_COMMIT)
    for rel, h in sorted(up.hashes.items()):
        print("  %s  %s" % (h, rel))
    print("port harness: %s" % _PORT.get("variant"))
    obs = observed()
    for sc in SCENARIOS:
        row = []
        for cfg in sc.cfgs:
            u, p = obs[(sc.name, cfg)]
            row.append("%s %s/%s%s" % (cfg[:3], u["verdict"], p["verdict"], "" if u["verdict"] == p["verdict"] else " <DIFF>"))
        print("%-46s %s" % (sc.name, " | ".join(row)))
        for cfg in sc.cfgs:
            u, p = obs[(sc.name, cfg)]
            if u["verdict"] != p["verdict"]:
                print("     [%s] up: %s ; port: %s" % (cfg, u["detail"], p["detail"]))


if __name__ == "__main__":
    if "--table" in sys.argv:
        report()
    else:
        unittest.main(verbosity=2)
