#!/usr/bin/env python3
"""TASK-1882: SAT diagnostic events, no control action.

no ignition:     CH asked (PWM ON phase or CS above the cold cutoff) for 180 s, flow below CS, no flame.
heating failure: SAT at the maximum (setpoint clamped or PWM max duty) for 15 min with a valid room temperature,
                 room rise < 0.5 C over the window (Robert A, DM 1570699094622470167; "high heat" definition by the
                 coordinator under Robert's delegation, 2026-11-15).

REAL code: satDiagEventsStep() and its constants are cut from SATcontrol.ino between the markers
"// --- satDiagEvents begin/end" and driven tick by tick (30 s) with fake time; DebugTf and satNarratef_P are
recorded. A source check pins that the step sends nothing (no CS/MM/CH or queue call) and that the tick calls it
after its CS is decided. On code without the step the cut fails: the event does not exist there.

    python tests/test_sat_diag_events.py
    OTGW_SATCONTROL_INO=<older SATcontrol.ino> python tests/test_sat_diag_events.py   # old code: fails
"""
import os
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent
sys.path.insert(0, str(TESTS))
from test_sat_published_curve import compile_cpp  # noqa: E402

INO = Path(os.environ.get("OTGW_SATCONTROL_INO", TESTS.parent / "src" / "OTGW-firmware" / "SATcontrol.ino"))


def source():
    return INO.read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")


HDR = Path(os.environ.get("OTGW_SATTYPES_H", INO.with_name("SATtypes.h")))


def cut(src):
    a = src.find("// --- satDiagEvents begin")
    b = src.find("// --- satDiagEvents end")
    if a < 0 or b < 0:
        raise AssertionError("satDiagEventsStep() not found in SATcontrol.ino (no diagnostic events on this code)")
    block = src[a:b]
    if "struct SatDiagIn {" not in block:   # since the TASK-1882 review the input struct lives in SATtypes.h
        h = HDR.read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n") if HDR.exists() else ""
        i = h.find("struct SatDiagIn {")
        if i < 0:
            raise AssertionError("struct SatDiagIn not found in SATtypes.h nor in the satDiagEvents block")
        block = h[i:h.index("};", i) + 2] + "\n" + block
    return block


HARNESS = r"""
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#define PSTR(s) (s)
typedef const char* PGM_P;
struct { struct { bool bNoIgnition = false, bHeatingFailure = false; float fHfRoomStart = 0, fHfRoomEnd = 0;
                  uint32_t iHfEvents = 0, iNoIgnitionEvents = 0; } sat; } state;
static std::string g_log;
static void DebugTf(const char* f, ...) { char b[256]; va_list a; va_start(a, f); vsnprintf(b, sizeof b, f, a); va_end(a); g_log += b; }
#define F(s) (s)
static void DebugTln(const char* s) { g_log += s; g_log += "\n"; }   // TASK-1882 review: satDiagEventsReset() logs the sat_off clear
static void satNarratef_P(const char* f, ...) { char b[256]; va_list a; va_start(a, f); vsnprintf(b, sizeof b, f, a); va_end(a); g_log += "N:"; g_log += b; g_log += "\n"; }
@@CUT@@
static SatDiagIn in0(uint32_t now) {
  SatDiagIn d; d.now = now; d.intervalMs = 30000; d.chAsked = false; d.bandOff = false; d.flame = false; d.flowAboveCs = false;
  d.atMax = false; d.roomValid = true; d.room = 19.0f; d.cs = 45.0f; d.maxSetpoint = 62.0f; d.duty = 0.0f; return d;
}
int main(int argc, char** argv) {
  const std::string c = argv[1];
  uint32_t t = 100000; int firstTick = -1;
  if (c == "noign" || c == "flame" || c == "flow" || c == "gap") {
    for (int i = 0; i < 20; i++, t += 30000) {
      if (c == "gap" && i == 3) t += 60000;                  // ticks 3.. come after a 90 s hole (an early-return tick)
      SatDiagIn d = in0(t); d.chAsked = true;
      d.flame = (c == "flame" && i >= 4);
      d.flowAboveCs = (c == "flow");
      satDiagEventsStep(d);
      if (state.sat.bNoIgnition && firstTick < 0) firstTick = i;
    }
    printf("first=%d active=%d events=%u\n", firstTick, (int)state.sat.bNoIgnition, (unsigned)state.sat.iNoIgnitionEvents);
  } else if (c == "pwm" || c == "pwmflame" || c == "satoff") {
    // TASK-1882 review: PWM ON 4 ticks (120 s) / OFF 4 ticks for 2 h, never a flame (a locked-out boiler); "pwmflame":
    // a flame in the ON phase of cycle 10; "satoff": SAT switched off once the event is up (satDiagEventsReset)
    int flips = 0; bool prev = false;
    for (int i = 0; i < 240; i++, t += 30000) {
      SatDiagIn d = in0(t); d.chAsked = (i % 8) < 4;
      d.flame = (c == "pwmflame" && i == 81);
      if (c == "satoff" && state.sat.bNoIgnition) { satDiagEventsReset(); printf("reset_at=%d\n", i); c.size(); break; }
      satDiagEventsStep(d);
      if (state.sat.bNoIgnition && firstTick < 0) firstTick = i;
      if (state.sat.bNoIgnition != prev) { flips++; prev = state.sat.bNoIgnition; }
    }
    printf("first=%d active=%d events=%u flips=%d\n", firstTick, (int)state.sat.bNoIgnition,
           (unsigned)state.sat.iNoIgnitionEvents, flips);
  } else {                                                    // heating failure: "hf_<rise per window * 100>" or "hf_below"/"hf_invalid"
    float risePerWindow = 0.2f; bool atMax = true, invalid = false;
    if (c == "hf_60") risePerWindow = 0.6f;
    if (c == "hf_below") atMax = false;
    if (c == "hf_invalid") invalid = true;
    // Q-428a (entry 501): the window counts only while SAT has CH enabled. "hf_band": CH asked for 20 ticks, a 10-tick tolerance-band
    // pause (CH not asked) in which the room cools by 0.6 C, then CH asked again; the asked time itself rises 0.6 C per 15 min.
    // "hf_hold": a real failure is flagged, then a band pause: the flag stays.
    const bool band = (c == "hf_band"), hold = (c == "hf_hold");
    if (band) risePerWindow = 0.6f;
    const int N = band ? 90 : (hold ? 50 : 70);
    float room = 19.0f;
    for (int i = 0; i < N; i++, t += 30000) {                // 35 min
      SatDiagIn d = in0(t); d.atMax = atMax; d.roomValid = !(invalid && i == 20);
      d.chAsked = true;
      if (band) {
        const bool pause = i >= 20 && i < 30;
        d.chAsked = !pause; d.bandOff = pause;
        room += pause ? -0.06f : risePerWindow / 30.0f;
        d.room = room;
      } else if (hold) {
        d.chAsked = i < 35; d.bandOff = i >= 35;
        d.room = 19.0f + 0.2f * (float)(i < 35 ? i : 35) / 30.0f;
      } else {
        d.room = 19.0f + risePerWindow * (float)i / 30.0f;    // 30 ticks = 15 min
      }
      satDiagEventsStep(d);
      if (state.sat.bHeatingFailure && firstTick < 0) firstTick = i;
    }
    printf("first=%d active=%d events=%u start=%.2f end=%.2f\n", firstTick, (int)state.sat.bHeatingFailure,
           (unsigned)state.sat.iHfEvents, state.sat.fHfRoomStart, state.sat.fHfRoomEnd);
  }
  printf("LOG\n%s", g_log.c_str());
  return 0;
}
"""


class DiagEvents(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cpp = Path(cls.tmp.name) / "t.cpp"
        block = cut(source())
        if "static void satDiagEventsReset()" not in block:   # code before the TASK-1882 review: no reset, so the
            block += "\nstatic void satDiagEventsReset() {}\n"   # satoff case runs and fails on behaviour, not on compile
        cpp.write_text(HARNESS.replace("@@CUT@@", block), encoding="utf-8")
        cls.exe = compile_cpp(cpp, Path(cls.tmp.name) / "t.exe")
        if cls.exe is None:
            raise unittest.SkipTest("no host C++ compiler")

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def run_case(self, c):
        out = subprocess.run([str(self.exe), c], capture_output=True, text=True, check=True).stdout
        head, log = out.split("LOG\n", 1)
        print(c, head.strip())
        return dict(kv.split("=") for kv in head.split()), log

    def test_no_ignition_after_180_s_of_ch_without_flame(self):
        r, log = self.run_case("noign")
        self.assertEqual(r["first"], "6", "tick 6 = 180 s after the first asking tick")
        self.assertEqual((r["active"], r["events"]), ("1", "1"))
        self.assertIn("SATEVENT no_ignition=1 asked_s=180", log)

    def test_a_flame_clears_and_prevents_it(self):
        r, log = self.run_case("flame")
        self.assertEqual((r["first"], r["active"], r["events"]), ("-1", "0", "0"))

    def test_flow_above_cs_is_a_boiler_that_rightly_stays_off(self):
        r, _ = self.run_case("flow")
        self.assertEqual((r["first"], r["events"]), ("-1", "0"))

    def test_a_hole_adds_no_asked_time_but_keeps_what_was_counted(self):
        # TASK-1882 review: the tick after a 90 s hole adds nothing (the hole is not counted as asked time), the 60 s
        # counted before it stay: ticks 1-2 add 60 s, tick 3 (after the hole) adds 0, ticks 4-7 add 120 s -> 180 s at 7
        r, _ = self.run_case("gap")
        self.assertEqual(r["first"], "7")

    def test_pwm_without_a_flame_raises_it_once_and_holds_it(self):
        # TASK-1882 review: the asked time adds up over PWM ON phases; an OFF phase pauses it and does not clear the flag
        # (before: it flapped every cycle and counted cycles, not incidents)
        r, _ = self.run_case("pwm")
        self.assertEqual((r["events"], r["flips"], r["active"]), ("1", "1", "1"))
        self.assertEqual(r["first"], "11", "180 s of asked time: ON ticks 1-3 add 90 s, ON ticks 9-11 add 90 s more")

    def test_a_flame_in_a_later_on_phase_clears_it(self):
        r, log = self.run_case("pwmflame")
        self.assertEqual(r["active"], "1", "set again after the flame: still a boiler that does not light")
        self.assertIn("no_ignition=0 reason=flame", log)

    def test_sat_switch_off_clears_it(self):
        r, log = self.run_case("satoff")
        self.assertEqual(r["active"], "0")
        self.assertIn("SATEVENT no_ignition=0 reason=sat_off", log)

    def test_heating_failure_when_the_room_rises_less_than_half_a_degree(self):
        r, log = self.run_case("hf_20")
        self.assertEqual(r["first"], "30", "the first full 15 min window ends at tick 30")
        self.assertEqual((r["active"], r["events"]), ("1", "1"), "one event for the whole run, not one per window")
        self.assertAlmostEqual(float(r["end"]) - float(r["start"]), 0.2, places=2)
        self.assertRegex(log, r"SATEVENT heating_failure=1 room_start=19\.00 room_end=19\.20 rise=0\.20 window_s=900")

    def test_no_heating_failure_when_the_room_rises_enough(self):
        r, log = self.run_case("hf_60")
        self.assertEqual((r["first"], r["events"]), ("-1", "0"))
        self.assertIn("SATEVENT heating_failure=0 room_start=19.00", log, "every window is logged with its numbers")

    def test_a_band_pause_cooling_is_not_a_heating_failure_q428a(self):
        # Q-428a, entry 501: the window counts only CH-asked time. The room rises 0.6 C per 15 min while CH is asked and cools 0.6 C in
        # a 10-tick band pause. Old code: the window spans the pause, the net rise is below 0.5 C, so it flags a failure at tick 30.
        r, log = self.run_case("hf_band")
        self.assertEqual((r["first"], r["events"], r["active"]), ("-1", "0", "0"))

    def test_a_flagged_failure_stays_through_a_band_pause_q428a(self):
        r, _ = self.run_case("hf_hold")
        self.assertEqual((r["first"], r["active"], r["events"]), ("30", "1", "1"))

    def test_below_the_maximum_or_an_invalid_room_gives_none(self):
        self.assertEqual(self.run_case("hf_below")[0]["events"], "0")
        r, _ = self.run_case("hf_invalid")
        self.assertEqual(r["first"], "51", "the invalid room at tick 20 restarts the window: 21 + 30")


class NoControlAction(unittest.TestCase):
    def test_the_step_sends_nothing(self):
        body = re.sub(r"//[^\n]*", "", cut(source()))   # code only
        self.assertNotRegex(body, r"satSend(CS|MM|CH|Command)|addCommandToQueue|satEnqueue|sendPICSerial|satSetControlMode")
        own = {"state.sat.bNoIgnition", "state.sat.bHeatingFailure", "state.sat.fHfRoomStart", "state.sat.fHfRoomEnd",
               "state.sat.iHfEvents", "state.sat.iNoIgnitionEvents"}
        for target in re.findall(r"\b(state\.\w+\.\w+)\s*(?:=(?!=)|\+\+)", body):
            self.assertIn(target, own, f"the step writes {target}")
        for target in re.findall(r"\b(settings\.\w+\.\w+)\s*(?:=(?!=)|\+\+)", body):
            self.fail(f"the step writes the setting {target}")

    def test_the_tick_calls_it_after_the_cs_is_decided_and_before_it_is_sent(self):
        src = source()
        call = src.index("satDiagEventsStep(d);")
        self.assertLess(src.index("state.sat.fFinalSetpoint = finalSetpoint;", src.index("static void satControlLoopRun()")), call)
        self.assertLess(call, src.index("satSendCS(finalSetpoint);", call))


class ChAskedFollowsTheBand(unittest.TestCase):
    """TASK-3034: the CH decision the diagnostic step reads is the one satSendCH sends.

    STRUCTURAL: the call sites sit inside satControlLoopRun(), which cannot be compiled on the host, so this reads the
    source. marceld91d beta.410 capture: 10 of 10 no_ignition events fired inside 'Heating paused (CH off, PWM stays
    active)' windows, CH bit 0 all the time, because d.chAsked called satChEnableDecide() without the band flag. The
    helper itself is covered with bandOff in test_sat_ch_enable.cpp; the bench (TASK-3034 AC 4) covers the live path."""

    def test_every_chenable_decision_in_the_tick_passes_the_band_flag(self):
        src = re.sub(r"//[^\n]*", "", source())
        run = src[src.index("static void satControlLoopRun()"):]
        calls = re.findall(r"satChEnableDecide\(([^;]*?)\)\s*[;)&|]", run)
        self.assertGreaterEqual(len(calls), 2, calls)
        for args in calls:
            self.assertIn("_sat_tolBandOff", args, f"satChEnableDecide({args}) ignores the tolerance band")

    def test_the_diag_input_uses_the_same_decision_as_the_send(self):
        src = re.sub(r"//[^\n]*", "", source())
        diag =re.search(r"d\.chAsked\s*=([^;]*);", src).group(1)
        send = re.search(r"satSendCH\((satChEnableDecide\([^;]*\))\);", src).group(1)
        inner = lambda t: re.sub(r"\s+", "", t[t.index("satChEnableDecide("):].rstrip(")"))
        self.assertEqual(inner(diag), inner(send))


if __name__ == "__main__":
    unittest.main(verbosity=2)
