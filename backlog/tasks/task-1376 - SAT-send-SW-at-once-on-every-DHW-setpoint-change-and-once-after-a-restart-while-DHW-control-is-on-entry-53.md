---
id: TASK-1376
title: >-
  SAT: send SW= at once on every DHW setpoint change and once after a restart,
  while DHW control is on (entry 53)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-25T08:29:00Z'
updated_date: '2026-10-30T14:35:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
ordinal: 511000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D entry 53 (2026-10-25, answer to Q17: A). Entry 33: a DHW setpoint change sends SW={value}. Firmware today sends SW= only on the off-to-on edge of a hot water draw and only with bDhwEnabled (SATcontrol.ino:4885-4891 at HEAD 31933adaf). thermo-nova sends at once on change (number.py:72-74). Change: send SW=<int> when the setting changes (REST/MQTT/UI) and once after boot/SAT enable, while bDhwEnabled; keep the draw-edge send or drop it (decide in plan). Note entry 53: users set it once and forget it.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 A change of dhw_setpoint while DHW control is on sends SW=<value> once, at once (host test red on HEAD, green on fix)
- [x] #2 After a restart with DHW control on, SW= is sent once when the command interface is up
- [x] #3 No SW= while DHW control is off
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-29: Sergeant D entry 69 (2026-10-27, Q17: yes) re-confirms entries 33 and 53: SW= at once on every change and once after a restart while DHW control is on; slider bounded by MsgID 48. No change to scope. Open (from entry 69): whether the SW= on a draw edge also stays; decide in the plan.

2026-10-30: Sergeant D entry 146: must be in beta 1 (TASK-1504, target Sun 2026-11-01). Priority raised to high.

Robert 2026-10-30: SW= sync only while SAT is enabled (not when SAT is off with DHW control on). Plan drafted: satDhwSetpointSync() after satRestartStep() (strict entry 121 reading), drop the draw-edge send if Sergeant D agrees.

2026-10-30 coordinator (session 80): lane reviewed (opus, ready after dropping the duplicated corrections hunk); known limits documented in firmware-sat-spec.md (no SW= after a boiler power cycle / OT-link recovery, queue-full drop not retried, HA DHW climate entity bypasses settings.sat.fDhwSetpoint so the restart send restores SAT's stored value). Integrated on alpha.554 as alpha.555; conflicts with 1370 resolved (draw-edge SW= removed, 1370's resend disarm kept); test_sat_ot_loss harness got a satDhwSetpointSync stub. Gates: evaluate --quick EXIT 0; 68/69 Python test files OK (test_settings_float_roundtrip pre-existing); test_sat_dhw_sw_sync red on base (2 failures, 3 errors, 7 skipped: function absent), 13 OK on fix.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
alpha.555 (5e8366d3f, pushed, zip msg 1565735469747011745). With SAT and DHW control on, SW=<setpoint> goes out at once on a setpoint change and once after a restart / SAT re-enable / DHW-control re-enable, after satRestartStep (entry 121); draw-edge SW= removed (entry 150 A); nothing with DHW control or SAT off. Evidence: tests/test_sat_dhw_sw_sync.py on the real satDhwSetpointSync: base 2 F / 3 E / 7 skipped (function absent), fix 13 OK; dhw_range 14 OK, restart_quiet 17 OK; full suite 68/69 (pre-existing float roundtrip); evaluate EXIT 0. Bench .63 alpha.555 (PIC, sim) telnet CmdQueue: AC3 setpoint 50 with DHW control off -> no SW=; enable -> SW=50 at once; setpoint 52 -> SW=52 within 0.1 s of the capture mark; SAT off/on -> exactly one SW=52 1.2 s after enable; 0 queue errors. AC2: the reboot send itself was not observable (SAT takes over in sim before telnet is reachable); the same re-arm path is shown by the SAT re-enable and covered by the host test. NOT verified: OT-Direct (unit offline).
<!-- SECTION:FINAL_SUMMARY:END -->
