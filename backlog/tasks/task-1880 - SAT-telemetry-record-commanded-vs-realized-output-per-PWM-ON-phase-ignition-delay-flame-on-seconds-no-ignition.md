---
id: TASK-1880
title: >-
  SAT telemetry: record commanded vs realized output per PWM ON phase (ignition
  delay, flame-on seconds, no-ignition)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-13T06:13:00Z'
updated_date: '2026-11-13T06:33:00Z'
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
- [ ] #1 Host test reproduces a PWM ON phase without ignition and shows ignited=false with the commanded ON time; red on the old code (field absent), green on the fix
- [ ] #2 REST /api/v2 and MQTT expose the per-phase record and counters; no control behaviour changes (existing SAT host tests unchanged and green)
- [ ] #3 Bench run by otgw-bench-tester on a PIC and an OT-Direct unit shows the record matching the flame bit in a capture
- [ ] #4 Web UI (U10): a PWM phase card in the Technical view shows commanded ON time, ignition delay and ignited yes/no for the last phases; webui test on a fixture payload
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
<!-- SECTION:NOTES:END -->
