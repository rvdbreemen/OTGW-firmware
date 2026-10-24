---
id: TASK-1344
title: >-
  SAT: release the MM override when SAT stops (disable, trip, fallback end,
  reboot)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-24T10:10:00Z'
updated_date: '2026-10-24T14:28:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: high
ordinal: 467000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by the SAT port audit 2026-10-24 (docs/sat/sat-port-audit-2026-10-24.md), finding #1. satDisable() (SATcontrol.ino:2328) only sends CS=0; no SAT path sends MM release, so MM=0 from PWM stays on the PIC (gateway.asm:4752-4765, single source) and the thermostat gets the boiler back at max modulation 0 %. thermo-nova serial coordinator sets MM on every update (coordinator/serial/__init__.py:228-233). Not a behaviour choice for SAT control itself (it is the hand-back), but name it to Sergeant D in the plan.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Every SAT stop path (disable, trip, MQTT-fallback end, boot with SAT off) releases MM (host test red on HEAD, green on fix; bench capture shows MM= release on PIC)
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-24: fix in satDisable(): MM=T after CS=0/CH=1 (non-numeric MM= clears the override: PIC gateway.asm:4752 SetMaxModLevel -> ClrMaxModLevel when GetDecimalArg fails; pyotgw.py:542-543 'Clear the setting by specifying a non-numeric value'; OT-Direct MM= handler clearWriteOverride(14) for non-digit/non-'-'). Every stop path calls satDisable(): settings disable (settingStuff.ino:989), early bail (:4765), fallback end (:4710), both safety trips (:4875, :5209). Boot with SAT off dropped from scope: an ESP-only reboot with SAT enabled resumes SAT; a power cut also resets the PIC; a user-disabled SAT already released at disable time. Evidence: tests/test_otd_ch_override.py test_sat_disable_releases_mm OK on fix, FAIL on HEAD; SAT tests OK; evaluate 0 failed; build.bat SUCCESS. BLOCKED AC: PIC bench capture (no PIC board connected); OT-Direct has no PR report for MM, so no runtime capture there either.

2026-10-24: shipped in alpha.493+97b1e5b; asked gruijter (PIC Classic) to check MsgID 14 after switching SAT off in PWM.
<!-- SECTION:NOTES:END -->
