---
id: TASK-1243
title: settings.ini can hold 'nan' for a float setting (invalid JSON)
status: Done
assignee:
  - '@claude'
created_date: '2026-10-17 20:54'
updated_date: '2026-10-17 22:17'
labels:
  - bug
  - settings
dependencies: []
ordinal: 381000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found by reading code during TASK-1242, not reproduced. updateSetting parses floats with strtof and clamps with constrain(); constrain(NaN, lo, hi) returns NaN because both comparisons are false. writeJsonFloatKV then writes dtostrf(NaN) = 'nan', which is not JSON. A REST POST of the string 'nan' to a float setting is the likely trigger.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 Bench: POST value 'nan' to a float setting on the old firmware, show the stored value and the settings.ini line
- [x] #2 Fix rejects or clamps non-finite floats; same POST on the fix leaves a finite value and valid JSON
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence 2026-10-18, Classic-S3 COM8/.88.63, esp32-combo, probe .tmp/bench-1243/set_probe.py. OLD alpha.434: POST SATpvboostdeltac='nan' -> 200, file line '"SATpvboostdeltac": nan,' (json.load fails line 141), and /api/v2/settings also invalid JSON ('"value":nan'), which breaks the v2 settings page: wider than the description assumed. FIX alpha.435+647a4bb (54 float settings via clampSettingFloat): boot with the stored 'nan' -> REST valid JSON, value 1.5 (default); POST 'nan' and 'inf' -> no flush, value kept; POST 2.0 -> valid JSON 2.00; POST 1.5 -> settings.ini byte-identical to the baseline, REST 176/176 no diffs. Gate: evaluate --quick FAILED on the old code (54 raw atof/strtof of newValue), PASS on the fix; tests/test_evaluate.py OK. Firmware image 2.7 KB smaller. NOT covered: a unit that already stored 'nan' keeps it in settings.ini until the next real save (memory and REST are healed at boot); only affects units that hit this bug before. Not addressed: other non-settings floats in REST JSON were not audited.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
Float settings reject non-finite input: clampSettingFloat() keeps the current value for NaN/inf and clamps otherwise; all 54 float settings use it, and readSettings() goes through the same path. Bench old-vs-fix: on alpha.434 'nan' made both settings.ini and /api/v2/settings invalid JSON; on alpha.435 'nan'/'inf' are ignored, a stored 'nan' heals to the default at boot, and normal values work. Gate fails on any raw atof/strtof of newValue. Residual: a stored 'nan' stays in the file until the next save. alpha.435.
<!-- SECTION:FINAL_SUMMARY:END -->
