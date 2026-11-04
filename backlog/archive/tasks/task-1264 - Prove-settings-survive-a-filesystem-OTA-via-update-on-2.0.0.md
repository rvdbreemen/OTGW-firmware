---
id: TASK-1264
title: Prove settings survive a filesystem OTA via /update on 2.0.0
status: Done
assignee:
  - '@claude'
created_date: '2026-10-20 01:43'
updated_date: '2026-10-20 01:59'
labels:
  - ota
  - settings
milestone: 2.0.0
dependencies: []
priority: medium
ordinal: 394000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
The 2.0.0 updater rewrites settings.ini from RAM after a filesystem upload (OTGW-ModUpdateServer-esp32.h ~441-455, same as 1.x finalizeFsOta). Release notes and the zip README said a filesystem upload resets settings; never bench-proven either way (TASK-1223.9 proved fs OTA works, not settings survival). Other user files on LittleFS (dallas_labels.ini, sat_markers) are not restored.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Bench: GET /api/v2/settings before and after a WiFi filesystem upload through /update shows 0 differing keys (diff transcript)
- [x] #2 Release notes / README / Discord statements match the measured result
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
2026-10-20 bench OTGW32 .88.61: baseline alpha.427 settings 567 keys. Firmware via POST /safeboot + safeboot upload (field firmware): 10.5 s, alpha.439+9ac0c39 up after 22 s, settings diff 0/567. Filesystem via POST /update?cmd=100&size=786432 (field filesystem), 8.3 s, back after 23 s: /version.hash 4755422 -> 9ac0c39 (new image live), settings diff 0/567 (re-computed by hand from the saved JSON). settings.ini rewritten (5574 -> 5575 B: the missing comma after SATpvboostmaxdurationmin from alpha.427's writer is now present, TASK-1242 fix). dallas_labels.ini did not exist before, so its loss is inferred from code, not measured. Statements: README in zip (TASK-1263) already says write-back + safety net; docs/guides/WIFI_FIRMWARE_UPDATE.md corrected (said 'resets your settings'); Discord correction posted.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Measured on the bench: a WiFi filesystem upload through /update keeps all settings (0 of 567 keys changed, alpha.427 -> alpha.439, new filesystem image confirmed live via version.hash). The updater writes the in-RAM settings back after the upload. Corrected docs/guides/WIFI_FIRMWARE_UPDATE.md and the Discord statement; the zip README already matched. Not measured: loss of dallas_labels.ini (did not exist on the unit); inferred from code. USB filesystem writes (flash_otgw --update) still reset settings, by design.
<!-- SECTION:FINAL_SUMMARY:END -->
