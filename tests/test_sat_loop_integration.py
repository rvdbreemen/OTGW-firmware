#!/usr/bin/env python3
"""TASK-1412.18.1 (1412.18 AC2): ONE host program runs the real SAT control loop, the real PIC command queue and the real boiler-frame feed
hooks together, on a stub clock, with sensor loss and recovery.

REAL CODE (compiled from src/, nothing re-implemented):
  - SATcontrol.ino, SATcycles.ino, SATpid.ino WHOLE (all of satControlLoop() -> satControlLoopRun() with its 148 callees: boot/restart gate,
    OT-link gate, PID, heating curve, PWM schedule and ON/OFF phases, flood and MI=500, command senders and cache, ownership rule), with
    Arduino-style auto prototypes generated for them (tests/sat_loop_build.py);
  - the component type headers and the OTGWState / OTGWSettings aggregates (SATtypes.h, OTBustypes.h, SATboilerFeed.h, SATflowAvg.h, ...);
  - safeTimers.h (the firmware's own DUE()/timer implementation, driven by the stub millis());
  - the PIC command queue of OTGW-Core.ino (addCommandToQueue, handleCommandQueue, checkCommandResponse, the PM chain, the ownership
    refusal) and satBlocksExternalCommand() of OTGW-firmware.h;
  - the processOT() text for a boiler frame: the B-frame head (iBoilerLastSeenMs stamp), the MsgID 3 -> satNoteSlaveConfig() branch, the
    PM-chain acknowledgement line, and the MsgID 25/28 feed hook with the flow average (satFeedNote, satFlowAvgNote).
STUBBED (tests/sat_loop_stubs.h, tests/sat_loop_stubs/, QUEUE_STUBS in tests/sat_loop_build.py; each says what it does not model):
  Arduino core/millis (a stub clock), FreeRTOS critical sections, JsonEmit/REST, MQTT publish (counted only), SATble/SATweather/SATpressure
  (not compiled; weather = a fixed outside temperature refreshed every pass), LittleFS (every open fails: the PID/state load finds nothing,
  so every run boots "not valid" and starts with the restart hand-back), settings persistence, the OT support map (two flags), the
  PR=E board return sensor (absent), the diagnostic recorder, OT-Direct (the PIC transport is the one exercised), the PIC UART (sendPICSerial
  records the line; the PIC echoes it 25 ms later).
BOILER MODEL (tests/sat_loop_driver.inc): thermostat polls of MsgID 25/28/0/17/3/24 as B frames through the real hooks, the PIC answering SAT's
  PM= commands with the matching frame 300 ms later, and a first-order water model (burner with a minimum output, hysteresis cut-off, minimum
  off time). The constants are a plausible combi boiler, not a measurement; the dynamics are a model, so absolute times are the model's.
LIMITS: SATble/SATweather/SATpressure/SATmqttPublish are not compiled; the OT-Direct ring (ADR-242) and OT-Direct transport are not exercised
  (scenario 4 covers the PIC queue only); the OT value store of processOT() (print_*) is replaced by direct writes to OTcurrentSystemState;
  the one-frame delay of delayedOTdata is not run; the alternative-slot detection (satPm25NoteFrame) is not fed, so SAT's own PM=25 poll
  never fires (the start-up flood's PM=25 does); no DHW draw, no fault flag, no real wall-clock time. It proves the real loop, queue and
  feed together under the model; it does not prove the physical boiler (that stays a bench item).

Mutants: each scenario has a mutant, a text edit of an in-memory COPY of the real source (src/ is never written); the scenario assertions
must fail on it and pass on the real source.

Run: python tests/test_sat_loop_integration.py
"""
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import sat_loop_build as B  # noqa: E402

_CACHE = {}
_TMP = tempfile.TemporaryDirectory()


def build(name="base", overrides=None):
    """Compile one variant (cached). Returns the exe path; a compile error raises RuntimeError (never a silent skip)."""
    if name in _CACHE:
        return _CACHE[name]
    d = Path(_TMP.name) / name
    d.mkdir()
    (d / "t.cpp").write_text(B.assemble(overrides=overrides), encoding="utf-8")
    rc, out = B.cl_compile(d / "t.cpp", d / "t.exe")
    if rc is None:
        raise unittest.SkipTest("no host C++ compiler (MSVC) found")
    if rc != 0:
        raise RuntimeError("host compile failed for variant %s:\n%s" % (name, out[-3000:]))
    _CACHE[name] = d / "t.exe"
    return _CACHE[name]


def mutant(file, old, new):
    """overrides dict for assemble(): the real source with exactly one occurrence of `old` replaced."""
    src = B.rd(B.FW / file)
    assert src.count(old) == 1, "mutant target must occur exactly once in %s, found %d: %r" % (file, src.count(old), old)
    return {file: src.replace(old, new)}


class Run:
    def __init__(self, exe, script):
        out = subprocess.run([str(exe)], input="\n".join(script) + "\n", capture_output=True, text=True, check=True).stdout
        self.ev = []
        for l in out.splitlines():
            m = re.match(r"EV (\d+) (\w+) ?(.*)", l)
            if m:
                self.ev.append((int(m.group(1)), m.group(2), m.group(3)))

    def tx(self, a=0, b=10 ** 12, prefix=""):
        return [(t, x) for t, k, x in self.ev if k == "TX" and a <= t < b and x.startswith(prefix)]

    def kind(self, kind, a=0, b=10 ** 12):
        return [(t, x) for t, k, x in self.ev if k == kind and a <= t < b]

    def snap(self, tag):
        for t, k, x in self.ev:
            if k == "STATE" and x.startswith(tag + " "):
                return dict(p.split("=") for p in x.split()[1:]), t
        raise AssertionError("no snapshot %s" % tag)

    def mark(self, tag):
        for t, k, x in self.ev:
            if k == "MARK" and x == tag:
                return t
        raise AssertionError("no mark %s" % tag)

    def dump(self, a=0, b=10 ** 12, limit=60):
        return "\n".join("  %d %s %s" % e for e in self.ev if a <= e[0] < b and e[1] != "PIC_ECHO")[:limit * 60]


# ---- scenario 1: steady low load -> PWM, ON and OFF phases and their CS on the wire -------------------------------------------------
S1 = ["run 2400000", "snap end"]


def check_s1(tc, r):
    modes = r.kind("MODE")
    tc.assertIn("2", [x for _, x in modes], "PWM (mode 2) was never reached:\n" + r.dump(limit=25))
    phases = [(t, x) for t, x in r.kind("PWMPHASE")]
    ons = [t for t, x in phases if x == "ON"]
    offs = [t for t, x in phases if x == "OFF"]
    tc.assertTrue(ons, "no PWM ON phase\n" + r.dump(limit=25))
    first_on = ons[0]
    off_after = [t for t in offs if t > first_on]
    tc.assertTrue(off_after, "the PWM ON phase never ended (no ON->OFF transition); phases=%r" % phases)
    t_off = off_after[0]
    # the CS of the ON phase reached the wire through the real queue: heating setpoints above the 10 C release value, CH=1 present
    on_cs = [float(x[3:]) for t, x in r.tx(first_on, t_off, "CS=")]
    tc.assertTrue(on_cs and max(on_cs) > 20.0, "no ON-phase CS on the wire: %r" % on_cs)
    tc.assertTrue(r.tx(first_on, t_off + 1, "CH=1"), "no CH=1 during the ON phase")
    # the OFF phase: CS=10.0 and CH=0 on the wire
    tc.assertTrue(r.tx(t_off - 1000, t_off + 35000, "CS=10.0"), "OFF phase CS=10.0 never on the wire after %d" % t_off)
    tc.assertTrue(r.tx(t_off - 1000, t_off + 65000, "CH=0"), "OFF phase CH=0 never on the wire after %d" % t_off)
    # and a later ON start (the cycle repeats)
    tc.assertTrue([t for t in ons if t > t_off], "no second ON phase after the first OFF phase")


# ---- scenario 2: MsgID 25 lost -------------------------------------------------------------------------------------------------------
def s2_script(gap_s):
    return ["run 700000", "snap before", "mute25 1", "run %d" % (700000 + gap_s * 1000 - 1000), "snap during", "mute25 0", "run %d" % (700000 + gap_s * 1000 + 120000), "snap after"]


def cont_cs(r):
    """The continuous setpoint SAT sends before PWM is entered (last CS= before the mode change)."""
    t_pwm = r.kind("MODE")[-1][0] if len(r.kind("MODE")) > 1 else 10 ** 9
    return [float(x[3:]) for t, x in r.tx(60000, t_pwm, "CS=")][-1]


def check_s2_loss(tc, r):
    b, _ = r.snap("before")
    tc.assertEqual((b["mode"], b["idle"], b["pwmOn"]), ("2", "0", "1"), "not in a running PWM ON phase before the loss: %r" % b)
    d, _ = r.snap("during")
    tc.assertEqual(d["idle"], "1", "PWM did not idle on a flow temperature older than 60 s: %r" % d)
    tc.assertEqual(d["pwmOn"], "0")
    # TASK-1412.18.1 finding: with the flow stale the 3 s suppression refresh must not send the minimum CS while CH stays on;
    # thermo-nova makes no override without a flow temperature (heating_control.py:435-437) and keeps the setpoint.
    low = [(t, x) for t, x in r.tx(700000, 790000, "CS=") if float(x[3:]) <= 10.05]
    tc.assertEqual(low, [], "minimum CS sent during the flow loss (3 s refresh without a flow temperature)")
    cs = cont_cs(r)
    in_loss = [float(x[3:]) for t, x in r.tx(762000, 790000, "CS=")]
    tc.assertTrue(in_loss and abs(in_loss[-1] - cs) < 0.15, "during the loss the CS on the wire is %r, not the continuous setpoint %.1f" % (in_loss, cs))
    a, _ = r.snap("after")
    tc.assertEqual(a["idle"], "0", "PWM did not recover after the flow temperature came back: %r" % a)
    tc.assertEqual(a["mode"], "2")


def check_s2_short(tc, r):
    d, _ = r.snap("during")
    tc.assertEqual((d["idle"], d["pwmOn"], d["mode"]), ("0", "1", "2"), "a 50 s MsgID 25 gap idled PWM (validity window is 60 s): %r" % d)
    cs = [float(x[3:]) for t, x in r.tx(700000, 752000, "CS=")]
    tc.assertTrue(cs and all(v > 30.0 for v in cs), "CS on the wire during a 50 s gap left the ON-phase values: %r" % cs)


# ---- scenario 3: return temperature lost -> flood and MI=500 --------------------------------------------------------------------------
S3A = ["ret28unknown 1", "run 700000", "snap end"]
S3B = ["run 100000", "mark ret_ok", "cool 28", "run 160000", "mark retlost", "muteret 1", "run 720000", "snap before", "cool 28", "run 800000", "snap end"]


def check_s3a(tc, r):
    fl = [t for t, x in r.kind("FLOOD") if x == "1"]
    tc.assertTrue(fl, "no start-up flood although MsgID 28 is Unknown-DataID:\n" + r.dump(limit=30))
    t0 = fl[0]
    pm = r.tx(t0, t0 + 20000, "PM=25")
    tc.assertGreaterEqual(len(pm), 8, "flood PM=25 count in 20 s: %d" % len(pm))
    tc.assertTrue(len(r.tx(t0, t0 + 20000, "CS=")) >= 8, "flood CS= count too low")
    tc.assertTrue(r.tx(0, 10 ** 9, "MI=500"), "MI=500 never sent for a boiler without a return temperature")


def check_s3b(tc, r):
    ok = r.mark("ret_ok")
    pre = [t for t, x in r.kind("FLOOD", ok, ok + 70000) if x == "1"]
    tc.assertFalse(pre, "control: a flood ran while the return temperature was valid")
    lost = r.mark("retlost")
    # the second `cool` is after the return was lost for > 5 minutes
    tc.assertTrue(r.snap("before")[1] - lost > 300000)
    fl = [t for t, x in r.kind("FLOOD", lost + 300000) if x == "1"]
    tc.assertTrue(fl, "no flood on the ignition after the return temperature was lost for > 5 minutes:\n" + r.dump(lost + 300000, lost + 600000, 40))
    tc.assertTrue(r.tx(fl[0], fl[0] + 20000, "PM=25"), "flood without PM=25")
    tc.assertTrue(r.tx(0, 10 ** 9, "MI=500"), "MI=500 not sent after 5 minutes without a return temperature")


# ---- scenario 4: queue full: a refused CS= is not cached as delivered and is retried --------------------------------------------------
S4 = (["run 100000", "snap old", "outside 0", "echomute 1"] +
      [x for k in range(1, 31) for x in ("fill 20", "run %d" % (100000 + 2000 * k))] +     # the queue is topped up every 2 s for 60 s
      ["snap full", "echomute 0", "run 330000", "snap drained"])


def check_s4(tc, r):
    old, _ = r.snap("old")
    old_cs = float(old["lastCS"])
    dropped = [float(x.split()[0][3:]) for t, x in r.kind("ENQ", 100000, 160500) if x.startswith("CS=") and x.endswith("Dropped")]
    changed = [v for v in dropped if abs(v - old_cs) > 0.05]
    tc.assertTrue(changed, "no refused CS= with a NEW value while the queue was full (refused values %r): %s" % (sorted(set(dropped)), r.dump(100000, 160500, 30)))
    full, _ = r.snap("full")
    tc.assertAlmostEqual(float(full["lastCS"]), old_cs, 1, "a refused CS= was cached as delivered: last-sent CS %s (was %.1f)" % (full["lastCS"], old_cs))
    new = [float(x[3:]) for t, x in r.tx(160500, 330000, "CS=") if abs(float(x[3:]) - old_cs) > 0.05]
    tc.assertTrue(new, "the refused CS= was not retried after the queue drained: " + r.dump(160500, 330000, 40))
    tc.assertAlmostEqual(new[0], changed[0], 1, "the retry carried %.1f, the refused value was %.1f" % (new[0], changed[0]))


def check_ownership(tc, r):
    u = r.kind("USER")
    tc.assertTrue(u and u[0][1] == "CS=40.0 Dropped", "a user CS= was accepted while SAT controls: %r" % u)
    s, _ = r.snap("own")
    tc.assertEqual(s["extRefused"], "1")


# ---- scenario 5: OT link lost -> one hand-back, then nothing ------------------------------------------------------------------------------
S5 = ["run 200000", "mark linkdown", "link 0", "run 500000", "snap lost", "mark linkup", "link 1", "run 600000", "snap back"]


def check_s5(tc, r):
    a, b = r.mark("linkdown"), r.mark("linkup")
    sent = [x for t, x in r.tx(a, b)]
    tc.assertEqual(sent.count("CS=0"), 1, "hand-back CS=0 count %d in the loss window: %r" % (sent.count("CS=0"), sent[:20]))
    tc.assertEqual(sent.count("CH=1"), 1, "hand-back CH=1 count %d: %r" % (sent.count("CH=1"), sent[:20]))
    tc.assertEqual(sent.count("MM=T"), 1, "hand-back MM=T count %d: %r" % (sent.count("MM=T"), sent[:20]))
    # after the hand-back (30 s of silence + pass) nothing of SAT reaches the wire until a boiler reply returns
    t_hb = r.tx(a, b, "CS=0")[0][0]
    tc.assertEqual([x for t, x in r.tx(t_hb + 2000, b)], [], "SAT sent more after the hand-back: %r" % r.tx(t_hb + 2000, b)[:10])
    s, _ = r.snap("lost")
    tc.assertEqual(s["otLost"], "1")
    tc.assertTrue(r.tx(b, 10 ** 9, "CS="), "SAT did not resume after the boiler replied again")


class SatLoopIntegration(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe = build("base")

    def test_s1_steady_low_load_pwm(self):
        check_s1(self, Run(self.exe, S1))

    def test_s1_pwm_not_reached_when_boiler_denies_low_off(self):
        # control: the same boiler with MsgID 3 HB bit 4 = 1 (master low-off not allowed) never gets PWM
        r = Run(self.exe, ["lowoff 1"] + S1)
        self.assertNotIn("2", [x for _, x in r.kind("MODE")])

    def test_s1_jittered_loop_period(self):
        # loop period 10 ms + a seeded 0-40 ms jitter (average ~30 ms); the real DUE() timers must still produce the same behaviour
        check_s1(self, Run(self.exe, ["seed 7", "jitter 40"] + S1))

    def test_s2_flow_lost_90s_idles_pwm_and_recovers(self):
        check_s2_loss(self, Run(self.exe, s2_script(90)))

    def test_s2_flow_gap_50s_does_not_idle(self):
        check_s2_short(self, Run(self.exe, s2_script(50)))

    def test_s3a_unknown_dataid_on_return_floods_and_sends_mi(self):
        check_s3a(self, Run(self.exe, S3A))

    def test_s3b_return_lost_over_5_minutes_floods_on_next_ignition(self):
        check_s3b(self, Run(self.exe, S3B))

    def test_s4_queue_full_refused_cs_not_cached_and_retried(self):
        check_s4(self, Run(self.exe, S4))

    def test_s4_user_cs_refused_while_sat_controls(self):
        check_ownership(self, Run(self.exe, ["run 100000", "user CS=40.0", "snap own"]))

    def test_s5_ot_link_lost_one_handback(self):
        check_s5(self, Run(self.exe, S5))


class Mutants(unittest.TestCase):
    """Each mutant is the real source with one edit; the scenario assertion that passes on the real source must fail on it."""

    def red(self, name, overrides, script, check):
        exe = build(name, overrides)
        r = Run(exe, script)
        with self.assertRaises(AssertionError) as cm:
            check(self, r)
        print("\nMUTANT %s RED: %s" % (name, " ".join(str(cm.exception).split())[:420]))

    def test_m1_pwm_on_phase_never_ends(self):
        self.red("m1", mutant("SATcontrol.ino", "    if (elapsedMs >= onTimeMs) {\n      // ON time expired", "    if (false && elapsedMs >= onTimeMs) {\n      // ON time expired"), S1, check_s1)

    def test_m2_pwm_ignores_stale_flow(self):
        self.red("m2", mutant("SATcontrol.ino", "static const uint32_t SAT_FLOW_VALID_MS   = 60000UL;", "static const uint32_t SAT_FLOW_VALID_MS   = 4000000000UL;"),
                 s2_script(90), check_s2_loss)

    def test_m3_flood_disabled(self):
        self.red("m3", mutant("SATcontrol.ino", "  _flood_active   = true;\n  _flood_startMs  = millis();", "  _flood_active   = false;\n  _flood_startMs  = millis();"), S3A, check_s3a)

    def test_m4_refused_cs_cached_on_enqueue(self):
        self.red("m4", mutant("SATcontrol.ino", "  const CmdQueueResult r = addCommandToQueue(cmd, strlen(cmd), false, 0, true);\n  return r != CmdQueueResult::Dropped;",
                              "  const CmdQueueResult r = addCommandToQueue(cmd, strlen(cmd), false, 0, true);\n  (void)r; return true;"), S4, check_s4)

    def test_m5_handback_every_pass(self):
        self.red("m5", mutant("SATcontrol.ino", "    state.sat.bOtLinkLost = true;          // after satDisable(), which clears it", "    state.sat.bOtLinkLost = false;         // MUTANT"),
                 S5, check_s5)


if __name__ == "__main__":
    unittest.main(verbosity=2)
