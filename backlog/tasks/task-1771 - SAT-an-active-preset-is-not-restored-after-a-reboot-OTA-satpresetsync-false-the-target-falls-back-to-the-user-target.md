---
id: TASK-1771
title: >-
  SAT: an active preset is not restored after a reboot/OTA (satpresetsync
  false): the target falls back to the user target
status: In Progress
assignee:
  - '@claude'
created_date: '2026-11-10T03:24:00Z'
updated_date: '2026-11-11T14:42:00Z'
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
- [x] #1 Reproduced on the bench: set a preset, reboot (and OTA), read active_preset and target before/after; capture the SAT state file write
- [ ] #2 Decision recorded (Sergeant D / Robert): an active preset survives a reboot or not; implement and test accordingly
<!-- AC:END -->

## Implementation Plan

<!-- SECTION:PLAN:BEGIN -->
Behaviour (Sergeant D entry 308, Q-1771a A): after a restart the active preset and its target come back, as thermo-nova restores them through RestoreEntity: custom_components/sat/climate.py:175-181 (target, ATTR_TEMPERATURE) and :187-188 (preset, ATTR_PRESET_MODE); also :190-194 (pre-activity / pre-custom temperature). The firmware has done this since TASK-1487 (/sat/sat_preset.json, satPresetStateApply in initSAT()); a plain /ReBoot keeps the preset on the bench. The loss is an OTA timing bug, not SAT behaviour, so it does not change how SAT controls the boiler and needs no new Sergeant D question. Fix (guard plus keep, the TASK-1266/1286 pattern): 1) satPersistAllToFs() writes nothing until initSAT() has loaded the SAT state files (_sat_stateLoaded); 2) the SAT state files join the update server's KEEP_LIST (first, before the larger entries; KEEP_TOTAL_MAX 8192 -> 10240, maxima 9824), so a filesystem image written before initSAT() keeps them via the heap, and once loaded RAM (written after the restore) wins; /sat_tune.json, which has no RAM writer, is kept the same way (it was wiped by every filesystem OTA); 3) the restore creates /sat (open 'w', true). Settings and the OT profiles load before startWebserver() (OTGW-firmware.ino:333/336 vs :524), so only SAT was exposed. Proof: the bench repro (fs OTA the moment HTTP answers) red on beta.180, green on the fix, plus the late fs OTA still green, with non-trivial energy and tune files.
<!-- SECTION:PLAN:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-11-11: Sergeant D answered Q-1771a with A (07:26:05 UTC, msg 1569976495491915849, entry 308): restore the active preset and its target after a reboot or update, like thermo-nova climate.py:175-181 (target) and :187-188 (preset). The behaviour is settled; the task keeps needs-maintainer-ok until Robert releases it.

2026-11-11: released by Robert (AskUserQuestion in the coordinator session: 'Release, build it'). Behaviour per Sergeant D entry 308 (Q-1771a A: restore the active preset and its target after a restart, like thermo-nova climate.py:175-188). Queue: after the OT-Direct audit fixes; worker to be assigned by the coordinator.

2026-11-11 d3 bench .61 (beta.180+2b5c29a, OT-Direct, satsimulation on, SAT enabled for the run, settings saved/restored, diff 0). POST /api/v2/sat/preset comfort -> active_preset 3, target 21.0, /sat/sat_preset.json {"p":3,"t":21.00,"pc":20.00,...} written within 1 s (telnet 'SAT: preset state saved'). (1) /ReBoot: preset 3 / 21.0 kept. (2) ota_flash.py app+fs: preset LOST (active_preset 0, target 20.0, file rewritten to p:0) = the .63 symptom. (3) app-only OTA: kept. (4) fs-only OTA long after boot (15:32): kept. (5) /ReBoot, then fs-only OTA the moment HTTP answers (15:33:54): LOST again, file p:0. Root cause: setup() starts the async web server (OTGW-firmware.ino:524) before initSAT() (:600) loads the SAT state files; a filesystem upload in that window runs _restoreRuntimeFiles() -> satPersistAllToFs() (OTGW-ModUpdateServer-esp32.h:517), which writes the not-yet-loaded RAM state (p:0) over the restored files. ota_flash.py (and anyone flashing app then fs quickly) uploads the fs right after the new app answers, so it hits the window. The same path writes the energy totals and the HCR/PID state, so they are exposed the same way (not measured: .61 has no real energy data).

2026-11-11 implemented on lane-1771 (not built yet, build slot after 09's beta.181): OTGW-ModUpdateServer-esp32.h KEEP_LIST + cap + create-dirs restore; SATcontrol.ino _sat_stateLoaded guard set after the last load in initSAT(). tests/test_fs_update_keep.py updated (list, cap, guard and flag order): 37 OK; on the old source 3 of its checks fail (source checks; the behaviour proof is the bench). Also found: satSaveEnergyState() writes state.sat.fEnergyTotal without a loaded check, so before the fix an early fs OTA would reset the energy total too; and /sat_tune.json (self-tune window, TASK-1525/1796) was in neither list, so every filesystem OTA restarted the 3-day window.

For the release note (coordinator, 2026-11-11): before this fix every filesystem update restarted the self-tune's 3-day window (/sat_tune.json was wiped), so a tester who flashed every alpha within 3 days never got a self-tune decision. Bench plan for the build slot (after 09's beta.181, this commit re-bumped to 182): red already on beta.180; green on 182 with the same fs OTA right after boot, with a non-zero energy total and a /sat_tune.json present before the OTA, both checked after.
<!-- SECTION:NOTES:END -->
