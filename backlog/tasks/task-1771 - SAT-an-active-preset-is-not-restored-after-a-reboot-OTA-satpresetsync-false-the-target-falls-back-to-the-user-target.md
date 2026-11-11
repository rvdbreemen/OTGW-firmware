---
id: TASK-1771
title: >-
  SAT: an active preset is not restored after a reboot/OTA (satpresetsync
  false): the target falls back to the user target
status: To Do
assignee: []
created_date: '2026-11-10T03:24:00Z'
updated_date: '2026-11-11T12:30:00Z'
labels:
  - sat
milestone: 2.0.0
dependencies: []
priority: low
ordinal: 868000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Seen on .63 (Classic + PIC, SAT enabled) around the beta.166 OTA of 2026-11-09 23:14: GET /api/v2/settings showed sattargettemp 19.8 at 23:01, 21.0 just before the OTA (21.0 = satpresetcomfort), 19.8 after it. GET /api/v2/sat/status on beta.166 at 04:22: active_preset 0 (none), target_temp 19.8, follow_thermostat false (body saved in the session scratchpad). Reading (hypothesis, not proven: no record of active_preset before the OTA): the comfort preset was active, GET settings reports the active target (restAPI.ino sattargettemp = settings.sat.fTargetTemp), settings.ini stores satUserTargetTemp() (settingStuff.ino:358, never a preset), and the reboot did not bring the preset back; the restore (SATcontrol.ino ~2016) needs a held preset in the SAT state file, which may not have been written before the OTA restart. Open questions: who set comfort around 23:0x (HA/MQTT sat/preset? the web UI?), and should an active preset survive a reboot/OTA (thermo-nova behaviour, ask Sergeant D per the SAT ground-truth rule).
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [ ] #1 Reproduced on the bench: set a preset, reboot (and OTA), read active_preset and target before/after; capture the SAT state file write
- [ ] #2 Decision recorded (Sergeant D / Robert): an active preset survives a reboot or not; implement and test accordingly
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-11: Sergeant D answered Q-1771a with A (07:26:05 UTC, msg 1569976495491915849, entry 308): restore the active preset and its target after a reboot or update, like thermo-nova climate.py:175-181 (target) and :187-188 (preset). The behaviour is settled; the task keeps needs-maintainer-ok until Robert releases it.

2026-11-11: released by Robert (AskUserQuestion in the coordinator session: 'Release, build it'). Behaviour per Sergeant D entry 308 (Q-1771a A: restore the active preset and its target after a restart, like thermo-nova climate.py:175-188). Queue: after the OT-Direct audit fixes; worker to be assigned by the coordinator.
<!-- SECTION:NOTES:END -->
