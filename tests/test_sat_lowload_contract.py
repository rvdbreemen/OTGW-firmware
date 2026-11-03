#!/usr/bin/env python3
"""TASK-1412.3: executable SAT low-load contract regressions, one test per LL fixture (LL-001..004, LL-010..023).

Definitions: docs/sat/sat-low-load-verification-plan-2026-10-28.md section 3 (LL-001..LL-023) and
docs/sat/sat-low-load-requirements-crosswalk-2026-10-31.md. Nothing is re-implemented: the production functions of
SATcontrol.ino / SATcycles.ino / SATpid.ino are cut out and compiled on the host by the existing helpers (tests/test_sat_hp_pwm.py,
test_sat_overshoot_sustain.py, test_sat_pid_cadence.py, test_sat_pwm_override.compile_cpp). A fixture that an existing suite already
covers calls that suite's test (see DELEGATED) instead of copying it.

Every fixture has exactly one of four kinds (reported per LL id at the end and written to OTGW_LL_EVIDENCE=<file.json> if set):
  contract          passes on the current code (a passing contract: it must stay green)
  known_defect      reproduced against the current code and marked unittest.expectedFailure; the task named in the decorator
                    fixes it. When the fix lands the test turns into an UNEXPECTED SUCCESS (a failure), which is the signal to
                    remove the marker: red-then-green without disabling any unrelated assertion.
  open_decision     expectedFailure because the approved oracle depends on an open SAT question (QD-8, Conf-1371, QD-1..3, ...)
  unavailable       cannot run on the host (needs PH-FIELD data or a bench); skipped in normal mode
Source hashes: sha256 of the production files, of the generated host program (cut code + harness) per compiled fixture, and of the
delegated test modules; the git HEAD and the OTGW_SAT* override variables are recorded too.

RELEASE MODE: OTGW_LL_RELEASE=1 rejects the run (exit 3) if any LL test is skipped, expectedFailure or an unexpected success, if an
LL id is missing, or if no host compiler exists. Normal mode: exit 0 (all LL fixtures behave as recorded), 1 (a contract failed or a
known defect disappeared without the marker being removed), 2 (infrastructure: no host C++ compiler).

Run: python tests/test_sat_lowload_contract.py
"""
import hashlib
import io
import json
import os
import re
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

TESTS = Path(__file__).resolve().parent
REPO = TESTS.parent
FW = REPO / "src" / "OTGW-firmware"
sys.path.insert(0, str(TESTS))
import test_sat_hp_pwm as hp  # noqa: E402
import test_sat_overshoot_sustain as sustain  # noqa: E402
import test_sat_pid_cadence as pidc  # noqa: E402
from test_sat_pwm_override import compile_cpp  # noqa: E402

HP_MAIN_ORIG = hp.MAIN   # test_sat_pwm_anchors replaces hp.MAIN for good in its setUpClass; every group starts from the original
RELEASE = os.environ.get("OTGW_LL_RELEASE") == "1"
EVIDENCE = os.environ.get("OTGW_LL_EVIDENCE")
ALL_IDS = ["LL-001", "LL-002", "LL-003", "LL-004"] + ["LL-%03d" % n for n in range(10, 24)]

REGISTRY = {}   # test method name -> {"ll", "kind", "functions", "task", "question", ...}
HARNESS_HASH = {}


def sha(data) -> str:
    return hashlib.sha256(data if isinstance(data, bytes) else data.encode("utf-8")).hexdigest()


def file_sha(path: Path) -> str:
    return sha(path.read_bytes()) if path.exists() else "missing"


def ll(ident, kind, functions, task="", question=""):
    """Record the fixture's kind; known_defect/open_decision also apply expectedFailure."""
    def deco(fn):
        REGISTRY[fn.__name__] = {"ll": ident, "kind": kind, "functions": functions, "task": task, "question": question}
        if kind in ("known_defect", "open_decision"):
            fn = unittest.expectedFailure(fn)
        return fn
    return deco


# ---- shared compiled programs (one compile per harness for the whole run) -------------------------------------------------
_cache = {}


def _build(key, make):
    if key not in _cache:
        d = tempfile.mkdtemp(prefix="ll-" + key + "-")
        _cache[key] = make(Path(d))
    return _cache[key]


def pwm_exe():
    """The real PWM block + CH helper (hp.build) with a three-tick LL-001/LL-002 main."""
    def make(d):
        old = hp.MAIN
        hp.MAIN = hp.MAIN[:hp.MAIN.index("int main")] + r'''
int main(int argc, char** argv) {
  reset(); g_source=SAT_SRC_GAS_BOILER; settings.sat.iCyclesPerHour=3;
  g_flame=false; g_flameOnStart=g_ms-4000000UL; g_flameOffStart=g_ms-3000000UL;   // last flame-off 3000 s ago
  _pwm_lastUpdateMs=g_ms-3000000UL; _pwm_idle=false; _pwm_entryPending=false; state.sat.bPwmFlameRequested=false;   // OFF wait has run out (the OFF anchor is PWM's own ON->OFF switch since TASK-1380)
  const bool ultra = argc > 1 && !strcmp(argv[1], "ultra");
  const float request=ultra ? 28.568f : 38.6f;
  for(int i=0;i<3;i++) {
    const float cs=satApplyPWM(request);
    const unsigned ch=satChEnableDecide(request,g_cold,satPwmDrivesSetpoint(request),state.sat.bPwmFlameRequested);
    printf("elapsed_s=%d request=%.3f duty=%.6f on_s=%.3f off_s=%.3f phase_on=%d cs=%.3f ch=%u\n",
      i*30,request,state.sat.fPwmDutyCycle,_pwm_lastOnTimeMs/1000.0,_pwm_lastOffTimeMs/1000.0,
      (int)state.sat.bPwmFlameRequested,cs,ch);
    g_ms+=30000UL;
  }
}
'''
        try:
            exe = hp.build(d)
        finally:
            hp.MAIN = old
        if exe:
            HARNESS_HASH["pwm"] = sha((d / "hp.cpp").read_text(encoding="utf-8"))
        return exe
    return _build("pwm", make)


def pid_exe():
    def make(d):
        sat = pidc.SAT_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        pid = pidc.PID_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        text = pidc.build_cpp(sat, pid)
        cpp = d / "t.cpp"
        cpp.write_text(text, encoding="utf-8")
        HARNESS_HASH["pid"] = sha(text)
        return compile_cpp(cpp, d / "t.exe")
    return _build("pid", make)


def sustain_exe():
    def make(d):
        src = sustain.CYCLES_INO.read_text(encoding="utf-8").replace("\r\n", "\n")
        flag, body = sustain.extract(src)
        text = flag + sustain.STUBS + body + sustain.MAIN
        cpp = d / "t.cpp"
        cpp.write_text(text, encoding="utf-8")
        HARNESS_HASH["sustain"] = sha(text)
        return compile_cpp(cpp, d / "t.exe")
    return _build("sustain", make)


def fields(line):
    return {k: float(v) for k, v in (p.split("=", 1) for p in line.split())}


def run(argv, **kw):
    return subprocess.run([str(a) for a in argv], capture_output=True, text=True, check=True, timeout=60, **kw).stdout


# ---- delegation to the existing suites ------------------------------------------------------------------------------------
# module -> class -> test names. One group runs once per module/class (one compile), the LL test then asserts its names.
DELEGATED = {
    "LL-010": [("test_sat_slave_lowload", "SlaveLowLoad", None)],
    "LL-011": [("test_sat_pwm_override", "SatPwmOverride", ["test_no_flow_temperature_idles_pwm_and_sends_the_continuous_setpoint"]),
               ("test_sat_no_return_ignition", "NoReturnIgnition", ["test_a_restored_unknown_dataid_verdict_alone_is_no_return_nothing",
                                                                    "test_valid_board_sensor_counts_as_a_return_temperature"])],
    "LL-012": [("test_sat_overshoot_sustain", "SustainedSwitches", ["test_dhw_guard_blocks_overshoot_for_300s_after_the_draw",
                "test_dhw_guard_overshoot_exact_window", "test_dhw_guard_inactive_without_a_draw_near_the_millis_wrap",
                "test_dhw_resets_underheat_and_blocks_it_for_300s", "test_dhw_resets_saturation_and_blocks_it_for_300s"]),
               ("test_sat_overshoot_sustain", "GateHeatPumpSwitch", ["test_heat_pump_overshoot_switches_to_pwm"]),
               ("test_sat_pwm_anchors", "SatPwmAnchors", ["test_e3_hot_water_flame_inside_on_adds_no_count", "test_f1_draw_in_on_phase_resumes_with_the_remaining_on_time_after_the_ch_relight",
                "test_f2_no_ch_flame_after_the_draw_resumes_180_s_after_the_draw_end", "test_f4_draw_in_off_phase_keeps_the_off_wait_from_the_switch",
                "test_h1_no_on_start_while_the_hot_water_flame_burns", "test_k1_a_draw_shorter_than_one_tick_still_marks_its_flame",
                "test_m3_short_draw_inside_the_hold_keeps_the_rest_of_the_hold"])],
    "LL-013": [("test_sat_cmd_cache", "CmdCacheReset", ["test_dropped_enqueue_is_not_cached_and_sent_again", "test_dropped_satsendcs_is_retried_by_the_5s_resend",
                "test_dropped_keepalive_is_tried_again_on_the_next_pass", "test_resend_repeats_last_values_after_5s_not_before",
                "test_mm_and_ch_only_on_change_and_again_after_pic_restart", "test_unchanged_mm_and_ch_repaired_after_60_s",
                "test_reset_across_uint32_wrap"])],
    "LL-014": [("test_sat_max_tset", "SatMaxTSet", ["test_not_chosen_boiler_maxtset_below_default_still_caps_cs", "test_bounds_clamp_upper", "test_bounds_clamp_lower",
                "test_bounds_arriving_later_resend_the_clamped_value", "test_invalid_bounds_fall_back_to_20_80", "test_change_sends_again_once",
                "test_heat_pump_chosen_55_is_the_cap_and_sh", "test_underfloor_never_above_50"]),
               ("test_sat_no_return_ignition", "NoReturnIgnition", ["test_hold_is_clamped_to_the_user_max"])],
    "LL-015": [("test_sat_hp_pwm", "SatHeatPumpPwm", ["test_hp1800_mid_range_duty_060", "test_hp2400_mid_range_duty_050", "test_hp_minimum_burn_600_holds",
                "test_hp_min_off_600_in_range5", "test_gas_oracle_4ph", "test_gas_3ph_default", "test_gas_on_phase_min_on_180",
                "test_hybrid_uses_the_heat_pump_cycle", "test_cph_getter_heat_pump_period_ignores_cph"]),
               ("test_sat_pwm_start_limit", "SatPwmStartLimit", ["test_hp_has_no_hourly_start_limit"])],
    "LL-017": [("test_sat_pwm_anchors", "SatPwmAnchors", ["test_ll017_off_phase_stops_the_boiler_without_overshoot_protection",
                "test_ll017_on_phase_without_protection_has_no_hold_or_suppression", "test_ll017_toggling_protection_keeps_the_phase_timing_and_the_off_phase_actuation",
                "test_ll017_protection_off_inside_an_on_phase_keeps_its_cs_until_the_off_phase",
                "test_ll017_heat_pump_off_phase_holds_cs10_for_the_600_s_minimum_off_without_protection", "test_ll017_continuous_path_does_not_read_overshoot_protection",
                "test_ll017_cold_start_without_protection_waits_with_the_requested_setpoint"])],
    "LL-018": [("test_sat_no_return_ignition", "NoReturnIgnition", ["test_hold_is_flow_plus_offset_without_return_temperature",
                "test_flood_alternates_cs_and_pm25_each_second_for_20_s_then_suppression", "test_no_stale_cs_interleaves_during_the_flood",
                "test_tick_inside_the_flood_sends_the_live_hold", "test_relight_after_hot_water_repeats_the_flood_and_a_dhw_flame_does_not",
                "test_normal_boiler_is_unchanged", "test_mi500_once_on_pic_after_no_return_detection", "test_mi500_never_on_ot_direct",
                "test_a_break_in_the_no_return_fact_restarts_the_5_minutes"])],
    "LL-019": [("test_sat_pwm_anchors", "SatPwmAnchors", ["test_d1_off_wait_ignores_hot_water_flame_offs", "test_d2_on_phase_without_flame_does_not_end_on_its_first_tick",
                "test_d4_no_flame_within_180_s_does_not_abort_on", "test_d5_on_time_below_180_s_never_starts_on", "test_d6_relight_inside_on_does_not_restart_the_timer",
                "test_d7_first_flame_after_the_wait_timed_out_does_not_resync_but_holds_180_s", "test_d8_late_ignition_keeps_on_until_180_s_after_flame_on",
                "test_e1_unlit_on_start_counts_once", "test_e2_ch_ignition_and_relight_inside_on_add_no_count", "test_h3_range_5_is_held_the_same_way"]),
               ("test_sat_hp_pwm", "SatHeatPumpPwm", ["test_r3_on_phase_waits_for_flame", "test_gas_range5_restarts_without_min_off"])],
    "LL-020": [("test_sat_pid_cadence", "Cadence", ["test_a_integral_and_step_count_independent_of_the_control_tick", "test_b_millis_wrap_gives_the_same_steps_and_integral",
                "test_c_loop_stall_gives_one_step", "test_d_startup_step_at_the_first_pass", "test_f_no_room_gives_curve_only_never_nan",
                "test_j_room_back_gives_curve_only_until_the_next_step"]),
               ("test_sat_previous_cs_clamp", "PreviousCsClamp", ["test_a_falling_request_holds_previous_cs", "test_b_rising_request_follows_min_allowed",
                "test_c2_equal_request_takes_the_falling_branch"]),
               ("test_sat_overshoot_sustain", "TickClockAndLoopOrder", None)],
    "LL-021": [("test_sat_cycle_classifier", "CycleClassifier", None)],
    "LL-023": [("test_sat_ground_truth_block", "SatGroundTruthBlock", None)],
}
_delegated = {}   # (module, class) -> {test name: (ok, detail)}


def delegated_results(module, cls, names):
    key = (module, cls)
    if key not in _delegated:
        hp.MAIN = HP_MAIN_ORIG
        mod = __import__(module)
        klass = getattr(mod, cls)
        wanted = names if names is not None else unittest.TestLoader().getTestCaseNames(klass)
        suite = unittest.TestSuite(klass(n) for n in wanted)
        res = unittest.TestResult()
        suite.run(res)
        out = {n: (True, "") for n in wanted}
        for t, tb in res.failures + res.errors:
            out[t._testMethodName] = (False, tb[-600:])
        for t, why in res.skipped:
            out[t._testMethodName] = (False, "skipped: %s" % why)
        for t, _ in res.expectedFailures:
            out[t._testMethodName] = (False, "expectedFailure in the delegated suite")
        for t in res.unexpectedSuccesses:
            out[t._testMethodName] = (False, "unexpected success in the delegated suite")
        _delegated[key] = out
    return _delegated[key]


class LowLoadContract(unittest.TestCase):
    def need(self, exe):
        if not exe:
            raise InfrastructureError("no host C++ compiler (g++, clang++ or MSVC)")
        return exe

    def rec(self):
        return REGISTRY[self._testMethodName]

    def delegated(self, ident):
        """Every delegated test of this LL id must pass: not skipped, not expectedFailure."""
        bad = []
        n = 0
        for module, cls, names in DELEGATED[ident]:
            res = delegated_results(module, cls, names)
            for name, (ok, detail) in res.items():
                n += 1
                if not ok:
                    bad.append("%s.%s.%s: %s" % (module, cls, name, detail))
        self.assertEqual(bad, [], "%d delegated tests, failing:\n%s" % (n, "\n".join(bad)))
        self.rec()["delegated_tests"] = n
        self.rec()["delegated_modules"] = sorted({m for m, _, _ in DELEGATED[ident]})

    # LL-001 / LL-002 ----------------------------------------------------------------------------------------------------
    def pwm_rows(self, scenario):
        exe = self.need(pwm_exe())
        return [fields(line) for line in run([exe, scenario]).splitlines() if line]

    @ll("LL-001", "contract", "satApplyPWM, satPwmDrivesSetpoint, satChEnableDecide (SATcontrol.ino, via test_sat_hp_pwm.build)",
        task="TASK-1383/Q38 (fixed; passing since TASK-1383)")
    def test_ll001_mapped_on_time_zero_never_requests_on(self):
        rows = self.pwm_rows("ultra")
        self.assertEqual([r["elapsed_s"] for r in rows], [0, 30, 60])
        for r in rows:
            self.assertAlmostEqual(r["request"], 28.568, places=3)
            self.assertEqual((r["on_s"], r["phase_on"], r["ch"]), (0, 0, 0), "mapped ON=0 must not request ON/CH: %s" % r)
            self.assertAlmostEqual(r["cs"], 10.0, places=3, msg="OFF with protection enabled sends CS=10: %s" % r)

    @ll("LL-002", "contract", "satApplyPWM, satChEnableDecide (SATcontrol.ino, via test_sat_hp_pwm.build)",
        task="TASK-1382/Q35 (fixed; passing since TASK-1382)")
    def test_ll002_on_request_stays_asserted_while_waiting_for_ignition(self):
        rows = self.pwm_rows("slow")
        self.assertEqual([r["elapsed_s"] for r in rows], [0, 30, 60])
        for r in rows:
            self.assertEqual((r["phase_on"], r["ch"]), (1, 1), "the ON request ends before ignition: %s" % r)

    # LL-003 -------------------------------------------------------------------------------------------------------------
    @ll("LL-003", "contract", "whole SATpid.ino (satPidService/NoteRoom/Output/Reset), real safeTimers.h, satZonePid*, satPidGatherInputs stub (test_sat_pid_cadence.build_cpp)",
        task="TASK-1368 (fixed)")
    def test_ll003_integral_is_independent_of_the_control_tick(self):
        exe = self.need(pid_exe())

        def integral(tick):
            last = run([exe, "cad", tick, 1000000]).strip().split("\n")[-1]
            return float(pidc.parse(last)["I"])
        ref = integral(60)
        got = {t: integral(t) for t in (10, 30, 60, 120)}
        for tick, value in got.items():
            self.assertAlmostEqual(value, ref, delta=1e-4, msg="I at a %d s control tick differs from the 60 s reference %.6f: %s" % (tick, ref, got))
        self.rec()["observed"] = {"reference_I_60s": ref, "I_by_tick_s": got}

    # LL-004 -------------------------------------------------------------------------------------------------------------
    def exit_after(self, interval_ms, kind, jitter=(0,), start=None):
        """First-qualifying-sample time -> exit time in seconds, or None. Tokens are dt:kind@lateness (test_sat_overshoot_sustain.MAIN)."""
        exe = self.need(sustain_exe())
        toks = ["%d:%s@%d" % (interval_ms, kind, jitter[i % len(jitter)]) for i in range(40)]
        argv = ([ "start=%d" % start] if start is not None else []) + toks
        t = fields(run([exe, *argv]).strip())
        if t["t"] < 0:
            return None, t
        # the first qualifying tick is the first token; its time is the first token's dt (+lateness)
        first = interval_ms + jitter[0]
        return (t["t"] - first) / 1000.0, t

    @ll("LL-004", "contract", "satCycleCheckOvershootSwitch / satCycleCheckPwmExits (SATcycles.ino, via test_sat_overshoot_sustain.extract)",
        task="TASK-1420 (fixed; entries 86, 95, 109)")
    def test_ll004_underheat_and_saturation_exit_after_180_300_s_from_the_first_qualifying_sample(self):
        rows = []
        for interval in (30, 60, 90, 120, 300):
            for kind, threshold in (("u", 180), ("z", 300)):
                elapsed, t = self.exit_after(interval * 1000, kind)
                rows.append((interval, kind, threshold, elapsed))
                with self.subTest(interval_s=interval, condition=kind):
                    self.assertIsNotNone(elapsed, "no exit at all")
                    self.assertGreaterEqual(elapsed, threshold)
                    self.assertLessEqual(elapsed, threshold + interval)
                    self.assertEqual(int(t["mode"]), 1)
        self.rec()["observed"] = rows

    # LL-010 -------------------------------------------------------------------------------------------------------------
    @ll("LL-010", "contract", "satNoteSlaveConfig, satPwmPermitted, satPwmEntryAllowed, satSetControlMode, satRestoreControlMode, satHandleControlMode, "
        "satCycleCheckOvershootSwitch, satCycleCheckLowLoadVeto (test_sat_slave_lowload; entries 213-215, ADR-157)",
        task="TASK-1412.4: HB4 = 1 and no valid reply block the overshoot switch, the restart restore, the pwm command and any other entry; "
             "HB4 turning 1 leaves PWM in that tick")
    def test_ll010_hb4_permission_is_consulted_before_low_load_cycling(self):
        self.delegated("LL-010")

    # LL-011 -------------------------------------------------------------------------------------------------------------
    @ll("LL-011", "contract", "satApplyPWM flow-sentinel path; no-return verdict and board-sensor return (test_sat_pwm_override, test_sat_no_return_ignition)",
        task="partial: 0.0 flow sentinel, Unknown-DataID verdict and board-sensor return only")
    def test_ll011_flow_sentinel_unknown_dataid_and_board_sensor_return(self):
        self.delegated("LL-011")

    @ll("LL-011", "known_defect", "none: SAT keeps no per-ID age or Data-Invalid state for IDs 25/28 (structural check)",
        task="TASK-1412.5", question="QD-5, QD-6")
    def test_ll011b_per_id_age_and_invalid_state_exist(self):
        sat = (FW / "SATcontrol.ino").read_text(encoding="utf-8") + (FW / "SATtypes.h").read_text(encoding="utf-8")
        self.assertRegex(sat, r"(?i)(flow|return)\w*(Age|Fresh|Invalid|SeenMs|LastMs)\w*", "no per-ID age/validity for the flow and return temperature")

    # LL-012 .. LL-015, LL-017 .. LL-021, LL-023 -------------------------------------------------------------------------
    @ll("LL-012", "contract", "satCycleCheck* DHW guard, satApplyPWM DHW pause/resume (SATcycles.ino, SATcontrol.ino)",
        task="partial: existing DHW cases; setpoint fall, max limiting, pump step, flow None are TASK-1412.7")
    def test_ll012_dhw_transitions(self):
        self.delegated("LL-012")

    @ll("LL-012", "open_decision", "none: no one-minute post-hot-water setpoint hold exists (structural)", task="TASK-1412.7", question="QD-8")
    def test_ll012b_post_dhw_one_minute_setpoint_hold(self):
        sat = (FW / "SATcontrol.ino").read_text(encoding="utf-8")
        self.assertRegex(sat, r"(?i)post[- ]?(dhw|hot[- ]?water)[^\n]*hold[^\n]*(60|one minute)", "no post-DHW setpoint hold; QD-8 not asked yet")

    @ll("LL-013", "contract", "satEnqueueOwnCS/satSendCS/satCmdResend/satEnqueueIfChanged* (SATcontrol.ino, via test_sat_cmd_cache)",
        task="TASK-1574 (commit-on-success); wire ordering and PIC/boiler acks stay TASK-1412.11")
    def test_ll013_dropped_enqueue_is_not_cached_and_is_retried(self):
        self.delegated("LL-013")

    @ll("LL-014", "contract", "satEffective MaxTSet / CS ceiling and SH negotiation (SATcontrol.ino, via test_sat_max_tset)",
        task="partial: static limits; lowering the limit during ON and a rejected write are TASK-1412.13")
    def test_ll014_every_cs_obeys_the_effective_ceiling(self):
        self.delegated("LL-014")

    @ll("LL-015", "contract", "satGetPwmPeriodSec, satGetMaxCyclesPerHour, satApplyPWM per source (via test_sat_hp_pwm, test_sat_pwm_start_limit)",
        task="partial: mapper and phase arithmetic; full-loop automatic entry per source is TASK-1412.7/9", question="Q-1527a (heat pumps)")
    def test_ll015_source_profiles_period_and_minimums(self):
        self.delegated("LL-015")

    @ll("LL-017", "contract", "satApplyPWM, satPwmSuppressionRefresh, continuous path (via test_sat_pwm_anchors)",
        task="TASK-1412.6 (QD-4 B, entry 230): every PWM OFF phase stops the boiler")
    def test_ll017_overshoot_protection_toggle(self):
        self.delegated("LL-017")

    @ll("LL-018", "contract", "satPwmSuppressionRefresh, no-return flood/hold, MI=500 (via test_sat_no_return_ignition)",
        task="TASK-1371 (fixed); Data-Invalid/stale return is LL-018b")
    def test_ll018_missing_return_ignition(self):
        self.delegated("LL-018")

    @ll("LL-018", "open_decision", "none: a return temperature that stops arriving (Data-Invalid/stale) is not detected (structural)",
        task="TASK-1412.12", question="Conf-1371, QD-5")
    def test_ll018b_stale_or_invalid_return_is_treated_as_missing(self):
        sat = (FW / "SATcontrol.ino").read_text(encoding="utf-8")
        self.assertRegex(sat, r"(?i)return\w*(Stale|Invalid|Age|SeenMs|LastMs)\w*", "no per-ID stale/invalid return handling (BoilerLastSeenMs is whole-link liveness and does not count)")

    @ll("LL-019", "contract", "satApplyPWM phase anchors, ignition wait, start counting (via test_sat_pwm_anchors, test_sat_hp_pwm)",
        task="TASK-1380/1382/1383/1421 (fixed); fourth start/wrap/range changes are TASK-1412.9")
    def test_ll019_phase_clocks_and_start_counts(self):
        self.delegated("LL-019")

    @ll("LL-020", "contract", "SATpid.ino service/output, satPreviousCs clamp, tick clock (via test_sat_pid_cadence, previous_cs_clamp, overshoot_sustain)",
        task="partial: PID cadence, wrap, stall, clamp rule; jitter/delayed-tick exit sweep is LL-020b")
    def test_ll020_pid_cadence_clamp_and_tick_clock(self):
        self.delegated("LL-020")

    @ll("LL-020", "contract", "satCycleCheckOvershootSwitch / satCycleCheckPwmExits with late ticks and a millis wrap (SATcycles.ino)",
        task="TASK-1412.8 AC2 (jitter and millis rollover)")
    def test_ll020b_exit_timing_with_jitter_and_millis_wrap(self):
        results = []
        for kind, threshold in (("u", 180), ("z", 300)):
            for jitter in ((0, 7000, 3000, 5000), (0, 25000), (12000,)):
                for start in (None, 4294967295 - 100000):
                    elapsed, t = self.exit_after(30000, kind, jitter=jitter, start=start)
                    results.append((kind, jitter, start, elapsed))
                    with self.subTest(kind=kind, jitter_ms=jitter, start=start):
                        self.assertIsNotNone(elapsed)
                        self.assertGreaterEqual(elapsed, threshold, "early exit")
                        # one tick later at most, where a tick may itself be late by the largest lateness
                        self.assertLessEqual(elapsed, threshold + 30 + max(jitter) / 1000.0, "late exit")
        self.rec()["observed"] = results

    @ll("LL-021", "contract", "satCycleClassify* C++ classifier (via test_sat_cycle_classifier)",
        task="partial: C++ side only; the Python side of the shared trace is TASK-1412.14 (no Python SAT here)")
    def test_ll021_classifier_rules_and_window_inputs(self):
        self.delegated("LL-021")

    @ll("LL-023", "contract", "docs/AGENTS ground-truth block checks (via test_sat_ground_truth_block) and LL id coverage in the crosswalk",
        task="partial: structural; independent citation review stays human (TASK-1412.26)")
    def test_ll023_documents_name_every_ll_id_and_the_ground_truth_block_is_intact(self):
        self.delegated("LL-023")
        plan = (REPO / "docs/sat/sat-low-load-verification-plan-2026-10-28.md").read_text(encoding="utf-8")
        for ident in ALL_IDS:
            self.assertIn("**%s**" % ident, plan, "%s missing in the verification plan table" % ident)
        self.assertTrue((REPO / "docs/sat/sergeant-d-corrections.md").exists())

    # unavailable on the host --------------------------------------------------------------------------------------------
    @ll("LL-016", "unavailable", "none", task="TASK-1412.16/19 (plant models), PH-FIELD measurements")
    def test_ll016_room_comfort_and_delivered_heat_under_ph_field(self):
        self.skipTest("unavailable: needs calibrated delivered heat/energy and PH-FIELD data (comfort noninferiority), not a host function")

    @ll("LL-022", "unavailable", "none", task="TASK-1412.15 (and TASK-1389 for the percentage unit)")
    def test_ll022_telemetry_separates_requested_queued_acknowledged_measured(self):
        self.skipTest("unavailable: needs queue-rejection/ack events on a device or the wire; no host adapter yet (TASK-1412.15)")


class InfrastructureError(Exception):
    pass


# ---- runner: classification, evidence, release mode -----------------------------------------------------------------------
class Result(unittest.TextTestResult):
    def __init__(self, *a, **k):
        super().__init__(*a, **k)
        self.outcome = {}

    def addSuccess(self, t):
        super().addSuccess(t); self.outcome[t._testMethodName] = "pass"

    def addExpectedFailure(self, t, e):
        super().addExpectedFailure(t, e); self.outcome[t._testMethodName] = "expectedFailure"

    def addUnexpectedSuccess(self, t):
        super().addUnexpectedSuccess(t); self.outcome[t._testMethodName] = "unexpectedSuccess"

    def addSkip(self, t, r):
        super().addSkip(t, r); self.outcome[t._testMethodName] = "skip"

    def addFailure(self, t, e):
        super().addFailure(t, e); self.outcome[t._testMethodName] = "fail"

    def addSubTest(self, test, subtest, err):
        super().addSubTest(test, subtest, err)
        if err is not None:   # a failing subTest must not leave the test without an outcome
            self.outcome[test._testMethodName] = "infrastructure" if err[0] is InfrastructureError else "fail"

    def addError(self, t, e):
        super().addError(t, e); self.outcome[t._testMethodName] = "infrastructure" if e[0] is InfrastructureError else "error"


def git(*args):
    try:
        return subprocess.run(["git", *args], cwd=REPO, capture_output=True, text=True).stdout.strip()
    except OSError:
        return ""


def main():
    started = time.time()
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(LowLoadContract)
    only = [x.strip() for x in os.environ.get("OTGW_LL_ONLY", "").split(",") if x.strip()]
    if only:   # a focused red/green run for a fix task; never a release verdict
        suite = unittest.TestSuite(t for t in suite if REGISTRY[t._testMethodName]["ll"] in only)
    stream = io.StringIO()
    runner = unittest.TextTestRunner(stream=stream, verbosity=2, resultclass=Result)
    res = runner.run(suite)
    print(stream.getvalue())
    per_test = []
    print("%-8s %-14s %-10s %s" % ("LL id", "kind", "outcome", "test"))
    seen_ids = set()
    infra = False
    release_reject = []
    for name in sorted(res.outcome):
        rec = REGISTRY[name]; ident = rec["ll"]
        outcome = res.outcome[name]
        seen_ids.add(ident)
        infra = infra or outcome == "infrastructure"
        print("%-8s %-14s %-10s %s%s" % (ident, rec["kind"], outcome, name, ("  [%s %s]" % (rec["task"], rec["question"])).rstrip() if rec["task"] or rec["question"] else ""))
        if outcome in ("skip", "expectedFailure", "unexpectedSuccess"):
            release_reject.append("%s %s: %s" % (ident, name, outcome))
        per_test.append({"ll": ident, "test": name, "kind": rec["kind"], "outcome": outcome, "functions": rec["functions"], "task": rec["task"],
                         "question": rec["question"], "delegated_tests": rec.get("delegated_tests"), "delegated_modules": rec.get("delegated_modules"),
                         "observed": rec.get("observed") or rec.get("observed_jitter")})
    missing = [i for i in ALL_IDS if i not in seen_ids]
    if only:
        print("OTGW_LL_ONLY=%s: focused run, %d LL ids not run" % (",".join(only), len(missing)))
    modules = sorted({m for v in DELEGATED.values() for m, _, _ in v} | {"test_sat_hp_pwm", "test_sat_overshoot_sustain", "test_sat_pid_cadence", "test_sat_lowload_contract"})
    evidence = {
        "utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime(started)),
        "git_head": git("rev-parse", "HEAD"), "git_dirty_src": git("status", "--porcelain", "--", "src").splitlines(),
        "release_mode": RELEASE, "override_env": {k: v for k, v in os.environ.items() if k.startswith("OTGW_SAT")},
        "production_sources_sha256": {n: file_sha(FW / n) for n in ("SATcontrol.ino", "SATcycles.ino", "SATpid.ino", "SATchEnable.h", "SATtypes.h", "safeTimers.h")},
        "harness_program_sha256": HARNESS_HASH,
        "test_modules_sha256": {m: file_sha(TESTS / (m + ".py")) for m in modules},
        "missing_ll_ids": missing, "tests": per_test,
        "runtime_s": round(time.time() - started, 1),
    }
    print("\nruntime %.1f s; missing LL ids: %s" % (evidence["runtime_s"], missing or "none"))
    if EVIDENCE:
        Path(EVIDENCE).write_text(json.dumps(evidence, indent=1, default=str), encoding="utf-8")
        print("evidence written to", EVIDENCE)
    if infra:
        print("INFRASTRUCTURE: no host C++ compiler; this is not a pass"); return 2
    if RELEASE:
        if release_reject or missing or only:
            print("RELEASE MODE REJECTED: skipped/expectedFailure/unexpectedSuccess = %s; missing = %s" % (release_reject, missing))
            return 3
    return 0 if res.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main())
