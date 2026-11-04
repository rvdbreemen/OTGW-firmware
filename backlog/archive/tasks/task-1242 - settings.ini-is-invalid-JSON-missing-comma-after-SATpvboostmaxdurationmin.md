---
id: TASK-1242
title: 'settings.ini is invalid JSON: missing comma after SATpvboostmaxdurationmin'
status: Done
assignee:
  - '@claude'
created_date: '2026-10-17 20:37'
updated_date: '2026-10-17 20:54'
labels:
  - bug
  - settings
dependencies: []
ordinal: 380000
---

## Description

<!-- SECTION:DESCRIPTION:BEGIN -->
Found 2026-10-17 while backing up the Classic-S3 settings (alpha.430): json.load on /settings.ini fails with "Expecting ',' delimiter: line 144". settingStuff.ino:399 writes SATpvboostmaxdurationmin with withComma=false, but the BLE fields were added after it. The firmware reader (parseJsonKVLine) is line-based and ignores commas, so the device loads every setting; the defect only hits anyone who reads the downloaded settings.ini as JSON (backups, tooling). The alpha.427 backup has the same defect.
<!-- SECTION:DESCRIPTION:END -->

## Acceptance Criteria
<!-- AC:BEGIN -->
- [x] #1 json.load of /settings.ini fails on the old firmware and passes on the fix (bench, same settings)
- [x] #2 Every setting survives the round trip: settings via REST before and after are equal
- [x] #3 Exactly one writeJson*KV call per settings writer passes withComma=false, and it is the last one
<!-- AC:END -->

## Implementation Notes

<!-- SECTION:NOTES:BEGIN -->
Evidence 2026-10-17, Classic-S3 COM8/.88.63, esp32-combo. OLD alpha.430+8b39770 with real settings: json.load(settings.ini) -> "Expecting ',' delimiter: line 144" (.tmp/bench-a430-fs/settings.ini; same defect in .tmp/bench-a427-release/settings-before.ini). Strict scan of that file: this is the only defect (211 keys, no dupes, no NaN, no control chars). FIX alpha.431+3f9d4d5: defaults file valid JSON, 211 keys (fix-defaults-combo.ini); old settings.ini restored via /upload + /ReBoot, REST settings 176 vs 176, 0 differences (settings.json vs settings-after.json); forced rewrite (SATpvboostmaxdurationmin 241 -> 240) gives valid JSON, 211 keys, value 240 (fix-real.ini). Guard test tests/test_settings_writer_json.py: FAILED on old code (['SATpvboostmaxdurationmin','WifiDns2']), OK on the fix. Build esp32-classic + esp32-combo green, evaluate --quick 0 FAIL. Side note: first flashed esp32-classic by mistake (bench runs combo; classic writes 179 keys, no OTD/ETH sections); caught before the restore, reflashed combo, nothing lost.
<!-- SECTION:NOTES:END -->

## Final Summary

<!-- SECTION:FINAL_SUMMARY:BEGIN -->
settings.ini is valid JSON again: SATpvboostmaxdurationmin passed withComma=false while the BLE keys follow it. One-word fix plus a guard test that fails when any writer entry but the last omits its comma. Bench old-vs-fix on the Classic-S3: old json.load fails at line 144, fix parses (defaults and real settings), REST settings identical across the round trip (176/176). alpha.431.
<!-- SECTION:FINAL_SUMMARY:END -->
