---
id: TASK-1480
title: >-
  SAT pressure-health drop rate differs from thermo-nova: 10-minute window and
  suspension on SAT on/off instead of CH active
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-29T14:38:00Z'
updated_date: '2026-10-30T00:11:00Z'
labels:
  - sat
  - pressure
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 640000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found in gruijter's field capture (TASK-1474, alpha.527): sat/pressure_alarm fired 10 times in 4.5 h (09:15 ... 13:27), each cleared within minutes. The boiler CH pressure (MsgID 18, read ~every 5 min) is ~2.0 bar while the slave CH-mode bit (pump incl. overrun) is set and 1.70-1.74 bar when it clears, so every pump stop is a 0.3 bar step that the regression reads as ~1.7 bar/h. Firmware satUpdatePressure() (SATcontrol.ino ~3864-4022 at fd491ff): 20-slot ring x 30 s, so the real window is ~10 min (the 3600 s eviction is dead code), and the 600 s suspension/clear follows state.sat.bActive (SAT on/off). thermo-nova binary_sensor.py:335-345 and 357-388 (constants :25-28): window max(age, 3600 s), suspension/clear on coordinator.active (= CH active; serial coordinator master CH enable :54, MQTT OpenTherm coordinator central-heating state opentherm.py:60). Second-reader simulation on the real series: firmware model reproduces 9/10 logged alarms; thermo-nova would also alarm (9 with master CH enable, 4-5 with the MQTT CH-mode signal). The alarm is display/MQTT only (no control effect). Whether a pump-head step should count at all is a question for Sergeant D (via the coordinator).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 satUpdatePressure follows thermo-nova binary_sensor.py:335-388 (window max(age,3600 s), suspension and sample clear on the CH-active transition), host test on the real function with the captured 2.0/1.7 bar series old vs new
- [ ] #2 Sergeant D's answer on pump-head steps is recorded in sergeant-d-corrections.md before any behaviour beyond parity is changed
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-30 close-out: merged on origin/dev as ab41bd626 (alpha.536). Batch build 2.0.0-alpha.542+c00c5f0 (esp32-combo from origin/dev c00c5f025, 2026-10-30 00:47, exit 0, fresh artefacts, Discord msg 1565512587494301767); integration check on the merged set: evaluate --quick 100%, 0 failed; every tests/test_sat_*.py plus tests/test_manual_sat_commands.py exit 0. AC#1 evidence, rerun this session on c00c5f025 with the local field capture (OTGW_PRESSURE_CAPTURE, data not committed) and OTGW_REQUIRE_HOST_CC=1: python tests/test_sat_pressure_health.py Ran 7, OK; with the pre-fix SATcontrol.ino (ab41bd626^) FAILED (failures=7; e.g. 0.514286 != 0.0 at t=1801 s, drop rate not suspended after CH enable fell; suspended on SAT on/off). Capture replay: 553 evaluations over 4.60 h; alarm episodes: field (published) 10, old firmware 11 (matches all 10 field start times), new firmware 9, thermo-nova on master CH enable (bit 8) 9, thermo-nova on the boiler CH-mode bit (bit 1) 5; drop rate suspended in 61% of evaluations with bit 8. Synthetic capture-shaped series: 7/7/7/0. Long run 1/1. This is parity with thermo-nova, not a fix for the field alarm flood (10 -> 9).

Open interpretation (implementer, not a ruling): thermo-nova's coordinator.active is mapped to the master CH enable bit (Statusflags 0x0100), as the serial coordinator (coordinator/serial/__init__.py:53-55) and the TASK-1387 precedent (SATcycles.ino:614/969). thermo-nova's MQTT coordinator uses centralheating == ON (coordinator/mqtt/opentherm.py:23, :60-61), which is the boiler's slave CH-mode bit in this firmware (OTGW-Core.ino:2520). With bit 8, parity does not reduce the field alarms; the slave CH-mode bit would (9 vs 5 on the capture). Because SAT clears CH enable in every PWM OFF phase (entries 5, 9, 17; entry 60 for the CH-mode bit), every PWM phase change suspends the drop rate for 600 s and clears the samples. This question is NOT yet queued in docs/sat/open-questions.md (checked 2026-10-30); it goes to Sergeant D together with AC#2.

AC#2 open: Sergeant D's answer on whether a pump-head step should count is not recorded yet.

alpha.542 bench check failed 2026-10-30 (v2.js 64 KiB cut, settings loss after FS OTA); Done waits for a passing bench check on the fixed build.

2026-10-30: the coordinator.active mapping question and AC#2 (pump-head step) are now queued for Sergeant D as Q70 in docs/sat/open-questions.md (queued, not asked; advice: boiler CH-mode bit, same mapping as Q69 A). Supersedes the 'not yet queued' remarks above.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
satUpdatePressure now follows thermo-nova binary_sensor.py:335-388: window max(age, 3600 s) in a 121-slot FIFO with age eviction, 600 s suspension and sample clear on the CH-active transition (master CH enable bit), not on SAT on/off; millis-wrap safe suspension. Merged ab41bd626, alpha.536.

Evidence per AC:
- AC#1: tests/test_sat_pressure_health.py on the real pressure block, rerun 2026-10-30 on c00c5f025 with the local field capture: Ran 7 OK; pre-fix code FAILED (failures=7). Capture replay: field 10, old 11, new 9, thermo-nova bit 8 9, bit 1 5 alarm episodes. Parity, not a cure for the alarm flood.
- AC#2 (open): Sergeant D's answer on pump-head steps not recorded yet. Also open and not yet queued: which signal stands for coordinator.active (master CH enable chosen; the slave CH-mode bit would halve the field alarms).
Status: In Progress. alpha.542 bench check failed 2026-10-30 (v2.js 64 KiB cut, settings loss after FS OTA); Done waits for a passing bench check on the fixed build.
<!-- SECTION:FINAL_SUMMARY:END -->
