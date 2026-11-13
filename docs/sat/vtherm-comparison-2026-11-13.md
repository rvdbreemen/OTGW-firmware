# Versatile Thermostat compared with SAT: what we can learn (2026-11-13)

Status: internal analysis, not yet second-read. No claimed difference in this file is published
(Discord, GitHub) before a second reader has opened both citations and tried to refute it
(`.claude/rules/sat-ground-truth.md`).

## Sources

- **Versatile Thermostat (VTherm)**: https://github.com/jmcollin78/versatile_thermostat, `main` at
  commit `a45ec370cf4aaac020f991a323733d6bceaec626` (2026-11-12), MIT licence. Paths are relative to
  `custom_components/versatile_thermostat/`.
- **thermo-nova**: the pinned commit `7beaaf531ee99dc74a19ac74f3c06ce7f2922cfb`. Paths are relative to
  `custom_components/sat/`.
- **Firmware**: `src/OTGW-firmware/` on `dev`. Corrections entries are in
  [`sergeant-d-corrections.md`](sergeant-d-corrections.md).

Every citation below was opened during the session that wrote this file. Two readers read the code,
and the main session opened the core citations again: `pid.py:186-189`, `pwm.py:150-158`,
`area.py:285-301`, `prop_algo_tpi.py:98-101, 196-199`, `feature_central_boiler_manager.py:511-530`,
`const.py:540-544` and `auto_tpi_manager.py:1545-1581`.

## Conclusion

VTherm and SAT solve different problems.

- **VTherm** controls a room actuator (relay, TRV, valve) with time-proportional on/off cycles. On
  the boiler it is only an on/off switch.
- **SAT** controls the flow temperature of one modulating boiler over OpenTherm.

The VTherm control algorithm (TPI) and its central-boiler logic do not transfer to an OpenTherm
boiler. What does transfer is the engineering around the control loop:

- learning only on samples it accepts, and naming why it rejects the others;
- accounting for the power that was actually delivered as well as the power that was commanded;
- a heating-failure watchdog;
- offline identification from logs.

One control point is a question for Sergeant D: the integral (lesson 6).

## Side by side

| Aspect | VTherm | SAT (thermo-nova / port) |
|---|---|---|
| Controlled quantity | On-fraction per room: `coef_int·(target−Tin) + coef_ext·(target−Tout)`, clamped 0..1, 5 min cycle (prop_algo_tpi.py:100-101, 124-132; config_schema.py:147) | Flow setpoint = heating curve + P + I + D (pid.py:127-132) |
| Integral | PI self-regulation: Σe clamped; with overheat protection on, divided by 2·max(Δt, 0.5) on a sign change (halved at Δt = 1); not reset on a setpoint change (pi_algorithm.py:49-55, 98-107) | Integrated only within ±0.1 °C and reset to 0 outside it (pid.py:186-189, const.py:16) |
| Learning | Auto-TPI learns Kint, Kext and capacity once per cycle, with many guards (auto_tpi_manager.py:749-811, 857-866, 1458-1501) | thermo-nova: none (entry 298). Firmware: heating-curve self-tune (ADR-205/228, `SATcurveTune.h`) |
| Low load | Minimum on/off times with a penalty and an effective duty `e_eff` (cycle_tick_logic.py:64-148) | PWM phase machine and modulation suppression (pwm.py:117-301, heating_control.py:380-448) |
| Missing sensor | `on_percent` is `None` and the actuator keeps its state (prop_algo_tpi.py:198-199, prop_handler_tpi.py:369-378). Safety after 60 min uses a fallback of 10 % (feature_safety_manager.py:108-132, const.py:529-530) | thermo-nova: stale after 6 h (const.py:170), and nothing is sent without an outside temperature (climate.py:544-545). Firmware: stale after 5 min (SATcontrol.ino:146-147), then curve only with a reset PID (entries 38, 46, 115) |
| Open window | Slope based: below −3 °C/h, and only while heating (open_window_algorithm.py:133, 143; feature_window_manager.py:339-350) | Contact sensor only (climate.py:800-834) |
| Failure | Heating failure when on ≥ 90 % for 15 min and the room rises < 0.5 °C. It is an event only and blocks learning (const.py:540-544; feature_heating_failure_detection_manager.py:224-264) | Not found in thermo-nova. Not found in the firmware by a grep, which is not proof |
| Boiler | ON if the number of active devices or the total power reaches a threshold, with no hysteresis (feature_central_boiler_manager.py:511-530) | Modulated setpoint, area aggregation P75 + 5 (area.py:293-301) |
| Tests | 838 test functions, including a closed-loop `ThermalModel` that checks Auto-TPI converges (tests/test_auto_tpi_simulation.py:58-98) | Host tests, a closed-loop low-load simulation (tests/test_sat_closedloop_sim_1412_19.py) and tuner unit tests (tests/test_sat_curve_selftune.py) |

## Lessons

### A. No behaviour change (Sergeant D's OK not needed)

**1. Commanded and realized output per PWM period.**

- *VTherm:* it computes and learns on the duty the actuator actually delivered, `e_eff`
  (cycle_tick_logic.py:137-148; cycle_scheduler.py:934).
- *Firmware:* the per-cycle classifier records duration, maximum flow and overshoot
  (SATcycles.ino:1120-1150), and TASK-1407 added the PWM ON/OFF times to the log. We did not find
  a record per PWM ON phase that holds the commanded ON time, the ignition delay, the realized
  flame-on seconds and whether the boiler ignited at all.
- *Why it matters:* this is direct evidence for Q-P2 (the saturation exit while the boiler did not
  ignite, gruijter 2026-11-12).

**2. Tuner exclusion reasons as counters.**

- *VTherm:* every rejected learning sample has a named guard (auto_tpi_manager.py:749-811).
- *Firmware:* already does this per hour. `SatTuneReason` (SATcurveTune.h:37-40) is logged as
  `SATTUNEH ... reason=` (SATcontrol.ino:6424-6428), but only the first reason of the hour, and only
  over telnet.
- *Gap:* small. Counters per reason in REST/MQTT would make a long-run capture readable without a
  telnet transcript.

**3. Heating-failure and no-ignition diagnostic event.**

- *VTherm pattern:* high demand for N minutes without a temperature rise raises an event, with no
  control action (feature_heating_failure_detection_manager.py:224-264).
- *SAT counterparts:*
  - (a) a high requested setpoint for N minutes while the room does not rise;
  - (b) a PWM ON phase, or a requested CS above the cold cutoff, with no flame within X.
- *Scope:* event and telemetry only, plus marking those tuner hours as not counted.

**4. Offline identification from captures.**

- *VTherm:* fits `ΔT = g·(Tout−Tin) + B·u + q` per cycle bin from Recorder data
  (tpi_identification.py:98-155). It requires:
  - a 70/30 chronological split;
  - the physical guards 0 < g < 1 and B > 0;
  - a condition number below 1000;
  - at least 40 cycles (tpi_identification.py:185-198).

  It reports a block-bootstrap uncertainty (tpi_identification.py:19, 597), labels every result
  "exploratory" and never writes a value on its own (tpi_identification.py:630-650).
- *For us:* estimate heat loss and the heating-curve coefficient from 24 h captures in the
  `analyze-capture` pipeline. The coefficient debate (entries 298-312: 1.12 against 1.8) then
  becomes a measurement.
- *Before building:* compare existing open-source system-identification libraries, as the
  "reuse open source" directive requires.

**5. Tuner convergence against a plant model.**

- *Firmware:* has a closed-loop low-load simulation (TASK-1412.19) and plant-model studies
  (TASK-1412.16). The tuner tests check the decision rules on synthetic hours
  (tests/test_sat_curve_selftune.py:308-429).
- *Gap:* a closed-loop test that runs the tuner for weeks against a house model with a known correct
  coefficient and checks that it converges and does not oscillate, the counterpart of
  tests/test_auto_tpi_simulation.py:58-98. Only `tests/test_sat_sim_boiler_model.py` mentions the
  tuner, and we have not checked whether it covers this.

### B. Behaviour questions (Sergeant D, one question per message)

**6. The integral.**

- *The problem:* thermo-nova discards the integral as soon as |error| > 0.1 °C (pid.py:186-189).
  The heating curve must then carry the whole steady state, so a wrong coefficient shows up as a
  lasting offset or oscillation. Remediation R13 points at the same thing.
- *Relation to entry 324:* the room oscillation is his main concern.
- *VTherm:* keeps Σe, clamps it and, with overheat protection on, shrinks it on a sign change (pi_algorithm.py:98-107).
- *Options to put to him:*
  - (A) keep thermo-nova, as today;
  - (B) shrink (for example halve) instead of reset on leaving the band;
  - (C) a bounded leaky integral.
- *Timing:* ask only after the evening analysis that entry 324 waits for, so we do not steer on a
  hunch. Queued as Q-VT1 in [`open-questions.md`](open-questions.md).

**7. Slope-based open-window detection.** Not proposed. With one reference room, PWM cycles produce
room drops of their own, so false positives are likely. YAGNI unless testers ask for it.

### C. Not to copy

- **TPI on/off on the boiler, and the central-boiler OR without hysteresis.** On a modulating boiler
  this gives the short cycling SAT exists to prevent.
- **Load shedding (feature_central_power_manager.py:186-250).** Not relevant for gas. For heat pumps
  it would be a separate task if ever asked.

### Aside

The thermo-nova hourly start cap compares the number of starts with `duty_cycle_seconds` = 780
(pwm.py:156, const.py:167), so it never binds. This was already recorded in entry 123 and Q36.
VTherm's accounting of minimum on/off times with a penalty is the more robust pattern.

## Follow-up

Produced under TASK-1879.

| Lesson | Task / queue entry |
|---|---|
| 1. Commanded vs realized per PWM ON phase | TASK-1880 |
| 2. Tuner exclusion counters | TASK-1881 |
| 3. Heating-failure / no-ignition event | TASK-1882 (needs-maintainer-ok) |
| 4 + 5. Offline identification, tuner convergence test | TASK-1883 |
| 6. Integral outside the deadband | Q-VT1 in [`open-questions.md`](open-questions.md) (queued, after the entry 324 evening analysis) |
