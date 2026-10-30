---
id: TASK-1417
title: >-
  SAT PID: fixed deadband 0.1, remove the deadband setting and its widening
  (entry 76, Q26)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-28T06:45:00Z'
updated_date: '2026-10-30T05:50:00Z'
labels:
  - sat
  - sergeant-d
milestone: 2.0.0
dependencies:
  - TASK-1368
  - TASK-1416
references:
  - docs/sat/sergeant-d-corrections.md
documentation:
  - docs/sat/sat-low-load-execution-backlog-2026-10-28.md
priority: medium
ordinal: 581000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Sergeant D 2026-10-27 (entry 76, Q26: fixed 0.1). Deadband constant 0.1 as thermo-nova const.py:16; remove SATdeadband setting, REST/MQTT/UI exposure and the widening during room-temperature estimation. Sites listed in entry 76 (SATtypes.h, settingStuff.ino, settingsHelp.h, MQTTstuff.ino, restAPI.ino, SATcontrol.ino, SATpid.ino, v2.js, handleDebug.ino). API change: update docs/api/MQTT.md and REST docs. Update ADR-197.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Host test: deadband is 0.1 regardless of a stored SATdeadband value and while the room temperature is estimated
- [x] #2 Setting, REST field, MQTT set command and UI control removed; docs/api updated
- [x] #3 build + evaluate green
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-28 dependency reconciliation (Robert's go: reconcile the graph alongside the wave-3 work; scout+verifier report): TASK-1412.3 dependency replaced by TASK-1368 (own host-test AC#1; PID tasks run one at a time after 1368).

2026-10-28 Robert requested this execution-graph update. Preserve the earlier reconciliation. Pending PID writers run 1368 -> 1270 -> 1416 -> 1417 -> 1418 -> 1414 -> 1423 -> 1415; 1381 remains a separate branch after 1270 and Q53. Revalidate task premises against preceding commits. Integration also requires the DHW and auto-tune outcomes through their linked verification gates. Approval and bench gates remain in force.

2026-10-30 (otgw-firmware-84, wt-1416, local commit c5eefa142 on top of a2942c547, no bump). SAT_PID_DEADBAND = 0.1f in SATtypes.h (header: SATcontrol.ino precedes SATpid.ino in the TU); setting, REST, MQTT (command + sat/deadband, retired through kRetiredSatEntities), v2 UI, help, debug dump and the estimation widening removed; curve recommendation threshold fixed 0.2. thermo-nova opened at the pin: const.py:16, pid.py:187, :205, sensor.py:301. Evidence: test_sat_pid_cadence Widening on origin/dev FAILED (failures=2, I=0.710286 with stored 2.0 and with 1 h takeover widening), on fix 4/4 OK; DeadbandSettingGone guard OK (28 token hits on origin/dev, retired entry absent there). All 56 tests/test_*.py exit 0; evaluate --quick 79/0/0. Docs + CHANGELOG (Removed, breaking) updated. AC#3 build: open, comes with the coordinator's batch build.

Bench 2026-10-30 (coordinator otgw-firmware-b8, Classic-S3 .63, PIC 6.8): batch rebuilt after the 05:50 PC restart; esp32-combo alpha.547+6240b05 and alpha.548+8ded885 built (artifacts 06:12 / 06:42, githash = HEAD), evaluate --quick 79/0/0 on both; OTA app via safeboot + FS via /update?cmd=100, 174 settings keys kept. TASK-1417: old alpha.535 /api/v2/settings has satdeadband; alpha.547/548: field absent, POST {name:satdeadband,value:0.5} -> HTTP 400 {error:Unknown setting name}, still absent after. SAT simulation: raw_derivative stays 0 while |error| < 0.1 (target 20, room ~19.93) and moves at error 0.128 (06:32, rd 0.000238, D 2.33); the integral resets to 0 outside the deadband, as pid.py:186-189. Pushed 8ded885eb, zip alpha.548 posted (msg 1565603481086066738).
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Fixed PID deadband 0.1 (SAT_PID_DEADBAND), setting/REST/MQTT/UI/widening removed (alpha.546, BREAKING). AC#1 host test (test_sat_pid_cadence widening red on origin/dev, 4/4 OK on fix; notes 2026-10-30 otgw-firmware-84). AC#2 removal + docs (same notes) and bench: POST satdeadband -> 400 Unknown setting name on alpha.548. AC#3 esp32-combo build alpha.548+8ded885 + evaluate 79/0/0, this session. Published in the alpha.548 zip.
<!-- SECTION:FINAL_SUMMARY:END -->
