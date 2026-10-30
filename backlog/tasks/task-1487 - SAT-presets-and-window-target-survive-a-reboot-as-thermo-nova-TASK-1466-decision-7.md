---
id: TASK-1487
title: >-
  SAT presets and window target survive a reboot as thermo-nova (TASK-1466
  decision 7)
status: In Progress
assignee:
  - '@claude'
created_date: '2026-10-29T20:32:00Z'
updated_date: '2026-10-29T23:59:00Z'
labels:
  - sat
  - bug
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 646000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Robert 2026-10-29: as thermo-nova climate.py:169-190 (restore preset_mode, pre-custom temperature, target). Today satHandlePreset/_satCheckWindowTimer write the preset target into settings.sat.fTargetTemp in RAM only (SATcontrol.ino:1394, :1562) while eActivePreset and fPreCustomTemp are state; a later settings flush can persist the preset target as the user's own target. Persist active preset + pre-preset temperature (+ window pre-target) and keep the user's own target separate.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 After a reboot the active preset is still active and preset 'none' restores the user's own temperature (host test red on HEAD); an unrelated settings save never stores a preset temperature as the user's target
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-30 close-out: merged on origin/dev as 13f8aceb5 (alpha.537). Batch build 2.0.0-alpha.542+c00c5f0 (esp32-combo from origin/dev c00c5f025, 2026-10-30 00:47, exit 0, fresh artefacts, Discord msg 1565512587494301767); integration check on the merged set: evaluate --quick 100%, 0 failed; every tests/test_sat_*.py plus tests/test_manual_sat_commands.py exit 0. Workflow journal: red on origin/dev python tests/test_sat_preset_persist.py FAILED (failures=6, all behavioural: preset NONE after a reboot, 21.5/16.0/18.0 stored as the user's target); a verifier reproduced 6/6 red and found a further path (eco -> window switch -> none stored 18.0); the fix round added 3 cases, red on e3166a4be (failures=3), green Ran 9 OK; 35 SAT harnesses rc=0; evaluate --quick 79/0/0. Fresh rerun this session on c00c5f025: Ran 9, OK. Behaviour change, following thermo-nova climate.py:830-832 and :610-620: a preset other than activity chosen during a window switch ends the switch, and the window close restores only while the preset is ACTIVITY. The implementer asked for Sergeant D's confirmation of this; it is not queued in docs/sat/open-questions.md (checked 2026-10-30). The reboot in the test pipes satPresetStateFormat into satPresetStateApply; the LittleFS save/load (/sat/sat_preset.json), change detection and initSAT ordering were checked by reading only. Bench check pending: set a preset, reboot, read /sat/sat_preset.json and SATtargettemp in settings.ini. Known difference kept: a settings POST whose target equals a preset value clears the preset instead of selecting it (climate.py:563-565).

alpha.542 bench check failed 2026-10-30 (v2.js 64 KiB cut, settings loss after FS OTA); Done waits for a passing bench check on the fixed build.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
The active preset, the pre-preset temperature and the window pre-target survive a reboot in /sat/sat_preset.json (state per ADR-051, written on change from the loop task behind a 1 s gate, kept by a filesystem OTA). writeSettings stores the user's own target (satUserTargetTemp), never a preset or window temperature. thermo-nova climate.py:176-194, :567, :610-620, :819, :830-832. Merged 13f8aceb5, alpha.537.

Evidence per AC:
- AC#1 preset survives a reboot and none restores the user's temperature: tests/test_sat_preset_persist.py on the real extracted functions; red on origin/dev 6/6, green 9/9, rerun OK on c00c5f025 on 2026-10-30.
- AC#1 an unrelated settings save never stores a preset temperature: same test, including the verifier's eco -> window -> none path (red on the first lane commit, green after the fix).
- Build 2.0.0-alpha.542+c00c5f0 exit 0; evaluate 100%, 0 failed.
Not verified: the LittleFS file round trip and a real reboot (code reading only; the test simulates the reboot). Open: Sergeant D's confirmation that a preset chosen during a window switch ends it (thermo-nova behaviour) is not yet queued.
Status: In Progress. alpha.542 bench check failed 2026-10-30 (v2.js 64 KiB cut, settings loss after FS OTA); Done waits for a passing bench check on the fixed build.
<!-- SECTION:FINAL_SUMMARY:END -->
