---
id: TASK-1536
title: >-
  SAT: every transmitted CS is capped by the effective max setpoint
  (thermal-safe fallback, 5 s resend, PIC keepalive)
status: Done
assignee:
  - '@claude'
created_date: '2026-11-01T00:01:00Z'
updated_date: '2026-11-02T05:20:00Z'
labels:
  - sat
  - low-load
dependencies: []
priority: high
ordinal: 692000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the TASK-1412.2 second read (2026-11-01): the thermal-safe fallback caps CS with satGetMaxSetpoint() (user MaxTSet only, SATcontrol.ino:6515-6522) instead of satEffectiveMaxSetpoint() (:405, which also includes the boiler's own MaxTSet), and the 5 s resend (:634-641) and the PIC CS keepalive (:597-606) repeat the last CS without clamping it again after a limit change. Related: TASK-1412.13 (MaxTSet negotiation and every transmitted ceiling), entries 48/51/54/67/100/110/148/152.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test on the real code: thermal-safe fallback, resend and keepalive never send a CS above satEffectiveMaxSetpoint(), also right after the limit is lowered (red on HEAD, green)
- [x] #2 Decision check against entries 148/152 recorded in the notes before the change
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
1) satCapRepeatCS(): the 5 s resend and the PIC keepalive cap the repeated CS at satEffectiveMaxSetpoint() and store the capped value in the cache. 2) satThermalSafeSetpoint() = min(SAT_THERMAL_SAFE_FLOW, satEffectiveMaxSetpoint()) replaces the user-MaxTSet-only cap in the thermal-safe branch. 3) tests/test_sat_cs_ceiling_1536.py on the real code, red on HEAD; stub satEffectiveMaxSetpoint() in the 4 harnesses that compile the command block.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Triage 2026-11-02 (beta.5, origin/dev 6c41cc696) in code: no: thermal-safe CS=45 capped by satGetMaxSetpoint (user MaxTSet) not satEffectiveMaxSetpoint (boiler MaxTSet), SATcontrol.ino:6549; resend :663 and keepalive :629 repeat last CS unclamped. Agent-doable now.

AC#2 decision check (2026-11-02): entry 148 'SAT still caps every CS at the default (62 radiators, 50 underfloor)' and entry 152 'that value ... is the only cap: SH= carries it and SAT caps CS at it'. Both say EVERY CS is capped; the repeats and the fallback were the exceptions, so this change applies the decided rule to all senders and decides nothing new. thermo-nova (pinned 7beaaf5, opened this session): the CS clamp is clamp(setpoint, MINIMUM_SETPOINT, coordinator.maximum_setpoint) (climate.py:379, :394), and maximum_setpoint is the user or default limit (coordinator/__init__.py:275-283); the boiler's own MaxTSet is only exposed (coordinator/mqtt/opentherm.py:79-83), not used in that clamp. The boiler-MaxTSet term is a firmware rule that the control tick already applies (satEffectiveMaxSetpoint, TASK-1373); this task extends it to the repeat paths and the firmware-only thermal-safe fallback (no thermo-nova equivalent). No new behaviour on the tick path, so no Sergeant D question; mentioned in the overview as a notice.

Evidence 2026-11-02: host tests/test_sat_cs_ceiling_1536.py on the real code: HEAD (ca54e4a73~1) 4 of 5 FAIL (resend 'CS=60.0' with a 40 limit, keepalive 'CS=60.0', fallback helper absent, call site uses satGetMaxSetpoint); fix 5/5 OK. Full tests folder green (one harness fixed, test_sat_max_tset needed SAT_THERMAL_SAFE_FLOW). evaluate.py --quick: 0 failed, health 100%. Build esp32-combo 2.0.0-beta.6+ca54e4a SUCCESS; OTA app to .63 (175 settings unchanged). Bench (scratchpad bench_1536.py, SAT sim, interval 300 s, ceiling lowered by SATsystem 2 = underfloor 50, no SH=): beta.5 06:09:12 change, CS=62.0 every 5 s for 40 s; beta.6 06:18:23 change, 06:18:24 CS=50.0 and every 5 s after, tick value still 62. Settings restored. Pushed ca54e4a73, zip posted (msg 1566683194735067166), #alpha-testing note 1566683232844390411. Not bench-proven: the PIC keepalive path (host test only) and the thermal-safe fallback (needs 2 h without room temperature; host test only).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
SAT's 5 s CS resend and 30 s PIC keepalive now cap the repeated CS at satEffectiveMaxSetpoint() (satCapRepeatCS), and the thermal-safe fallback uses min(45, effective max) (satThermalSafeSetpoint). AC1: host test red on HEAD (4/5), green on the fix (5/5); bench .63 old-vs-fix: beta.5 kept CS=62 after the ceiling went to 50, beta.6 sent CS=50 one second later. AC2: entries 148/152 checked (every CS is capped; this applies the decided rule, nothing new); thermo-nova clamp at climate.py:379/:394 uses the user/default maximum, the boiler-MaxTSet term is the firmware's existing tick rule. Keepalive and fallback proven on host only. Shipped as 2.0.0-beta.6 (ca54e4a73).
<!-- SECTION:FINAL_SUMMARY:END -->
