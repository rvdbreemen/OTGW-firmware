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
  open_decision     expectedFailure because the approved oracle depends on an open SAT question (QD-8, OTD-PM25, ...)
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
    "LL-010": [("test_sat_slave_lowload", "SlaveLowLoad", None),
               ("test_sat_requested_vs_transmitted", "RequestedVsTransmitted", None)],   # TASK-1412.4 AC3: requested vs transmitted master bits
    "LL-011": [("test_sat_pwm_override", "SatPwmOverride", ["test_no_flow_temperature_idles_pwm_and_sends_the_continuous_setpoint"]),
               ("test_sat_no_return_ignition", "NoReturnIgnition", ["test_a_restored_unknown_dataid_verdict_alone_is_no_return_nothing",
                                                                    "test_valid_board_sensor_counts_as_a_return_temperature"])],
    "LL-011b": [("test_sat_flow_return_validity", "FlowReturnValidity", None)],   # TASK-1412.5: every validity class, 60 s flow, 5 min return
    "LL-012": [("test_sat_overshoot_sustain", "SustainedSwitches", ["test_dhw_guard_blocks_overshoot_for_300s_after_the_draw",
                "test_dhw_guard_overshoot_exact_window", "test_dhw_guard_inactive_without_a_draw_near_the_millis_wrap",
                "test_dhw_resets_underheat_and_blocks_it_for_300s", "test_dhw_resets_saturation_and_blocks_it_for_300s"]),
               ("test_sat_overshoot_sustain", "GateHeatPumpSwitch", ["test_heat_pump_overshoot_switches_to_pwm"]),
               ("test_sat_pwm_anchors", "SatPwmAnchors", ["test_e3_hot_water_flame_inside_on_adds_no_count", "test_f1_draw_in_on_phase_resumes_with_the_remaining_on_time_after_the_ch_relight",
                "test_f2_no_ch_flame_after_the_draw_resumes_180_s_after_the_draw_end", "test_f4_draw_in_off_phase_keeps_the_off_wait_from_the_switch",
                "test_h1_no_on_start_while_the_hot_water_flame_burns", "test_k1_a_draw_shorter_than_one_tick_still_marks_its_flame",
                "test_m3_short_draw_inside_the_hold_keeps_the_rest_of_the_hold",
                # TASK-1412.7 AC3: hot-water flame edges and draws never move a phase-owned timer (OFF anchor, ON clock, heat-pump OFF wait)
                "test_d1_off_wait_ignores_hot_water_flame_offs", "test_n9_hot_water_in_the_off_phase_does_not_move_the_heat_pump_off_wait",
                "test_f3_flame_still_lit_after_the_draw_resumes_at_once", "test_g4_second_draw_in_the_post_draw_wait_counts_the_180_s_from_the_last_draw_end",
                "test_d6_relight_inside_on_does_not_restart_the_timer"]),
               # TASK-1412.7 AC1/AC2: the entry cells (startup ramp, setpoint fall, valve/pump step, maximum limit, post-draw heat, real
               # saturation path, no flame/modulation input) and the QD-8 comparison traces on the real continuous clamp
               ("test_sat_overshoot_sustain", "LowLoadEntryCells", None),
               ("test_sat_post_dhw_traces", "PostDhwTraces", None),
               # TASK-1412.7 AC3: master CH enable and the boiler's CH-mode bit are different signals
               ("test_sat_ch_signals", "ChSignals", None),
               ("test_sat_cycle_classifier", "CycleClassifier", ["test_R2_master_ch_enable_decides_the_kind"]),
               ("test_sat_pressure_health", "SatPressureHealth", ["test_ch_enable_transition_suspends_and_clears", "test_sat_on_off_alone_does_not_suspend",
                "test_pwm_off_phase_does_not_suspend"])],
    "LL-013b": [("test_sat_command_delivery", "OtDirectRefusal", None)],
    "LL-013": [("test_sat_cmd_cache", "CmdCacheReset", ["test_dropped_enqueue_is_not_cached_and_sent_again", "test_dropped_satsendcs_is_retried_by_the_5s_resend",
                "test_dropped_keepalive_is_tried_again_on_the_next_pass", "test_resend_repeats_last_values_after_5s_not_before",
                "test_mm_and_ch_only_on_change_and_again_after_pic_restart", "test_unchanged_mm_and_ch_repaired_after_60_s",
                "test_reset_across_uint32_wrap", "test_resend_repeats_the_cs_the_suppression_refresh_sent",
                "test_pic_keepalive_stays_silent_while_the_resend_runs"]),
               # TASK-1412.11 AC1/AC2/AC3: intent, enqueue result, transmission and echo/boiler answer as separate events on the REAL
               # SAT senders and the REAL PIC queue (and the OT-Direct enqueue path); full queue, interrupted exchange, PIC reset,
               # superseded commands, recovery; measured queue latency at echo pacing
               ("test_sat_command_delivery", "CommandDelivery", None),
               ("test_pic_pm_chain", "PmChain", ["test_add_reports_its_result", "test_echo_paced_drain", "test_no_echo_waits_for_the_timeout",
                "test_overrun_is_resent_not_dropped", "test_stale_echo_does_not_remove_the_replacement", "test_unconfirmed_command_retries_behind_newer_ones",
                "test_updated_command_moves_to_the_back", "test_priority_slot_waits_for_the_boiler", "test_external_cs_refused_while_sat_enqueues_its_own",
                "test_cs_behind_a_full_echoing_queue_within_two_seconds", "test_cs_behind_a_full_queue_still_well_within_the_pic_minute",
                "test_simulation_held_back_command_is_consumed_not_dropped", "test_concurrent_add_and_drain_keep_the_queue_consistent"]),
               ("test_sat_release_retry", "ReleaseRetry", None)],
    "LL-014": [("test_sat_max_tset", "SatMaxTSet", ["test_not_chosen_boiler_maxtset_below_default_still_caps_cs", "test_bounds_clamp_upper", "test_bounds_clamp_lower",
                "test_bounds_arriving_later_resend_the_clamped_value", "test_invalid_bounds_fall_back_to_20_80", "test_change_sends_again_once",
                "test_heat_pump_chosen_55_is_the_cap_and_sh", "test_underfloor_never_above_50",
                "test_sh_is_sent_without_any_msgid_6_check_and_with_no_bounds_reported", "test_boiler_answers_are_classified_write_ack_accepted_rejects_rejected",
                "test_a_read_ack_or_a_gateway_answer_says_nothing", "test_rejected_sh_keeps_being_sent_by_the_normal_rules_and_reset_on_a_new_send",
                "test_no_sh_sent_means_no_verdict_is_claimed_by_the_step", "test_core_hook_records_only_boiler_write_replies_of_msgid_57"]),
               ("test_sat_no_return_ignition", "NoReturnIgnition", ["test_hold_is_clamped_to_the_user_max"]),
               ("test_sat_ceiling_paths", "CeilingPaths", None), ("test_sat_ceiling_paths", "PersistedLimits", None),   # TASK-1412.13: every CS path, AC3
               ("test_sat_cs_ceiling_1536", "CsCeiling", None)],
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
    # TASK-1412.12 (LL-018): the same ignition on the REAL measurement validity (tests/test_sat_no_return_startup.py), split by what it proves
    "LL-018c": [("test_sat_no_return_startup", "NoReturnStartup", ["test_unsupported_return_floods_from_the_first_ignition",
                "test_data_invalid_return_waits_5_minutes_then_counts_as_no_return", "test_stale_return_is_the_same_as_an_invalid_one_and_recovers",
                "test_fresh_zero_return_is_a_measurement_not_a_missing_return", "test_board_sensor_return_is_a_return_even_when_the_boiler_says_unknown",
                "test_unsupported_verdict_restored_from_file_is_not_enough_alone", "test_missing_flow_sends_no_flood_and_no_flow_based_hold",
                "test_start_up_window_keeps_the_previous_cs_until_5_minutes_without_a_return"])],
    # TASK-1412.12 (LL-018b, entries 217, 263): the stale / Data-Invalid return rule and the start-up window that keeps the previous CS
    "LL-018b": [("test_sat_no_return_startup", "NoReturnStartup", ["test_start_up_window_keeps_the_previous_cs_until_5_minutes_without_a_return",
                "test_unsupported_verdict_restored_from_file_is_not_enough_alone", "test_data_invalid_return_waits_5_minutes_then_counts_as_no_return",
                "test_stale_return_is_the_same_as_an_invalid_one_and_recovers"])],
    # TASK-1384 (entry 261): the boot PM chain on a PIC board whatever SAT is; OT-Direct unchanged
    "LL-018i": [("test_pic_pm_chain", "PmChain", ["test_boot_chain_on_pic_with_sat_disabled", "test_boot_chain_on_pic_with_sat_enabled_unchanged",
                "test_boot_chain_ot_direct_unchanged", "test_init_sat_starts_the_chain_outside_the_sat_enabled_block"])],
    "LL-018d": [("test_sat_no_return_startup", "NoReturnStartup", ["test_runtime_enable_with_a_running_boiler_sends_no_flood_and_defers_mi500_5_minutes_on_pic",
                "test_runtime_enable_on_ot_direct_never_sends_mi500", "test_runtime_enable_then_pwm_ignition_floods"])],
    "LL-018e": [("test_sat_no_return_startup", "NoReturnStartup", ["test_hot_boiler_in_lockout_holds_the_clamped_flow_hold_and_floods_from_the_flame_not_from_the_on_phase",
                "test_cold_boiler_after_lockout_does_not_flood_before_the_flame"])],
    "LL-018f": [("test_sat_no_return_startup", "NoReturnStartup", ["test_flood_cs_uses_a_flow_sample_at_most_one_second_old_when_the_boiler_answers_pm25",
                "test_without_an_answer_to_pm25_the_flood_uses_the_thermostat_poll_age"])],
    # PIC transport: the SAT side (MI=500 only on a PIC, PM=25 only when no other PM= holds the single priority slot) and the PIC queue side
    "LL-018g": [("test_sat_no_return_ignition", "NoReturnIgnition", ["test_pm25_is_skipped_while_another_pm_is_pending", "test_mi500_once_on_pic_after_no_return_detection",
                "test_no_return_mi500_waits_5_minutes_the_quirk_does_not", "test_mi500_for_the_manufacturer_quirk_on_pic_only"]),
                ("test_pic_pm_chain", "PmChain", ["test_priority_slot_waits_for_the_boiler", "test_tp_and_pm_do_not_cancel_each_other", "test_priority_slot_frees_after_60_s",
                "test_user_pm_is_not_purged"])],
    # OT-Direct transport: no MI=500 from SAT, and the PM= branch plus the schedule that serves PM=25, on their own code (OTDirect.ino)
    "LL-018h": [("test_sat_no_return_ignition", "NoReturnIgnition", ["test_mi500_never_on_ot_direct"]),
                ("test_pic_pm_chain", "PmChain", ["test_not_armed_on_ot_direct"]),
                ("test_sat_no_return_startup", "OtDirectPm", ["test_pm25_goes_out_within_a_communication_interval_when_the_slow_poll_tier_is_idle",
                "test_pm25_latency_in_the_slow_poll_burst_is_bounded_but_not_one_interval", "test_without_pm25_the_msgid25_read_is_only_every_10_s_on_ot_direct",
                "test_pm_for_an_unscheduled_id_is_a_one_shot_read", "test_pm_out_of_range_is_refused_with_bv"])],
    # LL-019 (TASK-1412.9): the whole phase-anchor and start-count suites run, not a subset: every test of test_sat_pwm_anchors (anchors,
    # ignition wait, relight, DHW interruptions, per-source start counting, millis rollover) and test_sat_pwm_start_limit (limit windows,
    # PWM entry, range 5, flame lag, ring rollover), plus the P / 2P mapper, minimum ON / OFF and range-5 cells of test_sat_hp_pwm.
    "LL-019": [("test_sat_pwm_anchors", "SatPwmAnchors", None),
               ("test_sat_pwm_start_limit", "SatPwmStartLimit", None),
               ("test_sat_hp_pwm", "SatHeatPumpPwm", ["test_hp1800_mid_range_duty_060", "test_hp1800_low_range_duty_030", "test_hp1800_ultra_low_without_flame_008",
                "test_hp1800_over_max_092", "test_hp1800_ultra_low_with_flame", "test_hp1800_ultra_low_without_flame", "test_hp1800_over_max",
                "test_hp2400_ultra_low_with_flame_007", "test_hp2400_over_max", "test_hp10800_all_five_ranges", "test_hp10800_on_phase_ends_at_the_current_on_time",
                "test_hp3600_ranges", "test_hybrid_never_gets_3600", "test_hybrid_never_gets_10800", "test_hp1800_on_phase_ends_after_on_time",
                "test_hp1800_on_phase_holds_before_on_time", "test_hp_minimum_burn_600_holds", "test_hp_min_off_600_in_range5",
                "test_gas_range5_restarts_without_min_off", "test_gas_oracle_4ph", "test_gas_3ph_default", "test_gas_on_phase_min_on_180",
                "test_r3_on_start_without_flame_starts", "test_r3_on_phase_waits_for_flame", "test_hybrid_uses_the_heat_pump_cycle"])],
    "LL-020": [("test_sat_pid_cadence", "Cadence", ["test_a_integral_and_step_count_independent_of_the_control_tick", "test_b_millis_wrap_gives_the_same_steps_and_integral",
                "test_c_loop_stall_gives_one_step", "test_d_startup_step_at_the_first_pass", "test_f_no_room_gives_curve_only_never_nan",
                "test_j_room_back_gives_curve_only_until_the_next_step"]),
               ("test_sat_pid_semantics", "StartUp", None), ("test_sat_pid_semantics", "HeldDerivative", None),
               ("test_sat_pid_semantics", "Saturation", None), ("test_sat_pid_semantics", "Deadband", None),
               ("test_sat_pid_semantics", "KiZero", None), ("test_sat_pid_semantics", "Persistence", None),
               ("test_sat_previous_cs_clamp", "PreviousCsClamp", ["test_a_falling_request_holds_previous_cs", "test_b_rising_request_follows_min_allowed",
                "test_c2_equal_request_takes_the_falling_branch"]),
               ("test_sat_overshoot_sustain", "TickClockAndLoopOrder", None)],
    "LL-021": [("test_sat_cycle_classifier", "CycleClassifier", None),
               ("test_sat_classifier_parity", "TestUpstreamIdentity", None), ("test_sat_classifier_parity", "TestParity", None),
               ("test_sat_classifier_parity", "TestAc2Inventory", None), ("test_sat_classifier_parity", "TestRam", None)],
    "LL-023": [("test_sat_ground_truth_block", "SatGroundTruthBlock", None)],
    # TASK-1412.15: telemetry against independent timestamped frames. The TelemetryDecisions class of that module is not delegated
    # (its three decision checks are registered below as contract fixtures, one per decision, entries 266, 267 and 269).
    "LL-022": [("test_sat_telemetry_wire", "CycleMetricsVsFrames", None), ("test_sat_telemetry_wire", "BootGap", None),
               ("test_sat_telemetry_wire", "PwmSecondsVsWire", None), ("test_sat_telemetry_wire", "MmValue", None)],
}
_delegated = {}   # (module, class, requested names) -> {test name: (ok, detail)}; the names are part of the key (TASK-1412.9)


def delegated_results(module, cls, names):
    # TASK-1412.9: the requested names belong to the key. With (module, class) alone the first LL id to ask for a class fixed the subset that
    # ran, so a later id asking for other tests of the same class (LL-019 after LL-015 / LL-017) was reported green without running them.
    key = (module, cls, None if names is None else tuple(names))
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

    @ll("LL-003", "contract", "whole SATpid.ino via test_sat_pid_cadence.build_cpp; the oracle is pid.py _update_integral (pid.py:185-192) fed the logged step times",
        task="TASK-1412.10 AC1 (absolute oracle, not only tick-independence)")
    def test_ll003_integral_equals_pid_py_for_the_logged_steps(self):
        import math
        import test_sat_pid_semantics as sem
        exe = self.need(pid_exe())
        for tick in (10, 30, 60, 120):
            lines = run([exe, "cad", tick, 1000000]).strip().split("\n")
            last = pidc.parse(lines[-1])
            steps = [int(x) for x in last["log"].split(",") if x]
            self.assertEqual(len(steps), int(last["steps"]), "tick %d" % tick)
            self.assertEqual(steps[0], 0, "the start-up step is the first pass")
            self.assertTrue(all(b - a == 60 for a, b in zip(steps[1:], steps[2:])), "60 s between steps at tick %d: %s" % (tick, steps))
            pid = sem.PidPy()
            for t in steps:   # scripted room (test_sat_pid_cadence.roomAt, trace 0): inside the deadband, target 21, curve 40
                room = sem.f32(21.0 - (0.05 + 0.04 * math.sin(t / 97.0)))
                pid.update(room, sem.r3(21.0 - room), 1000000 / 1000.0 + t, 40.0)
            self.assertAlmostEqual(float(last["I"]), pid.integral, delta=2e-3, msg="I at tick %d against pid.py" % tick)

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

    @staticmethod
    def expected_exit_s(interval_ms, jitter, limit_s):
        """The elapsed-time contract (entries 86 Q44 A/Q46 B, 95 Q46b, 109): the exit comes on the first control tick whose tick time is
        at least limit_s after the first qualifying tick. Tick i = previous tick + interval + lateness (SKIP_MISSED_TICKS)."""
        t, first = 0, None
        for i in range(40):
            t += interval_ms + jitter[i % len(jitter)]
            if first is None:
                first = t
            elif t - first >= limit_s * 1000:
                return (t - first) / 1000.0
        return None

    @ll("LL-004", "contract", "satCycleCheckOvershootSwitch / satCycleCheckPwmExits (SATcycles.ino, via test_sat_overshoot_sustain.extract)",
        task="TASK-1420 (fixed; entries 86, 95, 109); TASK-1412.8 AC2: every supported tick (10 s to 300 s) for all three timers, exact against the contract")
    def test_ll004_underheat_and_saturation_exit_after_180_300_s_from_the_first_qualifying_sample(self):
        rows = []
        for interval in (10, 30, 60, 90, 120, 300):
            for kind, threshold, mode in (("o", 60, 2), ("u", 180, 1), ("z", 300, 1)):
                elapsed, t = self.exit_after(interval * 1000, kind)
                rows.append((interval, kind, threshold, elapsed))
                with self.subTest(interval_s=interval, condition=kind):
                    self.assertIsNotNone(elapsed, "no exit at all")
                    self.assertEqual(elapsed, self.expected_exit_s(interval * 1000, (0,), threshold))
                    self.assertGreaterEqual(elapsed, threshold)
                    self.assertLessEqual(elapsed, threshold + interval)
                    self.assertEqual(int(t["mode"]), mode)
        self.rec()["observed"] = rows

    # LL-010 -------------------------------------------------------------------------------------------------------------
    @ll("LL-010", "contract", "satNoteSlaveConfig, satPwmPermitted, satPwmEntryAllowed, satSetControlMode, satRestoreControlMode, satHandleControlMode, "
        "satCycleCheckOvershootSwitch, satCycleApplyEndOfCycle, satCycleCheckLowLoadVeto (test_sat_slave_lowload, test_sat_cycle_classifier; entries 213-215, ADR-157)",
        task="TASK-1412.4: HB4 = 1 and no valid reply block the overshoot switch, the end-of-cycle switch (TASK-1331), the restart restore, the pwm command and any other entry; "
             "HB4 turning 1 leaves PWM in that tick")
    def test_ll010_hb4_permission_is_consulted_before_low_load_cycling(self):
        self.delegated("LL-010")

    # LL-011 -------------------------------------------------------------------------------------------------------------
    @ll("LL-011", "contract", "satApplyPWM flow-sentinel path; no-return verdict and board-sensor return (test_sat_pwm_override, test_sat_no_return_ignition)",
        task="partial: 0.0 flow sentinel, Unknown-DataID verdict and board-sensor return only")
    def test_ll011_flow_sentinel_unknown_dataid_and_board_sensor_return(self):
        self.delegated("LL-011")

    @ll("LL-011", "contract", "satFlowFeedClass, satReturnFeedClass, satGetFlowTemp, satGetReturnTemp, satNoReturnTemp, the processOT frame hook (via test_sat_flow_return_validity)",
        task="TASK-1412.5 (fixed; entries 217, 218, 241): unsupported, never received, Data-Invalid, stale, fresh zero, valid; flow 60 s, return 5 min")
    def test_ll011b_per_id_age_and_invalid_state_exist(self):
        self.delegated("LL-011b")

    # LL-012 .. LL-015, LL-017 .. LL-021, LL-023 -------------------------------------------------------------------------
    @ll("LL-012", "contract", "satCycleCheck* DHW guard, satApplyPWM DHW pause/resume (SATcycles.ino, SATcontrol.ino)",
        task="TASK-1412.7: DHW tails and phase-owned timers; entry cells (startup, setpoint fall, valve/pump step, maximum limit, real saturation); "
             "CH enable vs CH mode; QD-8 comparison traces. Flow None: TASK-1412.5/1428 (LL-011b)")
    def test_ll012_dhw_transitions(self):
        self.delegated("LL-012")

    @ll("LL-012", "open_decision", "none: no one-minute post-hot-water setpoint hold exists (structural)", task="TASK-1412.7", question="QD-8")
    def test_ll012b_post_dhw_one_minute_setpoint_hold(self):
        sat = (FW / "SATcontrol.ino").read_text(encoding="utf-8")
        self.assertRegex(sat, r"(?i)post[- ]?(dhw|hot[- ]?water)[^\n]*hold[^\n]*(60|one minute)", "no post-DHW setpoint hold; QD-8 not asked yet")

    @ll("LL-013", "contract", "satEnqueueOwnCS/satSendCS/satCmdResend/satEnqueueIfChanged* (SATcontrol.ino, via test_sat_cmd_cache)",
        task="TASK-1574 (commit-on-success) and TASK-1412.11 (separate intent / enqueue / transmission / echo events, full queue, interrupted exchange, "
             "PIC reset, superseded commands, measured budgets; tests/test_sat_command_delivery.py)")
    def test_ll013_dropped_enqueue_is_not_cached_and_is_retried(self):
        self.delegated("LL-013")

    @ll("LL-013", "contract", "addCommandToQueue OT-Direct branch, handleOTDirectCommand CS=/MM= branches, otCmdEnqueue ring, SAT senders (via test_sat_command_delivery.OtDirectRefusal)",
        task="TASK-1593 (Q-1412.11a, Robert 2026-11-03): an OT-Direct write that the handler refused (command ring full) is answered Dropped and SAT does not cache it as delivered")
    def test_ll013b_ot_direct_refusal_is_visible_to_the_caller(self):
        self.delegated("LL-013b")

    @ll("LL-014", "contract", "satEffective MaxTSet / CS ceiling and SH negotiation (SATcontrol.ino, via test_sat_max_tset)",
        task="TASK-1412.13 (entries 100, 252): ID49 bounds with 20/80 fallback, SH= without an MsgID 6 gate, the SH= write verdict observed, every CS path under the ceiling")
    def test_ll014_every_cs_obeys_the_effective_ceiling(self):
        self.delegated("LL-014")

    @ll("LL-015", "contract", "satGetPwmPeriodSec, satGetMaxCyclesPerHour, satApplyPWM per source (via test_sat_hp_pwm, test_sat_pwm_start_limit)",
        task="partial: mapper and phase arithmetic; full-loop automatic entry per source is TASK-1412.7/9 (Q-1527a answered A, entry 227)")
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

    @ll("LL-018", "contract", "satNoReturnTemp, satReturnFresh, satReturnMissingLong, satGetFlowTemp on real frames (SATboilerFeed.h, the processOT feed hook), satNoReturnFlood*, satMiStep "
        "(via test_sat_no_return_startup)",
        task="TASK-1412.12 AC1 (entries 84, 135, 149, 151, 217, 241, 263): unsupported, Data-Invalid, stale and restored return; fresh zero; missing flow; the start-up window keeps the previous CS")
    def test_ll018c_unsupported_invalid_and_stale_return_through_the_real_validity(self):
        self.delegated("LL-018c")

    @ll("LL-018", "contract", "SAT switched on while the boiler runs: satMiStep, satNoReturnFloodOnFlameEdge, control mode gate (via test_sat_no_return_startup)",
        task="TASK-1412.12 AC1: runtime enable. The PM chain is NOT part of it (TASK-1384, LL-018 open decision below)")
    def test_ll018d_runtime_enable_with_a_running_boiler(self):
        self.delegated("LL-018d")

    @ll("LL-018", "contract", "hot boiler that does not light for 180 s: satPwmFlameOffHold clamp, flood armed by the flame edge only (via test_sat_no_return_startup)",
        task="TASK-1412.12 AC1: the boiler's own anti-cycle delay; the delay itself is a bench fact (TASK-1412.21)")
    def test_ll018e_hot_boiler_in_its_own_anti_cycle_lockout(self):
        self.delegated("LL-018e")

    @ll("LL-018", "contract", "age of the MsgID 25 sample behind each flood CS, with and without an answer to PM=25 (via test_sat_no_return_startup)",
        task="TASK-1412.12 AC2 (R39): modelled ages, OpenTherm v4.2 section 4.3.1 answer time; real ages are a bench measurement")
    def test_ll018f_flow_sample_age_behind_the_flood_cs(self):
        self.delegated("LL-018f")

    @ll("LL-018", "contract", "PIC path: addCommandToQueue PM=/MI=, pmChainBusy, the single priority slot (via test_sat_no_return_ignition, test_pic_pm_chain)",
        task="TASK-1412.12 AC3: PIC MI/PM, proven on its own; a command echo is not ignition")
    def test_ll018g_pic_transport_mi_and_pm(self):
        self.delegated("LL-018g")

    @ll("LL-018", "contract", "OT-Direct path: the PM= branch and scheduleMasterRequest() of OTDirect.ino, no SAT MI=500 (via test_sat_no_return_startup, test_pic_pm_chain)",
        task="TASK-1412.12 AC3: OT-Direct equivalents, proven on its own code; a replay is not ignition")
    def test_ll018h_ot_direct_transport_mi_and_pm(self):
        self.delegated("LL-018h")

    @ll("LL-018", "contract", "initSAT -> satBootPmChainWanted -> pmChainStart: the boot PM chain runs on a PIC board whatever SAT is, so MsgID 3 is known before a runtime SAT enable (via test_pic_pm_chain)",
        task="TASK-1384 (entry 261)")
    def test_ll018i_runtime_enable_sends_the_boot_pm_chain(self):
        self.delegated("LL-018i")

    @ll("LL-018", "open_decision", "none: a PM=25 on OT-Direct waits behind the entries already due in the schedule (a minute-aligned slow-poll burst delays it by seconds, not by one communication interval); no decision records what OT-Direct must do for PM=",
        task="TASK-1412.12 finding", question="OTD-PM25 (new, to be asked)")
    def test_ll018j_ot_direct_pm25_goes_out_within_one_communication_interval_in_the_full_schedule(self):
        import test_sat_no_return_startup as nrs
        cls = nrs.OtDirectPm
        cls.setUpClass()
        try:
            if not cls.exe:
                raise InfrastructureError("no host C++ compiler")
            lat = cls("test_pm_out_of_range_is_refused_with_bv").pm25_latencies(True)
        finally:
            cls.tearDownClass()
        self.assertLessEqual(max(lat), 1150, lat)

    @ll("LL-018", "contract", "satNoReturnTemp, satReturnFresh, satApplyPWM no-return start-up window (SATcontrol.ino, via test_sat_no_return_startup)",
        task="TASK-1412.12 (entries 217, 263; Conf-1371 answered by entry 263, confirmed by Sergeant D in entry 265): Data-Invalid / stale return counts as missing after 5 minutes; "
             "before any MsgID 28 answer of this boot the flame-off hold keeps the previous CS; a restored Unknown-DataID verdict alone does not count")
    def test_ll018b_stale_or_invalid_return_is_treated_as_missing(self):
        self.delegated("LL-018b")

    @ll("LL-019", "contract", "satApplyPWM phase anchors, ignition wait, start counting (via test_sat_pwm_anchors, test_sat_hp_pwm)",
        task="TASK-1380/1382/1383/1421 (fixed); TASK-1412.9 revalidation (matrix per source, limit windows, rollover)")
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
        for interval in (10, 30, 60, 120):
            for kind, threshold in (("o", 60), ("u", 180), ("z", 300)):
                for jitter in ((0, 7000, 3000, 5000), (0, 25000), (12000,)):
                    for start in (None, 4294967295 - 100000, 4294967295 - int(interval * 1500)):   # the last: the wrap falls inside the timer
                        elapsed, t = self.exit_after(interval * 1000, kind, jitter=jitter, start=start)
                        results.append((interval, kind, jitter, start, elapsed))
                        with self.subTest(interval_s=interval, kind=kind, jitter_ms=jitter, start=start):
                            self.assertIsNotNone(elapsed)
                            self.assertEqual(elapsed, self.expected_exit_s(interval * 1000, jitter, threshold),
                                             "the exit is not on the first tick at least %d s after the first qualifying tick" % threshold)
                            self.assertGreaterEqual(elapsed, threshold, "early exit")
                            # one tick later at most, where a tick may itself be late by the largest lateness
                            self.assertLessEqual(elapsed, threshold + interval + max(jitter) / 1000.0, "late exit")
        self.rec()["observed"] = results

    @ll("LL-021", "contract", "real pinned thermo-nova tracker/classifier and the real SATcycles.ino classifier on identical samples (via test_sat_cycle_classifier and test_sat_classifier_parity)",
        task="TASK-1412.14: verdict differences are catalogued, all are recorded deviations (entry 250); the gate test_sat_classifier_parity.TestParityGate fails when an unrecorded (class b) difference appears")
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

    @ll("LL-022", "contract", "satCycleOnFlameChange/satCycleSample/satGetWindow4hStats/satGetWindow24hStats/_cycleRecord/_hourCount*/_dailyRecord (SATcycles.ino), "
        "satApplyPWM + satChEnableDecide, satComputeMM and the published MM units, all against frame-derived timestamps (via test_sat_telemetry_wire)",
        task="TASK-1412.15: on/off seconds, off gaps, 4 h and 24 h duty, zero gap, DHW overlap, flicker, missing frame, window expiry; "
             "the PIC/OT-Direct frame decode (OTGW-Core.ino) and the control loop around the flame edge are not run")
    def test_ll022_telemetry_matches_independent_frames(self):
        self.delegated("LL-022")

    @ll("LL-022", "contract", "satCycleSample/satCycleOnFlameChange/satGetWindow4hStats (SATcycles.ino): a flame that already burns when SAT starts has an unknown first off gap (SAT_OFF_GAP_UNKNOWN), left out of the average off gap and the duty EMA (entry 266, thermo-nova tracker.py:144-145)",
        task="TASK-1412.15: Q-TW-1")
    def test_ll022_first_cycle_off_gap_of_a_flame_burning_at_boot_is_not_a_measurement(self):
        import test_sat_telemetry_wire as tw
        tw.check_first_cycle_gap_is_not_fabricated_when_the_flame_burns_at_boot()

    @ll("LL-022", "contract", "satApplyPWM (SATcontrol.ino): state.sat.bModSuppressed, published as mod_suppressed (SATcontrol.ino, restAPI.ino, handleDebug.ino), is true on the PWM ON-phase suppression path and false elsewhere (entry 267)",
        task="TASK-1412.15: Q-TW-2")
    def test_ll022_mod_suppressed_follows_the_real_suppression_state(self):
        import test_sat_telemetry_wire as tw
        tw.check_mod_suppressed_follows_the_suppression_state()

    @ll("LL-022", "contract", "satReportedModulation + the control tick (SATcontrol.ino): the published current_modulation is 100 while MM=T goes out at the maximum PWM duty (entry 269)",
        task="TASK-1412.15: Q-TW-4")
    def test_ll022_published_modulation_equals_the_mm_on_the_wire(self):
        import test_sat_telemetry_wire as tw
        tw.check_published_modulation_equals_the_mm_on_the_wire()


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
        "production_sources_sha256": {n: file_sha(FW / n) for n in ("SATcontrol.ino", "SATcycles.ino", "SATpid.ino", "SATchEnable.h", "SATboilerFeed.h", "SATtypes.h", "safeTimers.h", "OTDirect.ino", "OTGW-Core.ino")},
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
