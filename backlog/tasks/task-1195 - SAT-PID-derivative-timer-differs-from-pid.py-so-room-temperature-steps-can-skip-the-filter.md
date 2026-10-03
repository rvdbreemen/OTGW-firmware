---
id: TASK-1195
title: >-
  SAT PID derivative timer differs from pid.py, so room-temperature steps can
  skip the filter
status: Done
assignee:
  - '@claude'
created_date: '2026-10-02 19:48'
updated_date: '2026-10-03 09:28'
labels:
  - sat
  - pid
  - bug
dependencies: []
priority: medium
ordinal: 317000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D asked in #alpha-testing (2026-10-02) whether SAT's derivative smoothing filter is also in the firmware. The filter math in SATpid.ino `_pidUpdateDerivative()` matches pid.py `_update_derivative()`: deadband freeze, 60 s minimum interval, ±5 cap before and after filtering, alpha = dt/(60+dt). The timer does not match.

pid.py:
- takes dt from the sensor's own `last_changed` (area.py:128, climate.py:314);
- runs the no-change branch before the 60 s check;
- so dt is the interval between two room-temperature changes.

Firmware:
- takes dt from millis() at each PID update and checks the 60 s first;
- satPidUpdate() runs at least every 60 s while the error is unchanged (control interval 30 s by default);
- each idle update hits the no-change branch and moves the timer to now;
- a step first seen less than 60 s after that returns early;
- satPidUpdate() then stores the new temperature as `_pid_lastRoomTemp`, so that step never reaches the filter;
- steps that do get through use dt of about 60 s instead of the real interval.

Found by reading the code (SATpid.ino:161-211 and 215-275, pid.py:199-232). Not reproduced yet.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A host harness calls the real satPidUpdate() with a stepped room temperature (0.1 °C every 5 min, 30 s control interval) and reports, on the current code, how many steps update the filtered derivative and with which dt
- [x] #2 After the fix the same harness shows every step that comes more than 60 s after the previous one reaching the filter, with dt within one control interval of the interval between the two changes (the firmware sees a change at its next control tick)
- [x] #3 Harness cases keep the pid.py behaviour for the deadband freeze, the ±5 cap and alpha = dt/(60+dt)
- [x] #4 esp32-combo build and evaluate.py green. On the OTGW32 bench, with the external room temperature raised by 0.1 °C every 150 s through POST /api/v2/sat/externaltemp, the mean raw_derivative over steps 9-16 stays below 60% of the true slope on the old build and reaches at least 85% on the fix, as the harness predicts (OLD 25-49%, FIX 92-98%)
- [x] #5 An error of exactly 0.1 (target 21.0, room 20.9) counts as inside the deadband, because pid.py rounds TemperatureState.error to 3 decimals: the primary integral keeps growing, the primary derivative freezes, and the zone integral keeps growing. Harness B1, B1d and B2 fail on the old code and pass on the fix
- [x] #6 Through the TASK-894 room EMA (the device path), the mean rawDerivative of a 0.1 °C per 5 min heat-up is within 10% of pid.py on the raw sensor, and a sensor that changes every 30 s gives the true slope. Harness E1 and R1 fail on the old code and pass on the fix
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1. SATpid.ino: add a sensor-side change stamp, _pid_roomTempChangedMs plus _pid_roomTempSeen. It is set at the top of satPidUpdate(), before any early return, when the room temperature differs by 0.001 or more from the last value seen. It plays the role of HA's last_changed in pid.py. satPidReset() leaves it alone, the same way pid.py reset() keeps its timers.
2. Init block: _pid_lastDerivativeMs = _pid_roomTempChangedMs, so after a reset dt still spans the real interval.
3. _pidUpdateDerivative(): use the stamp instead of millis(), in the pid.py order: deadband freeze, then no change, then the 60 s check. The freeze, cap and alpha lines stay textually the same, so the mutants MF, MC and MA still apply.
4. AC#2 sharpened: dt within one control interval of the real interval. The firmware sees a change at its next control tick; pid.py has HA's exact time.
5. Proof: test_sat_pid_derivative.py --old-rev HEAD. FIX passes every check. OLD keeps the rules (C1, C2, C3, C5a, C6, C7, H1) and shows the defect. Each mutant fails its own case.
6. Same commit: bump to alpha.403, build.bat esp32-combo, evaluate.py --quick, and update c4-code-sat.md if it describes the timer. AC#4 on the bench: app-only flash of the OTGW32, then a SAT simulation heat-up.
7. P1 (the float compare at error 0.1): separate task, not in this fix.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
AC#1 REPRODUCED 2026-10-03 on the current code (SATpid.ino at HEAD febf7f4b4, alpha.402).
Harness: test/host/test_sat_pid_derivative.{cpp,py}. It compiles the real SATpid.ino (whole file), satGetEffectiveHeatingSystem() (SATcontrol.ino) and the real SATtypes.h (defaults fDeadband 0.1, iControlInterval 30, bAutoGains 1). A pid.py transcription runs next to it as a reference oracle, not as code under test: it gets the sensor's last_changed (climate.py:314) and is called every 60 s, like control_pid (climate.py:713).
Command: python test/host/test_sat_pid_derivative.py --report worktree
Scenario: 40 steps of +0.1 C every 300 s, step phase swept 0..29 s, loop latency off and on (60 runs, 39 judged steps each).
- Steps that reached the filter: min 9, max 26 of 39.
- dt the filter used: 60.0 .. 150.0 s; the room changed every 300 s.
- Final rawDerivative: firmware -0.000833 .. -0.001650; pid.py -0.000333 in every run, so the firmware is 2.5x to 5x larger.
- D term, both with Kd 11760 (coeff 1.5, curve 40, radiators; pid.py kd = 0.07*8400*kp, the same formula): firmware -9.8 .. -19.4 C, pid.py -3.9 C.
- C4: the first step out of the deadband ran with dt 60 s, pid.py 300 s.
- C6b: the first step after satPidReset() was swallowed.
- C5b: with the target moving every tick, 10..22 of 39 steps reached the filter.
The pid.py rules still hold on the current code: C1 (deadband freeze), C2/C2b (cap), C3 (alpha), C5a (a target change does not move D), C6 (no spike after reset), C7, and H1 (dt recovered from the filter output, exact on 90 s and 110 s).
Side finding P1 (report only, out of scope here): at error 21.0f-20.9f = 0.1000004 the firmware treats the error as OUTSIDE the deadband: the integral reset to 0. pid.py rounds the error to 3 decimals (TemperatureState.error) and keeps it inside.

EVIDENCE on the committed revisions, 2026-10-03. Command: python test/host/test_sat_pid_derivative.py --old-rev febf7f4b4 --rev HEAD (HEAD = edb5c5cfe, alpha.403). Exit 0.
OLD (febf7f4b4) fails the 11 defect checks:
- A2_reach: 9..26 of 39 steps reached the filter.
- A2_dt: dt 60..150 s.
- A2_oracle: 2.5-5x pid.py.
- C4: deadband exit with dt 60 s against 300 s.
- C5b, and C6b (the step after a reset was swallowed).
- R1: -0.000155 against the true slope -0.000333.
- E1: device path through the TASK-894 EMA, -0.000064..-0.000127 against pid.py -0.000333, i.e. 20-38%.
- B1, B1d and B2: an error of 0.1 counted as outside the deadband, so the integral reset to 0.
OLD keeps the pid.py rules: C1, C2, C2b, C3, C5a, C6, C7 and H1 pass.
FIX (HEAD) passes every check:
- 39 of 39 steps reach the filter with dt 300.0..300.1 s.
- Final raw -0.000333, the same as pid.py.
- E1 -0.000316..-0.000334, i.e. 95-100%.
- R1 -0.000333, the true slope.
- B1, B1d and B2: inside the deadband.
Mutants, each failing its own case: MF (freeze removed) fails C1, MC (raw cap removed) fails C2, MA (alpha 0.5) fails C3, MB (reference = last temperature seen, the timer-only variant) fails E1, MR (no rounding) fails B1, and MRz fails B2.
Scope note: after the first report I found that satControlLoop() smooths the room temperature with the TASK-894 EMA (tau 45 s) before satPidUpdate(), and that SAT Python feeds pid.py the raw HA entity (climate.py:271-279). The raw-step numbers from my first Discord message were therefore wrong for the device. On the device path the D term was too weak, not too strong. A correction was posted in #alpha-testing (msg 1555839738437570631). With the user's go, B' was added: the derivative reference temperature moves only together with the timer.
Residuals:
- pid.py runs every 60 s, the firmware PID every control tick.
- A change is stamped when the control loop reaches the PID; DHW, closed valves and summer simmer return earlier. With B' the next update then measures the average slope over that gap, so no step is lost.
- pid.py itself comes out low on fast sensors (R1: 40% of the true slope).
- Side finding, own task: the zone PID Kp divisor is inverted (iHeatingSystem == 1 ? 4 : 3), where the primary PID uses UNDERFLOOR ? 4 : 3.

AC#4 OLD SIDE ON THE BENCH, 2026-10-03, OTGW32 at 192.168.88.61 running 2.0.0-alpha.402+febf7f4. That build is HEAD's SATpid.ino and SATcontrol.ino, so it is the old code; it also carries the TASK-1162 CHUNK_INFLIGHT=0 diagnostic flag.
Setup: satexternaltemp=true, SAT enabled (control mode continuous), no boiler on the bench bus.
Run: scratchpad sat_bench_1195.py fed the external room temperature 18.00 +0.1 C every 150 s through POST /api/v2/sat/externaltemp/<v> (16 steps) and polled GET /api/v2/sat/status every 15 s.
Result: 171 polls, 0 poll errors, 17 feeds with 0 errors, never tripped, always enabled.
- Mean raw_derivative over the polls of steps 9-16 (81 polls): -0.000262.
- True slope: -0.000667. Ratio 0.393 (39%). This is inside the harness prediction for OLD (25-49%) and below the AC#4 limit of 60%.
- Per step, raw moved between -0.000140 and -0.000465.

AC#4 FIX SIDE ON THE BENCH, 2026-10-03, OTGW32 192.168.88.61 running 2.0.0-alpha.404+1d9ae71, flashed app-only. alpha.404 contains the TASK-1195 commit edb5c5cfe. The only other source change since then, 1d9ae71ae, is in satZonePidStep(), which runs only with satzonecount > 1, and the bench has 1.
Setup and run identical to the OLD side. Start: error 2.0 after the first full PID update; the 0.4 shown before it was the restored PID state from the OLD run.
Result: 171 polls, 0 poll errors, never tripped, always enabled.
- Mean raw_derivative over steps 9-16 (81 polls): -0.000676, ratio 1.014 (101%) of the true slope -0.000667. AC#4 limit is at least 85%; harness prediction 92-98%.
- OLD on the same scenario: -0.000262 (39%).
- Per-step means, steps 2-16: FIX -0.000525..-0.000724, OLD -0.000150..-0.000370.
The bench run is reproducible with scripts/tests/sat_derivative_bench.py. Bench settings were restored afterwards (satenabled false, satexternaltemp false).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The SAT PID derivative now behaves like pid.py (commit edb5c5cfe, alpha.403, pushed to origin/dev).
- It is timed by when the room temperature changed (_pid_roomTempChangedMs, stamped in satPidUpdate() before any early return), not by when the PID ran.
- It runs in pid.py's order: deadband, no change, then the 60 s check.
- Its reference temperature (_pid_derivRefTemp) moves only together with its timer (B', the maintainer's choice). The always-on TASK-894 room EMA spreads every sensor step over about 8 control ticks, and pid.py's literal last_temperature overwrite dropped most of each step.
- The PID error is rounded to 3 decimals like pid.py's TemperatureState.error (satPidError(), primary and zone PID), so an error of exactly 0.1 stays inside the deadband.

Behaviour change for testers, alpha.402 against alpha.403: on the device path the derivative went from 20-38% to 95-100% of pid.py (host), and from 39% to 101% of the true slope (bench). At an error of 0.1 the integral keeps working and the derivative freezes.

Evidence per AC:
- AC#1: python test/host/test_sat_pid_derivative.py --report worktree on the old code. 9-26 of 39 steps reached the filter, with dt 60-150 s against 300 s.
- AC#2, AC#3, AC#5, AC#6: python test/host/test_sat_pid_derivative.py --old-rev febf7f4b4 --rev HEAD, exit 0.
  - OLD fails the 11 defect checks (A2_*, C4, C5b, C6b, E1, R1, B1, B1d, B2) and keeps the pid.py rules (C1, C2, C2b, C3, C5a, C6, C7, H1).
  - FIX passes everything: 39 of 39 steps, dt 300.0-300.1 s, E1 95-100%, R1 the true slope.
  - Mutants: MF (freeze) fails C1, MC (cap) fails C2, MA (alpha) fails C3, MB (reference = last seen, the timer-only variant) fails E1, MR fails B1 and MRz fails B2.
- AC#4:
  - build.bat --target esp32-combo for alpha.403: 'Successfully created ESP32S3 image', firmware and filesystem SUCCESS, fresh +edb5c5c artifacts. python evaluate.py --quick: 71/0/0.
  - Bench on the OTGW32 with scripts/tests/sat_derivative_bench.py (external room temperature +0.1 C every 150 s): OLD alpha.402 at 39% of the true slope, FIX alpha.404 at 101%.

Sergeant D answered in #alpha-testing. The first message overstated the device effect; it was corrected (msg 1555839738437570631) once the EMA path was measured.
Side findings filed: TASK-1197 (zone Kp divisor, Done) and TASK-1200 (runaway adr-lint).
Residuals:
- pid.py runs every 60 s, the firmware PID every control tick.
- A room change is stamped when the control loop reaches the PID (DHW, closed valves and summer simmer return earlier). With B' the next update then takes the average slope over the gap.
- pid.py itself reads fast sensors low (R1: 40% of the true slope); upstream was told.
<!-- SECTION:FINAL_SUMMARY:END -->
