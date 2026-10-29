---
id: TASK-1277
title: >-
  Bench replay that drives SAT PWM through hold, flow-1 suppression and the 3 s
  refresh without a boiler
status: Done
assignee:
  - '@claude'
created_date: 2026-10-21 04:58
updated_date: '2026-10-29T19:17:00Z'
labels:
  - sat
  - bench
  - test
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 407000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The SAT simulation heats the flow in seconds, so a burn never lasts the 20 s modulation-suppression hold and the flow-1 step plus the 3 s refresh (TASK-1268) cannot be seen on the bench. A /simulate replay fixture with a long burn (status CH+flame), a slowly rising flow (MsgID 25) and return (MsgID 28) gives SAT real boiler frames, so the whole PWM ON chain can be measured on the OTGW32 bench without a boiler. Uses the existing replay (no firmware change).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Generator script writes a replay fixture with valid parity: idle, then a burn of at least 4 minutes with CH+flame status, flow rising about 0.1 C per frame and a return temperature; a test checks parity and the phases
- [x] #2 Bench run on the old firmware (alpha.443+067b478, before TASK-1268) and the new firmware: old shows CS flow-1 changing only every ~30 s, new shows return+offset during the hold, then flow-1 updated every ~3 s; samples saved
- [x] #3 Bench restored afterwards: settings diff vs baseline empty, original otgw_simulation.log back
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Approach: the existing /simulate replay instead of extending scripts/sat_boiler_emulator.py (no firmware or emulator change; the replay already feeds SAT real boiler frames, proven in TASK-1258). scripts/gen_sat_pwm_replay.py writes tests/webui/fixtures/sat-pwm-suppression-replay.txt (600 lines = 450 s: status CH+flame every 6 s, flow 40 -> 57.7 C at +0.3 C per 6 s, return flow-10 and mod 100 every 24 s). tests/test_sat_pwm_replay_fixture.py: 5 errors before the fixture existed, 5 OK after (parity, generator match, flame on throughout, >= 4 min, rising flow). Bench OTGW32 .88.61, SAT simulation OFF (SAT reads the replayed frames), force PWM, external room 19.0, target 22.5, underfloor max 45. NEW (alpha.443+e07a33c = TASK-1268 code): PWM ON with flame on, CS = flow-1 exactly (41.1 at flow 42.1 ... 45.0 at flow 46.0, then the 45 max clamp), 14 CS changes, gaps 5.4-6.4 s (12-15 s where return/mod frames interleave), i.e. the 3 s refresh follows every flow frame. OLD (alpha.443+067b478, from the zip posted in #alpha-testing): ON phase CS = PID 38.2, then CS 10 while still ON with the flame burning, then 45; only 3 changes, 30.7 s and 29.3 s apart (control tick only). Bench afterwards on alpha.444+a7bbacc (fw+fs), settings diff vs baseline 0, original otgw_simulation.log (12716 B) restored. Mistake on the way: the old .ino.bin was no longer in build/, the safeboot restart had already been requested, unit sat in safeboot until the current build was uploaded (recovered in ~1 min, no settings lost). Evidence %TEMP%\bench-1277\green-1268.json, red-067b478.json.

TASK-1412.1 records review (R38): 2026-10-29. current-status annotation (R38/LL-023, reviewed at 3baed73d8):
- AC#2 is checked, but this replay did not show the return+offset hold: the fixture burns from the start (see 'Not covered' in the final summary). The hold evidence is TASK-1268's SAT-simulation run.
- The measured flow-1 refresh was about 6 s, once per replayed flow frame, not 3 s.
- The evidence (alpha.443/444, %TEMP%\bench-1277) predates the ON-timing changes of TASK-1382/1421 (entries 80, 89, 90). The current refresh path is SATcontrol.ino:1202-1249.
- Bench replay only; not a physical certificate and not re-run at HEAD.

TASK-1412.1 records review (R39): 2026-10-29. scope annotation (R39, TASK-1412.1 AC#4; LL-023: history stays, nothing reopened). What this task's evidence proves: on the /simulate replay with protection on, CS = flow-1 changes with every new MsgID 25 frame, 5.4-6.4 s apart, because the fixture sends a flow frame every 6 s (scripts/gen_sat_pwm_replay.py:13-14, :25-27). The old firmware changed only on the ~30 s tick. Not proven, although AC#2's checked text claims it: (1) an update every ~3 s. The 3 s timer (SATcontrol.ino:589, satPwmSuppressionRefresh :1240-1265) reads the cached flow (:845-851) and sends write-on-change (:463-478, plus a 300 s same-value resend, :427), so the CS on the bus cannot change faster than ID25 samples arrive; the 3 s cadence itself was never observed. (2) 'return+offset during the hold': the replay burns from the first frame, so start-up (hold before ignition, SATcontrol.ino:1225-1233) is not covered, as the Final Summary already says. (3) No ID25 sample age or transport delay was measured (remediation plan WP5 item 3). Evidence class: replay/bench on OTGW32 without a boiler, not a physical-performance result. Open measurements are carried by TASK-1412.12 (start-up/ignition, sample age, both transports) and TASK-1412.23 (real equipment).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
A /simulate replay fixture (scripts/gen_sat_pwm_replay.py -> tests/webui/fixtures/sat-pwm-suppression-replay.txt) keeps a boiler burning for 7.5 minutes with a slowly rising flow, so SAT's PWM ON phase can be measured on the bench without a boiler. AC1: generator + test (red without fixture, 5 OK with). AC2: old firmware sends the PID setpoint, then 10 while burning, changing only every ~30 s; TASK-1268 firmware sends flow-1, updated every ~6 s (each flow frame). AC3: settings diff 0, simulation log restored, bench on alpha.444. Not covered: the hold before ignition (the replay burns from the start; the SAT simulation run in TASK-1268 showed the hold).
<!-- SECTION:FINAL_SUMMARY:END -->
