---
id: TASK-1880
title: >-
  SAT telemetry: record commanded vs realized output per PWM ON phase (ignition
  delay, flame-on seconds, no-ignition)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-13T06:13:00Z'
updated_date: '2026-11-13T16:06:00Z'
labels:
  - sat
  - telemetry
  - low-load
dependencies:
  - TASK-1879
priority: medium
ordinal: 953000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Lesson 1 of docs/sat/vtherm-comparison-2026-11-13.md (TASK-1879). VTherm learns on realized duty e_eff (cycle_tick_logic.py:137-148). SAT's cycle classifier records per flame cycle, not per PWM ON phase. Add, telemetry only (no control change), per PWM ON phase: commanded ON time, ignition delay (ON start to flame-on), realized flame-on seconds, and an ignited yes/no flag; expose the last N phases and counters via REST/MQTT and the telnet log. Direct evidence for Q-P2 (saturation exit while the boiler did not ignite, gruijter 2026-11-12). No String class (ADR-180), PROGMEM literals, flat MQTT topics (ADR-101).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test reproduces a PWM ON phase without ignition and shows ignited=false with the commanded ON time; red on the old code (field absent), green on the fix
- [x] #2 REST /api/v2 and MQTT expose the per-phase record and counters; no control behaviour changes (existing SAT host tests unchanged and green)
- [ ] #3 Bench run by otgw-bench-tester on a PIC and an OT-Direct unit shows the record matching the flame bit in a capture
- [x] #4 Web UI (U10): a PWM phase card in the Technical view shows commanded ON time, ignition delay and ignited yes/no for the last phases; webui test on a fixture payload
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Telemetry only: no control path reads the new data (AC2 guard).
1. Record (SATcycles.ino): a ring of 8 SatPwmPhaseRecord {startEpoch, cmdOnS (on time at the ON start), finalOnS (on time at the ON->OFF edge), ignitionDelayS (ON start to first CH flame; 0xFFFF = none), flameOnS (CH flame seconds inside the phase, hot-water flame excluded), ignited, endReason (off_edge | mode_exit | heating_off | dhw | reset)}. Open in satPwmBeginOn() (SATcontrol.ino:1235), close in satPwmEndOn() (:1270) and in satPwmReset() (:1694) when a phase is open; flame time sampled on the control tick plus the flame-edge hook satCycleOnFlame. Counters since boot: phases, ignited, not_ignited, ignition-delay sum/max.
2. Telnet: one SATDebug line per closed phase (PSTR, no String).
3. REST: /api/v2/sat/status?detail=full gets pwm_phases[] (last 8) and pwm_phase_counters via JsonEmit (ADR-146, no ArduinoJson).
4. MQTT flat topics (ADR-101): sat/pwm_last_cmd_on_s, sat/pwm_last_ignition_delay_s, sat/pwm_last_flame_on_s, sat/pwm_last_ignited, sat/pwm_phases_not_ignited; published on phase close; HA discovery for these sensors.
5. Web UI U10: PWM phase card in SAT > Technical (v2.js), last phases table from detail=full; English source keys plus nl; webui test on a fixture payload.
6. Tests: host test cutting the real satPwmBeginOn/EndOn/schedule with stubbed flame; a 300 s ON phase without flame then saturation exit gives ignited=false, cmdOnS kept, endReason=mode_exit (red: no record on old code). Mutation: a version that marks ignited on CS>10 instead of the flame bit must fail. Existing SAT host tests unchanged and green; evaluate --quick.
7. Bench AC3 via 09: PIC (.63) and OT-Direct (.61), compare records with the flame bit in a capture.
Open-source check: none needed (project-specific glue, a few dozen lines).
Order: code after the 220 post; build slot after 1885 (ba).
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-13: UI follow-up U10 from the VTherm UI review added as an AC.

2026-11-13 d3, firmware part on lane-1880 (uncommitted, bump waits for 220/1884/1871/1885 on origin): ring of 8 SatPwmPhaseRec + counters in SATcontrol.ino (TASK-1880 block before satPwmNarrateEdge), opened in satPwmBeginOn, closed in satPwmEndOn (off) / satPwmReset (cut), lit time sampled every loop pass on flame bit && !DHW bit; SATPWMPHASE telnet line; REST /api/v2/sat/status pwm_phases[] + counters (openapi); MQTT sat/pwm_last_cmd_on_s, pwm_last_ignition_delay_s (-1 never lit), pwm_last_flame_on_s, pwm_last_ignited, pwm_phases_not_ignited with HA discovery (diagnostic; MQTT_HA_SENSOR_COUNT 399->404, LUT regenerated); MQTT.md. Deviation from plan: REST fields in the base status, not only detail=full.

Evidence: tests/test_sat_pwm_phase_telemetry.py 6/6 OK; old code red (block absent, ValueError at build_source); mutants caught: ignited-on-open (3 fail), flame-always (2 fail), no close sample (1 fail); CS-instead-of-flame-bit call-site mutant fails the pin. Full tests folder run sequentially in the foreground in chunks (a run_tests.py pass hit 590 s and a background run was reaped): every test OK incl. test_sat_lowload_contract (35 tests, 522 s), only change to existing tests is a no-op satPwmPhaseSample stub in test_sat_ot_loss.py and test_sat_restart_quiet.py (they cut the loop). evaluate --quick: 86 passed, 0 failed. Open: AC4 web UI card (separate commit), AC3 bench.

AC4 (U10) on lane-1880, uncommitted: card 'PWM ON phases: asked vs done' in SAT > Technical (v2.html, satRenderPwmPhases in v2.js, design-system table.stats and kv-row only), strings in EN + nl/de/el packs (i18n-lang completeness gate green). tests/webui/sat-pwm-phases.test.mjs on a fixture payload: 5/5 pass, 0/5 on the old v2.js/v2.html. All 72 webui files run one by one: 3 fail on the base commit too (pic-flash-status picAfterFlash, graph-room-dropout, home-dial-sat-only; checked in a detached base worktree), not caused by this task; every other file passes when run alone.

2026-11-13 shipped in 2.0.0-beta.224 (5ddcc6543, post 1570832038179905578). Bench by 09 on .61: SAT simulation only; 1 PWM ON phase recorded ignited, ended cut, consistent with the flame bit; REST, MQTT and HA entities present. NOT bench-tested: a phase that never lit, a phase that ends 'off', a PIC unit (.63) and a real boiler with a capture to compare. The never-lit and 'off' paths are covered only by the host test (tests/test_sat_pwm_phase_telemetry.py). AC3 stays open for that.

09's question: cmd_on_s 3600 on a forced PWM entry. Intended. A PWM entry at a duty at or above the maximum is duty range 5 (continuous ON): satPwmBeginOn(flame, boilerTemp, maxMs, 0) with maxMs = 2 x period (SATcontrol.ino range 5, thermo-nova pwm.py:297 on_time_maximum_threshold = 2 x on_time_upper_threshold, pwm.py:55). With a 1800 s period (heat pump / hybrid default) that is 3600 s; with a gas boiler at 3 per hour it would be 2400 s. The record stores the ON time SAT asked at the start, so 3600 is the commanded value, not a bug.
<!-- SECTION:NOTES:END -->
